/**
 * @file bgp-fsm.cc
 * @author Nato Morichika <nat@nat.moe>
 * @brief The BGP Finite State Machine.
 * @version 0.1
 * @date 2019-07-05
 * 
 * @copyright Copyright (c) 2019
 * 
 */
#include "bgp-fsm.h"
#include "realtime-clock.h"
#include "value-op.h"
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>

namespace libbgp {

const char* bgp_fsm_state_str[] = {
    "Idle",
    "Open Sent",
    "Open Confirm",
    "Established",
    "Broken"
};

BgpFsm::BgpFsm(const BgpConfig &config) : in_sink(config.use_4b_asn, BGP_FSM_SINK_SIZE) {
    this->config = config;
    state = IDLE;
    out_buffer = (uint8_t *) malloc(BGP_FSM_BUFFER_SIZE);

    if (config.rev_bus) {
        rev_bus_exist = true;
        config.rev_bus->subscribe(this);
    } else rev_bus_exist = false;

    if (!config.clock) {
        clock = new RealtimeClock();
        clock_local = true;
    } else {
        clock = config.clock;
        clock_local = false;
    }

    if (!config.log_handler) {
        logger = new BgpLogHandler();
        log_local = true;
    } else {
        logger = config.log_handler;
        log_local = false;
    }

    in_sink.setLogger(logger);

    if (!config.rib) {
        rib = new BgpRib(logger);
        rib_local = true;
    } else {
        rib = config.rib;
        rib_local = false;
    }

    hold_timer = 0;
    peer_bgp_id = 0;
    peer_asn = 0;
}

BgpFsm::~BgpFsm() {
    free(out_buffer);
    if (rib_local) delete rib;
    if (clock_local) delete clock;
    if (rev_bus_exist) config.rev_bus->unsubscribe(this);
    if (log_local) delete logger;
}

uint32_t BgpFsm::getAsn() const {
    return (config.asn != 0) ? config.asn : peer_asn;
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

uint16_t BgpFsm::getHoldTimer() const {
    return hold_timer;
}

BgpRib& BgpFsm::getRib() const {
    return rib_local ? *rib : *(config.rib);
}

BgpState BgpFsm::getState() const {
    return state;
}

int BgpFsm::start() {
    if (state == BROKEN) {
        logger->log(ERROR, "BgpFsm::start: FSM is broken, consider reset.\n");
        return 0;
    }

    if (state != IDLE) {
        logger->log(ERROR, "BgpFsm::start: not in IDLE state.\n");
        return 0;
    }
    
    logger->log(INFO, "BgpFsm::start: sending OPEN message to peer.\n");

    uint16_t my_asn_2b = config.asn >= 0xffff ? 23456 : config.asn;

    BgpOpenMessage msg(logger, config.use_4b_asn, my_asn_2b, config.hold_timer, config.router_id);
    if (config.use_4b_asn) {
        msg.setAsn(config.asn);
    }

    setState(OPEN_SENT);
    if(!writeMessage(msg)) return -1;
    return 1;
}

int BgpFsm::stop() {
    if (state == BROKEN) {
        logger->log(ERROR, "BgpFsm::stop: FSM is broken, consider reset.\n");
        return 0;
    }

    if (state == IDLE) return 1;
    if (state != ESTABLISHED) {
        logger->log(ERROR, "BgpFsm::stop: FSM in not ESTABLISED nor IDLE, can't stop. To force stop, do a reset.\n");
        return 0;
    }

    logger->log(INFO, "BgpFsm::stop: de-peering...\n");
    

    BgpNotificationMessage notify (logger, E_CEASE, E_SHUTDOWN, NULL, 0);

    setState(IDLE);
    if(!writeMessage(notify)) return -1;
    return 1;
}

int BgpFsm::run(const uint8_t *buffer, const size_t buffer_size) {
    if (state == BROKEN) {
        logger->log(ERROR, "BgpFsm::run: FSM is broken, consider reset.\n");
        return -1;
    }

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

        if (poured <= -2) {
            logger->log(ERROR, "BgpFsm::run: sink seems to be broken, please reset.\n");
            setState(BROKEN);
            return -1;
        }

        LIBBGP_LOG(logger, DEBUG) {
            logger->log(DEBUG, "BgpFsm::run: got message (Current state: %s):\n", bgp_fsm_state_str[state]);
            logger->log(DEBUG, *packet);
        }

        const BgpMessage *msg = packet->getMessage();
        // parse failed / packet invalid (errors like Unsupported Optional 
        // Parameter falls in this catagory, since those errors are checked by
        // parsers, other errors like FSM error, Bad Peer AS, etc is handled in 
        // fsmEval*)
        if (poured == -1) {
            if (msg->type == NOTIFICATION) {
                logger->log(ERROR, "BgpFsm::run: got invalid NOTIFICATION message.\n");
                if (state == ESTABLISHED) {
                    logger->log(ERROR, "BgpFsm::run: discarding all routes.\n");
                }

                delete packet;
                setState(IDLE);
                return 0;
            }
            BgpNotificationMessage notify (logger, msg->getErrorCode(), msg->getErrorSubCode(), msg->getError(), msg->getErrorLength());
            setState(IDLE);
            if(!writeMessage(notify)) return -1;
            delete packet;
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
            logger->log(ERROR, "BgpFsm::run: got NOTIFICATION: %s (%d): %s (%d).\n", err_msg, notify->errcode, err_sub_msg, notify->subcode);
            delete packet;
            setState(IDLE);
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
                logger->log(ERROR, "BgpFsm::run: FSM in invalid state: %d.\n", state);
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
        logger->log(ERROR, "BgpFsm::tick: peer hold timer timeout.\n");
        BgpNotificationMessage notify (logger, E_HOLD, 0, NULL, 0);
        setState(IDLE);
        if(!writeMessage(notify)) return -1;
        return 0;
    }

    // send keepalive? 
    if (clock->getTime() - last_sent > hold_timer / 3) {
        BgpKeepaliveMessage keep = BgpKeepaliveMessage(logger);
        if(!writeMessage(keep)) return -1;
        return 2;
    }

    return 1;
}

int BgpFsm::resetSoft() {
    BgpNotificationMessage notify (logger, E_CEASE, E_RESET, NULL, 0);
    if(!writeMessage(notify)) return -1;
    resetHard();
    return 0;
}

void BgpFsm::resetHard() {
    in_sink.drain();
    setState(IDLE);
}

int BgpFsm::openRecv(const BgpOpenMessage *open_msg) {
    if (open_msg->version != 4) {
        BgpNotificationMessage notify (logger, E_OPEN, E_VERSION, NULL, 0);
        setState(IDLE);
        if(!writeMessage(notify)) return -1;
        return 0;
    }

    if (config.peer_asn != 0 && open_msg->my_asn != config.peer_asn) {
        BgpNotificationMessage notify (logger, E_OPEN, E_PEER_AS, NULL, 0);
        setState(IDLE);
        if(!writeMessage(notify)) return -1;
        return 0;
    }

    if (!validAddr(open_msg->bgp_id)) {
        LIBBGP_LOG(logger, ERROR) {
            char ip_str[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &(open_msg->bgp_id), ip_str, INET_ADDRSTRLEN);
            logger->log(ERROR, "BgpFsm::openRecv: peer BGP ID (%s) invalid.\n", ip_str);
        }
        BgpNotificationMessage notify (logger, E_OPEN, E_BGP_ID, NULL, 0);
        setState(IDLE);
        if(!writeMessage(notify)) return -1;
        return 0;
    }

    // if hold timer != 0 but < 3, reject witl E_HOLD_TIME (as per rfc4271)
    if (open_msg->hold_time < 3 && open_msg->hold_time != 0) {
        logger->log(ERROR, "BgpFsm::openRecv: invalid hold timer %d.\n", open_msg->hold_time);
        BgpNotificationMessage notify (logger, E_OPEN, E_HOLD_TIME, NULL, 0);
        setState(IDLE);
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
                logger->log(ERROR, 
                    "BgpFsm::openRecv: collision found, and some other FSM feels like they should live"
                    "while we feel like we should live too. is there duplicated FSMs?\n"
                );
                setState(BROKEN);
                return -1;
            }
        }
    }

    hold_timer = config.hold_timer > open_msg->hold_time ? open_msg->hold_time : config.hold_timer;
    peer_bgp_id = open_msg->bgp_id;
    peer_asn = open_msg->my_asn;
    use_4b_asn = open_msg->hasCapability(ASN_4B) && config.use_4b_asn;
    
    return 1;
}

int BgpFsm::resloveCollision(uint32_t peer_bgp_id, bool is_new) {
    if(is_new) {
        if (ntohl(config.router_id) > ntohl(peer_bgp_id)) {
            // this is a new connection, and "we" have higer ID, there's 
            // already a connection so THIS fsm should be dispose. since 
            // THIS fsm is created by peer connecting to us.
            BgpNotificationMessage notify (logger, E_CEASE, E_COLLISION, NULL, 0);
            if(!writeMessage(notify)) return -1;

            setState(IDLE);
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
            BgpNotificationMessage notify (logger, E_CEASE, E_COLLISION, NULL, 0);
            if(!writeMessage(notify)) return -1;

            setState(IDLE);
            return 0;
        }
    }

    // UNREACHED
    setState(BROKEN);
    logger->log(ERROR, "BgpFsm::resloveCollison: ??? :( \n");
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

    LIBBGP_LOG(logger, INFO) {
        logger->log(INFO, "BgpFsm::handleRouteCollisionEvent: detecting collision with %s.\n", inet_ntoa(*(const struct in_addr*) &(ev.peer_bgp_id)));
    }
    return resloveCollision(ev.peer_bgp_id, false) == 1;
}

bool BgpFsm::handleRouteAddEvent(const RouteAddEvent &ev) {
    if (state != ESTABLISHED) return false;

    logger->log(INFO, "BgpFsm::handleRouteAddEvent: got route-add events with %zu routes.\n", ev.routes.size());

    BgpUpdateMessage update (logger, use_4b_asn);
    update.setAttribs(ev.attribs);

    for (const Route &route : ev.routes) {
        if (config.out_filters.apply(route.getPrefix(), route.getLength()) == ACCEPT) {
            update.addNlri(route);
        } else {
            LIBBGP_LOG(logger, INFO) {
                uint32_t prefix = route.getPrefix();
                char ip_str[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &prefix, ip_str, INET_ADDRSTRLEN);
                logger->log(INFO, "BgpFsm::handleRouteAddEvent: route %s/%d filtered by out_filter.\n", ip_str, route.getLength());
            }
        }
    }

    if (update.nlri.size() <= 0) return false;

    prepareUpdateMessage(update);

    if(!writeMessage(update)) return false;
    return true;
}

bool BgpFsm::handleRouteWithdrawEvent(const RouteWithdrawEvent &ev) {
    if (state != ESTABLISHED) return false;

    logger->log(INFO, "BgpFsm::handleRouteAddEvent: got route-withdraw events with %zu routes.\n", ev.routes.size());


    BgpUpdateMessage withdraw (logger, use_4b_asn);
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
                logger->log(ERROR, "BgpFsm::validateState: got non-OPEN message in IDLE state.\n");
                return 0;
            }
            return 1;
        case OPEN_SENT:
            if (type != OPEN) {
                logger->log(ERROR, "BgpFsm::validateState: got non-OPEN message in OPEN_SENT state.\n");
                BgpNotificationMessage notify (logger, E_FSM, E_OPEN_SENT, NULL, 0);
                setState(IDLE);
                if(!writeMessage(notify)) return -1;

                return 0;
            }
            return 1;
        case OPEN_CONFIRM:
            if (type != KEEPALIVE) {
                logger->log(ERROR, "BgpFsm::validateState: got non-KEEPALIVE message in OPEN_CONFIRM state.\n");
                BgpNotificationMessage notify (logger, E_FSM, E_OPEN_CONFIRM, NULL, 0);
                setState(IDLE);
                if(!writeMessage(notify)) return -1;

                return 0;
            }
            return 1;
        case ESTABLISHED:
            if (type != UPDATE && type != KEEPALIVE) {
                logger->log(ERROR, "BgpFsm::validateState: got invalid message (type %d) in ESTABLISHED state.\n", type);
                BgpNotificationMessage notify (logger, E_FSM, E_ESTABLISHED, NULL, 0);
                setState(IDLE);
                if(!writeMessage(notify)) return -1;

                return 0;
            }
            return 1;
        default:
            logger->log(ERROR, "BgpFsm::validateState: got message in bad state. consider reset.\n");
            return -1;
    }
}

int BgpFsm::fsmEvalIdle(const BgpMessage *msg) {
    const BgpOpenMessage *open_msg = dynamic_cast<const BgpOpenMessage *>(msg);

    int retval = openRecv(open_msg);
    if (retval != 1) return retval;

    uint16_t my_asn_2b = config.asn >= 0xffff ? 23456 : config.asn;
    BgpOpenMessage open_reply (logger, use_4b_asn, my_asn_2b, hold_timer, config.router_id);
    if (use_4b_asn) {
        open_reply.setAsn(config.asn);
    }

    setState(OPEN_CONFIRM);
    if(!writeMessage(open_reply)) return -1;

    return 1;
}

int BgpFsm::fsmEvalOpenSent(const BgpMessage *msg) {
    const BgpOpenMessage *open_msg = dynamic_cast<const BgpOpenMessage *>(msg);

    int retval = openRecv(open_msg);
    if (retval != 1) return retval;

    BgpKeepaliveMessage keep = BgpKeepaliveMessage(logger);
    setState(OPEN_CONFIRM);
    if(!writeMessage(keep)) return -1;

    return 1;
}

int BgpFsm::fsmEvalOpenConfirm(__attribute__((unused)) const BgpMessage *msg) {
    BgpKeepaliveMessage keep = BgpKeepaliveMessage(logger);
    setState(ESTABLISHED);
    if(!writeMessage(keep)) return -1;

    std::vector<BgpRibEntry>::const_iterator iter = rib->get().begin();
    const std::vector<BgpRibEntry>::const_iterator end = rib->get().end();

    // FIXME: update routes grouping: don't exceed max size of the buffer.
    while (iter != end) {
        uint64_t cur_group_id = iter->update_id;
        BgpUpdateMessage update (logger, use_4b_asn);
        update.setAttribs(iter->attribs);

        for (; cur_group_id == iter->update_id && iter != end; iter++) {
            const Route &r = iter->route;
            if (config.out_filters.apply(r.getPrefix(), r.getLength()) == ACCEPT) {
                update.addNlri(r);
            } else {
                LIBBGP_LOG(logger, INFO) {
                    uint32_t prefix = r.getPrefix();
                    char ip_str[INET_ADDRSTRLEN];
                    inet_ntop(AF_INET, &prefix, ip_str, INET_ADDRSTRLEN);
                    logger->log(INFO, "BgpFsm::fsmEvalOpenConfirm: route %s/%d filtered by out_filter.\n", ip_str, r.getLength());
                }
            }
        }

        if (update.nlri.size() > 0) {
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

    if (update->nlri.size() > 0) {
        const BgpPathAttribNexthop &nh = dynamic_cast<const BgpPathAttribNexthop &>(update->getAttrib(NEXT_HOP));

        if (!validAddr(nh.next_hop)) {
            LIBBGP_LOG(logger, WARN) {
                char ip_str[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &(nh.next_hop), ip_str, INET_ADDRSTRLEN);
                logger->log(WARN, "BgpFsm::fsmEvalEstablished: ignored %zu routes with invalid nexthop %s\n", update->nlri.size(), ip_str);
            }
            return 1;
        }
    
        if (!config.no_nexthop_check && !Route::Includes(config.peering_lan_prefix, config.peering_lan_length, nh.next_hop)) {
            LIBBGP_LOG(logger, WARN) {
                char ip_str_nh[INET_ADDRSTRLEN];
                char ip_str_lan[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &(nh.next_hop), ip_str_nh, INET_ADDRSTRLEN);
                inet_ntop(AF_INET, &(config.peering_lan_prefix), ip_str_lan, INET_ADDRSTRLEN);
                logger->log(WARN, "BgpFsm::fsmEvalEstablished: ignored %zu routes with nexthop outside peering LAN. (%s not in %s/%d)\n", 
                    update->nlri.size(), ip_str_nh, ip_str_lan, config.peering_lan_length);
            }
            return 1;
        };
    }
    

    std::vector<Route> routes = std::vector<Route> ();
    for (const Route &route : update->nlri) {
        if(config.in_filters.apply(route.getPrefix(), route.getLength()) == ACCEPT) {
            routes.push_back(route);
        } else {
            LIBBGP_LOG(logger, INFO) {
                uint32_t prefix = route.getPrefix();
                char ip_str[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &prefix, ip_str, INET_ADDRSTRLEN);
                logger->log(INFO, "BgpFsm::fsmEvalEstablished: route %s/%d filtered by in_filters.\n", ip_str, route.getLength());
            }
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

void BgpFsm::dropAllRoutes() {
    if (peer_bgp_id != 0) {
        std::vector<Route> dropped_routes = rib->discard(peer_bgp_id);
        if (rev_bus_exist && dropped_routes.size() > 0) {
            RouteWithdrawEvent wev;
            wev.routes = dropped_routes;
            config.rev_bus->publish(this, wev);
        }
    }
}

void BgpFsm::setState(BgpState new_state) {
    if (state == new_state) return;

    logger->log(INFO, "BgpFsm::setState: changing state: %s -> %s\n", bgp_fsm_state_str[state], bgp_fsm_state_str[new_state]); 

    if (state == ESTABLISHED) {
        logger->log(INFO, "BgpFsm::setState: dropping all routes received from peer...\n");

        // moved from ESTABLISHED to something else. Drop all routes.
        dropAllRoutes();
    }

    state = new_state;
}

bool BgpFsm::validAddr(uint32_t addr) const {
    if (addr == config.nexthop || addr == config.router_id) {
        return false;
    }

    uint32_t addr_host = ntohl(addr);
    uint32_t first = addr_host >> 24;

    if (first == 0 || first == 127 || (first >= 224 && first <= 239) || first > 240) {
        return false;
    }

    return true;
}

bool BgpFsm::writeMessage(const BgpMessage &msg) {
    BgpPacket pkt(logger, use_4b_asn, &msg);
    LIBBGP_LOG(logger, DEBUG) {
        logger->log(DEBUG, "BgpFsm::writeMessage: write (Current state: %s):\n", bgp_fsm_state_str[state]);
        logger->log(DEBUG, pkt);
    }

    std::lock_guard<std::recursive_mutex> lock(out_buffer_mutex);

    ssize_t pkt_len = pkt.write(out_buffer, BGP_FSM_BUFFER_SIZE);
    last_sent = clock->getTime();

    if (pkt_len < 0) {
        logger->log(ERROR, "BgpFsm::writeMessage: failed to write message, abort.\n");
        setState(BROKEN);
        return false;
    }

    if (config.out_handler && !config.out_handler->handleOut(out_buffer, pkt_len)) {
        logger->log(ERROR, "BgpFsm::writeMessage: out_handler failed, abort.\n");
        setState(BROKEN);
        return false;
    }

    return true;
}

}