#include "bgp-fsm.h"
#include "bgp-error.h"
#include "realtime-clock.h"
#include "value-op.h"
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>

namespace libbgp {

BgpFsm::BgpFsm(const BgpConfig &config) : in_sink(config.use_4b_asn, BGP_FSM_SINK_SIZE) {
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

    uint16_t my_asn_2b = config.asn >= 0xffff ? 23456 : config.asn;

    BgpOpenMessage msg(use_4b_asn, my_asn_2b, config.hold_timer, config.router_id);
    if (use_4b_asn) {
        msg.setAsn(config.asn);
    }
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

    // since we use pourPtr, we need to make sure no fill() happens
    std::lock_guard<std::mutex> lock(in_sink_mutex);
    in_sink.fill(buffer, buffer_size);

    // tick the clock
    int tick_ret = tick();
    if (tick_ret <= 0) return tick_ret;
    last_recv = clock->getTime();

    int final_ret_val = -1;

    // keep running untill sink empty
    while (in_sink.getBytesInSink() > 0) {
        BgpPacket *packet = NULL;
        ssize_t poured = in_sink.pour(&packet);

        if (poured <= -2) return poured;

        const BgpMessage *msg = packet->getMessage();
        // parse failed / packet invalid (errors like Unsupported Optional 
        // Parameter falls in this catagory, since those errors are checked by
        // parsers, other errors like FSM error, Bad Peer AS, etc is handled in 
        // fsmEval*)
        if (poured == -1) {
            if (msg->type == NOTIFICATION) {
                _bgp_error("BgpFsm::run: got invalid NOTIFICATION message.\n");
                delete packet;
                state = IDLE;
                return 0;
            }
            BgpNotificationMessage notify (msg->getErrorCode(), msg->getErrorSubCode(), msg->getError(), msg->getErrorLength());
            if(!writeMessage(notify)) return -1;
            delete packet;
            state = IDLE;
            return 0;
        }

        if (msg->type == NOTIFICATION) {
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
            delete packet;
            state = IDLE;
            return 0;
        }

        int retval = -1;

        int vald_ret = validateState(msg->type);
        if (vald_ret <= 0) {
            delete packet;
            return vald_ret;
        }

        switch (state) {
            case IDLE: retval = fsmEvalIdle(msg); break;
            case OPEN_SENT: retval = fsmEvalOpenSent(msg); break;
            case OPEN_CONFIRM: retval = fsmEvalOpenConfirm(msg); break;
            case ESTABLISHED: retval = fsmEvalEstablished(msg); break;
            default: {
                _bgp_error("BgpFsm::run: FSM in invalid state: %d.\n", state);
                delete packet;
                return -1;
            }
        }

        delete packet;
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

int BgpFsm::resetSoft() {
    BgpNotificationMessage notify (E_CEASE, E_RESET, NULL, 0);
    if(!writeMessage(notify)) return -1;
    resetHard();
    return 0;
}

void BgpFsm::resetHard() {
    in_sink.drain();
    state = IDLE;
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

    if (!config.no_collision_detection && rev_bus_exist) {
        RouteCollisionEvent col = RouteCollisionEvent();
        col.peer_bgp_id = open_msg->bgp_id;

        // publish returned 0: no one complained about collision (either 
        // there's no collision, or some fsm has killed themselves)
        //
        // publish returned > 0: someone complained about collision, it think 
        // this session should be dropped
        if(config.rev_bus->publish(this, col) > 0) {
            int res_result = resloveCollision(open_msg->bgp_id, true);

            if(res_result == -1) return -1;
            if(res_result == 0) return 0;

            // should not be happening
            if(res_result == 1) {
                _bgp_error(
                    "BgpFsm::openRecv: collision found, and some other FSM feels like they should live"
                    "while we feel like we should live too. is there duplicated FSMs?\n"
                );
                state = BROKEN;
                return -1;
            }
        }
    }

    hold_timer = config.hold_timer > open_msg->hold_time ? open_msg->hold_time : config.hold_timer;
    use_4b_asn = open_msg->hasCapability(ASN_4B) && config.use_4b_asn;
    
    return 1;
}

int BgpFsm::resloveCollision(uint32_t peer_bgp_id, bool is_new) {
    if(is_new) {
        if (ntohl(config.router_id) > ntohl(peer_bgp_id)) {
            // this is a new connection, and "we" have higer ID, there's 
            // already a connection so THIS fsm should be dispose. since 
            // THIS fsm is created by peer connecting to us.
            BgpNotificationMessage notify (E_CEASE, E_COLLISION, NULL, 0);
            if(!writeMessage(notify)) return -1;

            state = IDLE;
            return 0;
        } else {
            // this is a new connection, and peer has higher ID. the exisiting
            // connection should be dispose, since THIS fsm is created by peer 
            // connecting to us, this one will be kept, we do nothing.
            return 1;
        }
    } else {
        if (config.router_id > peer_bgp_id) {
            // this is a old connection, and "we" have higer ID, the new one
            // shoud close, we do nothing.

            return 1;
        } else {
            // this is a old connection, and "peer" have higer ID, this one
            // shoud close.
            BgpNotificationMessage notify (E_CEASE, E_COLLISION, NULL, 0);
            if(!writeMessage(notify)) return -1;

            state = IDLE;
            return 0;
        }
    }

    // UNREACHED
    state = BROKEN;
    _bgp_error("BgpFsm::resloveCollison: ??? :( \n");
    return -1;
}

bool BgpFsm::handleRouteEvent(const RouteEvent &ev) {
    if (ev.type == ADD) return handleRouteAddEvent(dynamic_cast <const RouteAddEvent&>(ev));
    if (ev.type == WITHDRAW) return handleRouteWithdrawEvent(dynamic_cast <const RouteWithdrawEvent&>(ev));
    if (ev.type == COLLISION) return handleRouteCollisionEvent(dynamic_cast <const RouteCollisionEvent&>(ev));

    return false;
}

bool BgpFsm::handleRouteCollisionEvent(const RouteCollisionEvent &ev) {
    if (state != OPEN_CONFIRM) return false;
    return resloveCollision(ev.peer_bgp_id, false) == 1;
}

bool BgpFsm::handleRouteAddEvent(const RouteAddEvent &ev) {
    if (state != ESTABLISHED) return false;

    BgpUpdateMessage update (use_4b_asn);
    update.setAttribs(ev.attribs);

    for (const Route &route : ev.routes) {
        if (config.out_filters.apply(route.getPrefix(), route.getLength()) == ACCEPT) {
            update.addNlri(route);
        } 
    }

    if (update.nlri.size() <= 0) return false;

    prepareUpdateMessage(update);

    if(!writeMessage(update)) return false;
    return true;
}

bool BgpFsm::handleRouteWithdrawEvent(const RouteWithdrawEvent &ev) {
    if (state != ESTABLISHED) return false;

    BgpUpdateMessage withdraw (use_4b_asn);
    withdraw.setWithdrawn(ev.routes);

    if(!writeMessage(withdraw)) return false;
    return true;
}

void BgpFsm::prepareUpdateMessage(BgpUpdateMessage &update) {
    update.dropNonTransitive();
    if (config.forced_default_nexthop || !update.hasAttrib(NEXT_HOP)) {
        update.setNextHop(config.nexthop);
    } else {
        BgpPathAttribNexthop &nh = dynamic_cast<BgpPathAttribNexthop &> (update.getAttrib(NEXT_HOP));
        if (!Route::Includes(config.peering_lan_prefix, config.peering_lan_length, nh.next_hop)) {
            nh.next_hop = config.nexthop;
        }
    }

    if (config.use_4b_asn && use_4b_asn) {                
        update.restoreAsPath();
        update.restoreAggregator();
        update.prepend(config.asn);
    } else {
        update.downgradeAsPath();
        update.downgradeAggregator();
        update.prepend(config.asn);
    }    
}

int BgpFsm::validateState(uint8_t type) {
    switch(state) {
        case IDLE:
            if (type != OPEN) {
                _bgp_error("BgpFsm::validateState: got non-OPEN message in IDLE state.\n");
                return 0;
            }
            return 1;
        case OPEN_SENT:
            if (type != OPEN) {
                _bgp_error("BgpFsm::validateState: got non-OPEN message in OPEN_SENT state.\n");
                BgpNotificationMessage notify (E_FSM, E_OPEN_SENT, NULL, 0);
                if(!writeMessage(notify)) return -1;

                state = IDLE;
                return 0;
            }
            return 1;
        case OPEN_CONFIRM:
            if (type != KEEPALIVE) {
                _bgp_error("BgpFsm::validateState: got non-KEEPALIVE message in OPEN_CONFIRM state.\n");
                BgpNotificationMessage notify (E_FSM, E_OPEN_CONFIRM, NULL, 0);
                if(!writeMessage(notify)) return -1;

                state = IDLE;
                return 0;
            }
            return 1;
        case ESTABLISHED:
            if (type != UPDATE) {
                _bgp_error("BgpFsm::validateState: got invalid message in ESTABLISHED state.\n");
                BgpNotificationMessage notify (E_FSM, E_ESTABLISHED, NULL, 0);
                if(!writeMessage(notify)) return -1;

                state = IDLE;
                return 0;
            }
            return 1;
        default:
            _bgp_error("BgpFsm::validateState: got message in bad state. consider reset.\n");
            return -1;
    }
}

int BgpFsm::fsmEvalIdle(const BgpMessage *msg) {
    const BgpOpenMessage *open_msg = dynamic_cast<const BgpOpenMessage *>(msg);

    int retval = openRecv(open_msg);
    if (retval != 1) return retval;

    uint16_t my_asn_2b = config.asn >= 0xffff ? 23456 : config.asn;
    BgpOpenMessage open_reply (use_4b_asn, my_asn_2b, hold_timer, config.router_id);
    if (use_4b_asn) {
        open_reply.setAsn(config.asn);
    }
    if(!writeMessage(open_reply)) return -1;

    state = OPEN_CONFIRM;
    return 1;
}

int BgpFsm::fsmEvalOpenSent(const BgpMessage *msg) {
    const BgpOpenMessage *open_msg = dynamic_cast<const BgpOpenMessage *>(msg);

    int retval = openRecv(open_msg);
    if (retval != 1) return retval;

    BgpKeepaliveMessage keep = BgpKeepaliveMessage();
    if(!writeMessage(keep)) return -1;

    state = OPEN_CONFIRM;
    return 1;
}

int BgpFsm::fsmEvalOpenConfirm(__attribute__((unused)) const BgpMessage *msg) {
    BgpKeepaliveMessage keep = BgpKeepaliveMessage();
    if(!writeMessage(keep)) return -1;

    state = ESTABLISHED;

    // feed rib to peer; TODO: feed routes w/ same attrib w/ single message
    for (const BgpRibEntry &entry : rib->get()) {
        const Route route = entry.route;
        if (config.out_filters.apply(route.getPrefix(), route.getLength()) == ACCEPT) {
            BgpUpdateMessage update (use_4b_asn);
            update.setAttribs(entry.attribs);
            update.addNlri(route);
            prepareUpdateMessage(update);
            if(!writeMessage(update)) return -1;
        }
    }

    return 1;
}

int BgpFsm::fsmEvalEstablished(const BgpMessage *msg) {
    if (msg->type == KEEPALIVE) return 1;

    const BgpUpdateMessage *update = dynamic_cast<const BgpUpdateMessage *>(msg);

    for (const Route &route : update->withdrawn_routes) {
        rib->withdraw(peer_bgp_id, route);
    }

    const BgpPathAttribNexthop &nh = dynamic_cast<const BgpPathAttribNexthop &>(update->getAttrib(NEXT_HOP));
    
    if (!Route::Includes(config.peering_lan_prefix, config.peering_lan_length, nh.next_hop)) {
        // ignore invalid nexthop
        return 1;
    };

    std::vector<Route> routes = std::vector<Route> ();
    for (const Route &route : update->nlri) {
        if(config.in_filters.apply(route.getPrefix(), route.getLength()) == ACCEPT) {
            routes.push_back(route);
        }
    }

    if (routes.size() > 0) {
        rib->insert(peer_bgp_id, routes, update->path_attribute);
    }

    if (rev_bus_exist) {
        if (update->withdrawn_routes.size() > 0) {
            RouteWithdrawEvent wev = RouteWithdrawEvent();
            wev.routes = update->withdrawn_routes;
            config.rev_bus->publish(this, wev);
        }

        if (routes.size() > 0) {
            RouteAddEvent aev = RouteAddEvent();
            aev.routes = routes;
            aev.attribs = update->path_attribute;
            config.rev_bus->publish(this, aev);
        }
    }

    return 1;
}

bool BgpFsm::writeMessage(const BgpMessage &msg) {
    std::lock_guard<std::mutex> lock(out_buffer_mutex);
    BgpPacket pkt(use_4b_asn, &msg);
    ssize_t pkt_len = pkt.write(out_buffer, BGP_FSM_BUFFER_SIZE);

    if (pkt_len < 0) {
        _bgp_error("BgpFsm::writeMessage: failed to write message, abort.\n");
        state = BROKEN;
        return false;
    }

    if (config.out_handler && !config.out_handler->handleOut(out_buffer, pkt_len)) {
        _bgp_error("BgpFsm::writeMessage: out_handler failed, abort.\n");
        state = BROKEN;
        return false;
    }

    last_sent = clock->getTime();
    return true;
}

}