#include "bgp-fsm.h"
#include "bgp-error.h"
#include "protocol/bgp.h"
#include "protocol/value-op.h"
#include <stdlib.h>

namespace bgpfsm {

BgpFsm::BgpFsm(BgpConfig config) : in_sink(BGP_FSM_SINK_SIZE) {
    this->config = config;
    state = IDLE;
    out_buffer = (uint8_t *) malloc(BGP_FSM_BUFFER_SIZE);
}

BgpFsm::~BgpFsm() {
    free(out_buffer);
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
    if (state != IDLE) {
        _bgp_error("BgpFsm::start: not in IDLE state.\n");
        return -1;
    }

    BgpOpenMessage msg(config.asn, config.hold_timer, config.router_id);
    ssize_t len = msg.write(out_buffer, BGP_FSM_BUFFER_SIZE);
    if(!writeMessage(out_buffer, len)) return -1;

    state = OPEN_SENT;
    return 1;
}

int BgpFsm::stop() {
    if (state == IDLE) return 1;
    if (state != ESTABLISHED) {
        _bgp_error("BgpFsm::stop: FSM in not ESTABLISED nor IDLE, can't stop.\n");
        return -1;
    }

    BgpNotificationMessage msg(E_CEASE, E_SHUTDOWN, NULL, 0);
    ssize_t len = msg.write(out_buffer, BGP_FSM_BUFFER_SIZE);
    if(!writeMessage(out_buffer, len)) return -1;

    state = IDLE;
    return 1;
}

int BgpFsm::run(const uint8_t *buffer, const size_t buffer_size) {
    in_sink.fill(buffer, buffer_size);
    BufferPtr packet = in_sink.pourPtr();
    const uint8_t *packet_ptr = packet.buffer + 18;
    uint8_t message_type = getValue<uint8_t> (&packet_ptr);
    BgpMessage *msg;

    switch (message_type) {
        case OPEN: msg = new BgpOpenMessage(); break;
        case UPDATE: msg = new BgpUpdateMessage(); break;
        case KEEPALIVE: msg = new BgpKeepaliveMessage(); break;
        case NOTIFICATION: msg = new BgpNotificationMessage(); break;
        default: {
            // unknow message type
            BgpNotificationMessage notify (E_HEADER, E_TYPE, NULL, 0);
            ssize_t len = notify.write(out_buffer, BGP_FSM_BUFFER_SIZE);
            if(!writeMessage(out_buffer, len)) return -1;
            return 0;
        }
    }

    size_t msg_len = buffer_size - 19;
    ssize_t parsed_len = msg->parse(packet_ptr, msg_len);

    if (msg_len != parsed_len) {
        _bgp_error("BgpFsm::run: parsed length (%d) != message length (%d).\n", parsed_len, msg_len);
        delete msg;
        return -1;
    }

    int retval = -1;

    switch (state) {
        case IDLE: retval = fsmEvalIdle(msg); break;
        case OPEN_SENT: retval = fsmEvalOpenSent(msg); break;
        case OPEN_CONFIRM: retval = fsmEvalOpenConfirm(msg); break;
        case ESTABLISHED: retval = fsmEvalOpenConfirm(msg); break;
        default: {
            _bgp_error("BgpFsm::run: FSM in invalid state: %d.\n", state);
            delete msg;
            return -1;
        }
    }

    delete msg;
    return retval;
}

bool BgpFsm::writeMessage(const uint8_t *buffer, ssize_t len) {
    if (len < 0) {
        _bgp_error("BgpFsm::writeMessage: failed to write message, abort.\n");
        return false;
    }

    if (config.out_handler && !config.out_handler->handleOut(out_buffer, len)) {
        _bgp_error("BgpFsm::writeMessage: out_handler failed, abort.\n");
        return false;
    }

    return true;
}

}