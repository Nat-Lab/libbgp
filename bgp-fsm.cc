#include "bgp-fsm.h"
#include "bgp-error.h"
#include "realtime-clock.h"
#include "protocol/bgp.h"
#include "protocol/value-op.h"
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>

namespace bgpfsm {

BgpFsm::BgpFsm(const BgpConfig &config) : in_sink(BGP_FSM_SINK_SIZE) {
    this->config = config;
    state = IDLE;
    out_buffer = (uint8_t *) malloc(BGP_FSM_BUFFER_SIZE);
    
    if (!config.rib) {
        rib = new BgpRib();
        rib_local = true;
    } else rib = config.rib;

    if (config.rev_bus) {
        rev_bus_exist = true;
        config.rev_bus->subscribe(this);
    }

    if (!config.clock) {
        clock = new RealtimeClock();
        clock_local = true;
    } else clock = config.clock;
}

BgpFsm::~BgpFsm() {
    free(out_buffer);
    if (rib_local) delete rib;
    if (clock_local) delete clock;
    if (rev_bus_exist) config.rev_bus->unsubscribe(this);
}

uint32_t BgpFsm::getAsn() const {
    return config.asn;
}

uint32_t BgpFsm::getBgpId() const {
    return config.router_id;
}

uint32_t BgpFsm::getPeerAsn() const {
    return config.peer_asn;
}

uint32_t BgpFsm::getPeerBgpId() const {
    return peer_bgp_id;
}

const BgpRib& BgpFsm::getRib() const {
    return *(config.rib);
}

BgpState BgpFsm::getState() const {
    return state;
}

int BgpFsm::start() {
    if (state == BROKEN) {
        _bgp_error("BgpFsm::start: FSM is broken, consider reset.\n");
        return 0;
    }

    if (state != IDLE) {
        _bgp_error("BgpFsm::start: not in IDLE state.\n");
        return 0;
    }

    BgpOpenMessage msg(config.asn, config.hold_timer, config.router_id);
    if(!writeMessage(msg)) return -1;

    state = OPEN_SENT;
    return 1;
}

int BgpFsm::stop() {
    if (state == BROKEN) {
        _bgp_error("BgpFsm::stop: FSM is broken, consider reset.\n");
        return 0;
    }

    if (state == IDLE) return 1;
    if (state != ESTABLISHED) {
        _bgp_error("BgpFsm::stop: FSM in not ESTABLISED nor IDLE, can't stop.\n");
        return 0;
    }

    BgpNotificationMessage notify (E_CEASE, E_SHUTDOWN, NULL, 0);
    if(!writeMessage(notify)) return -1;

    state = IDLE;
    return 1;
}

int BgpFsm::run(const uint8_t *buffer, const size_t buffer_size) {
    if (state == BROKEN) {
        _bgp_error("BgpFsm::run: FSM is broken, consider reset.\n");
        return -1;
    }

    in_sink.fill(buffer, buffer_size);

    int final_ret_val = -1;

    // keep running untill sink empty
    while (in_sink.getBytesInSink() > 0) {
        BufferPtr packet = in_sink.pourPtr();
        if (packet.buffer_size == 0) return 3;

        const uint8_t *packet_ptr = packet.buffer + 18;
        uint8_t message_type = getValue<uint8_t> (&packet_ptr);
        BgpMessage *msg;

        // create message container
        switch (message_type) {
            case OPEN: msg = new BgpOpenMessage(); break;
            case UPDATE: msg = new BgpUpdateMessage(); break;
            case KEEPALIVE: msg = new BgpKeepaliveMessage(); break;
            case NOTIFICATION: msg = new BgpNotificationMessage(); break;
            default: {
                // unknow message type
                BgpNotificationMessage notify (E_HEADER, E_TYPE, NULL, 0);
                if(!writeMessage(notify)) return -1;
                return 0;
            }
        }

        // parse the message
        size_t msg_len = buffer_size - 19;
        ssize_t parsed_len = msg->parse(packet_ptr, msg_len);

        // parse failed / packet invalid (errors like Unsupported Optional 
        // Parameter falls in this catagory, since those errors are checked by
        // parsers, other errors like FSM error, Bad Peer AS, etc is handled in 
        // fsmEval*)
        if (parsed_len < 0) {
            if (message_type == NOTIFICATION) {
                _bgp_error("BgpFsm::run: got invalid NOTIFICATION message.\n");
                delete msg;
                state = IDLE;
                return 0;
            }

            BgpNotificationMessage notify (msg->getErrorCode(), msg->getErrorSubCode(), msg->getError(), msg->getErrorLength());
            if(!writeMessage(notify)) return -1;
            delete msg;
            state = IDLE;
            return 0;
        }

        if (msg_len != parsed_len) {
            _bgp_error("BgpFsm::run: parsed length (%d) != message length (%d).\n", parsed_len, msg_len);
            state = BROKEN;
            delete msg;
            return -1;
        }

        if (message_type == NOTIFICATION) {
            const BgpNotificationMessage *notify = dynamic_cast<const BgpNotificationMessage *>(msg);
            const char *err_msg = bgp_error_code_str[notify->errcode];
            const char *err_sub_msg = bgp_error_code_str[0];
            switch (notify->errcode) {
                case E_HEADER: err_sub_msg = bgp_header_error_subcode_str[notify->subcode]; break;
                case E_OPEN: err_sub_msg = bgp_open_error_subcode_str[notify->subcode]; break;
                case E_UPDATE: err_sub_msg = bgp_update_error_str[notify->subcode]; break;
                case E_FSM: err_sub_msg = bgp_fsm_error_str[notify->subcode]; break;
                case E_CEASE: err_sub_msg = bgp_cease_error_str[notify->subcode]; break;
            }
            _bgp_error("BgpFsm::run: got NOTIFICATION: %s: %s.\n", err_msg, err_sub_msg);
            delete msg;
            state = IDLE;
            return 0;
        };

        int retval = -1;

        switch (state) {
            case IDLE: retval = fsmEvalIdle(msg); break;
            case OPEN_SENT: retval = fsmEvalOpenSent(msg); break;
            case OPEN_CONFIRM: retval = fsmEvalOpenConfirm(msg); break;
            case ESTABLISHED: retval = fsmEvalEstablished(msg); break;
            default: {
                _bgp_error("BgpFsm::run: FSM in invalid state: %d.\n", state);
                delete msg;
                return -1;
            }
        }

        delete msg;
        if (retval < 0) return retval;
        if (retval == 0) final_ret_val = 0;
        if (retval == 1 && final_ret_val != 0 && final_ret_val != 2) final_ret_val = 1;
        if (retval == 2) final_ret_val = 2;
    }

    return final_ret_val;
}

int BgpFsm::tick() {
    if (state != ESTABLISHED) return 1;

    // peer hold-timer exipred?
    if (clock->getTime() - last_recv > hold_timer) {
        _bgp_error("BgpFsm::tick: peer hold timer timeout.\n");
        BgpNotificationMessage notify (E_HOLD, 0, NULL, 0);
        if(!writeMessage(notify)) return -1;
        state = IDLE;
        return 0;
    }

    // send keepalive? 
    if (clock->getTime() - last_sent > hold_timer / 3) {
        BgpKeepaliveMessage keep = BgpKeepaliveMessage();
        if(!writeMessage(keep)) return -1;
        return 2;
    }

    return 1;
}

int BgpFsm::openRecv(const BgpOpenMessage *open_msg) {
    if (open_msg->version != 4) {
        BgpNotificationMessage notify (E_OPEN, E_VERSION, NULL, 0);
        if(!writeMessage(notify)) return -1;
        return 0;
    }

    if (open_msg->my_asn != config.peer_asn) {
        BgpNotificationMessage notify (E_OPEN, E_PEER_AS, NULL, 0);
        if(!writeMessage(notify)) return -1;
        return 0;
    }

    // if hold timer != 0 but < 3, reject witl E_HOLD_TIME (as per rfc4271)
    if (open_msg->hold_time < 3 && open_msg->hold_time != 0) {
        _bgp_error("BgpFsm::openRecv: invalid hold timer %d.\n", open_msg->hold_time);
        BgpNotificationMessage notify (E_OPEN, E_HOLD_TIME, NULL, 0);
        if(!writeMessage(notify)) return -1;
        return 0;
    }

    hold_timer = config.hold_timer > open_msg->hold_time ? open_msg->hold_time : config.hold_timer;
    use_4b_asn = open_msg->use_4b_asn && config.use_4b_asn;
    
    return 1;
}

int BgpFsm::fsmEvalIdle(const BgpMessage *msg) {
    if (msg->type != OPEN) {
        _bgp_error("BgpFsm::fsmEvalIdle: got non-OPEN message in IDLE state.\n");
        return 0;
    }

    const BgpOpenMessage *open_msg = dynamic_cast<const BgpOpenMessage *>(msg);

    int retval = openRecv(open_msg);
    if (retval != 1) return retval;

    BgpOpenMessage open_reply (config.asn, hold_timer, config.router_id);
    open_reply.use_4b_asn = use_4b_asn;
    if(!writeMessage(open_reply)) return -1;

    state = OPEN_CONFIRM;
    return 1;
}

int BgpFsm::fsmEvalOpenSent(const BgpMessage *msg) {
    if (msg->type != OPEN) {
        _bgp_error("BgpFsm::fsmEvalOpenSent: got non-OPEN message in OPEN_SENT state.\n");
        BgpNotificationMessage notify (E_FSM, E_OPEN_SENT, NULL, 0);
        if(!writeMessage(notify)) return -1;

        state = IDLE;
        return 0;
    }

    const BgpOpenMessage *open_msg = dynamic_cast<const BgpOpenMessage *>(msg);

    int retval = openRecv(open_msg);
    if (retval != 1) return retval;

    BgpKeepaliveMessage keep = BgpKeepaliveMessage();
    if(!writeMessage(keep)) return -1;

    state = OPEN_CONFIRM;
    return 1;
}

int BgpFsm::fsmEvalOpenConfirm(const BgpMessage *msg) {
    if (msg->type != KEEPALIVE) {
        _bgp_error("BgpFsm::fsmEvalOpenConfirm: got non-KEEPALIVE message in OPEN_CONFIRM state.\n");
        BgpNotificationMessage notify (E_FSM, E_OPEN_CONFIRM, NULL, 0);
        if(!writeMessage(notify)) return -1;

        state = IDLE;
        return 0;
    }

    BgpKeepaliveMessage keep = BgpKeepaliveMessage();
    if(!writeMessage(keep)) return -1;

    state = ESTABLISHED;
    return 1;
}

bool BgpFsm::writeMessage(const BgpMessage &msg) {
    std::lock_guard<std::mutex> lock(out_buffer_mutex);
    ssize_t len = msg.write(out_buffer + 19, BGP_FSM_BUFFER_SIZE - 19);

    if (len < 0) {
        _bgp_error("BgpFsm::writeMessage: failed to write message, abort.\n");
        state = BROKEN;
        return false;
    }

    size_t pkt_len = (size_t) len + 19;
    memset(out_buffer, '\xff', 16);
    putValue<uint16_t> (&out_buffer, htons(pkt_len));
    putValue<uint8_t> (&out_buffer, msg.type);

    if (config.out_handler && !config.out_handler->handleOut(out_buffer, pkt_len)) {
        _bgp_error("BgpFsm::writeMessage: out_handler failed, abort.\n");
        state = BROKEN;
        return false;
    }

    return true;
}

}