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

BgpFsm::BgpFsm(const BgpConfig &config) : in_sink(config.use_4b_asn) {
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

    if (!config.rib4) {
        rib4 = new BgpRib4(logger);
        rib4_local = true;
    } else {
        rib4 = config.rib4;
        rib4_local = false;
    }

    if (!config.rib6) {
        rib6 = new BgpRib6(logger);
        rib6_local = true;
    } else {
        rib6 = config.rib6;
        rib6_local = false;
    }

    hold_timer = 0;
    peer_bgp_id = 0;
    peer_asn = 0;
}

BgpFsm::~BgpFsm() {
    free(out_buffer);
    if (rib4_local) delete rib4;
    if (rib6_local) delete rib6;
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
    return peer_asn;
}

uint32_t BgpFsm::getPeerBgpId() const {
    return peer_bgp_id;
}

uint16_t BgpFsm::getHoldTimer() const {
    return hold_timer;
}

BgpRib4& BgpFsm::getRib4() const {
    return rib4_local ? *rib4 : *(config.rib4);
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

    ssize_t fill_ret = in_sink.fill(buffer, buffer_size);
    if (fill_ret != (ssize_t) buffer_size) {
        logger->log(ERROR, "BgpFsm::run: failed to fill() sink.\n");
        setState(BROKEN);
        return -1;
    }

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

        if (poured == 0) return 3;

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
    peer_asn = open_msg->getAsn();
    use_4b_asn = open_msg->hasCapability(ASN_4B) && config.use_4b_asn;
    if (open_msg->hasCapability(MP_BGP)) {
        send_ipv4_routes = send_ipv6_routes = false;

        const std::vector<std::shared_ptr<BgpCapability>> &capabilities = open_msg->getCapabilities();

        for (const std::shared_ptr<BgpCapability> &cap : capabilities) {
            if (cap->code == MP_BGP) {
                const BgpCapabilityMpBgp &mp_cap = dynamic_cast<const BgpCapabilityMpBgp &> (*cap);
                if (mp_cap.afi == IPV6) send_ipv6_routes = true && config.mp_bgp_ipv6;
                if (mp_cap.afi == IPV4) send_ipv4_routes = true && config.mp_bgp_ipv4;
            }
        }
    } else {
        send_ipv4_routes = true && !(config.mp_bgp_ipv6 && !config.mp_bgp_ipv4);
        send_ipv6_routes = false;
    }
    
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

            logger->log(INFO, "(new_session) we have higher router id, dispose this new session.\n");

            setState(IDLE);
            return 0;
        } else {
            // this is a new connection, and peer has higher ID. the exisiting
            // connection should be dispose, since THIS fsm is created by peer 
            // connecting to us, this one will be kept, we do nothing.

            logger->log(INFO, "(new_session) peer has higher router id, ask the other fsm to dispose session.\n");
            return 1;
        }
    } else {
        if (ntohl(config.router_id) > ntohl(peer_bgp_id)) {
            // this is a old connection, and "we" have higer ID, the new one
            // shoud close, we do nothing.
            logger->log(INFO, "(old_session) we have higher router id, keep this old session.\n");

            return 1;
        } else {
            // this is a old connection, and "peer" have higer ID, this one
            // shoud close.

            logger->log(INFO, "(old_session) peer has higher router id, dispose this session.\n");
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
    if (ev.type == ADD4) return handleRoute4AddEvent(dynamic_cast <const Route4AddEvent&>(ev));
    if (ev.type == WITHDRAW4) return handleRoute4WithdrawEvent(dynamic_cast <const Route4WithdrawEvent&>(ev));
    if (ev.type == COLLISION) return handleRouteCollisionEvent(dynamic_cast <const RouteCollisionEvent&>(ev));

    return false;
}

bool BgpFsm::handleRouteCollisionEvent(const RouteCollisionEvent &ev) {
    if (state != OPEN_CONFIRM || peer_bgp_id != ev.peer_bgp_id) return false;

    LIBBGP_LOG(logger, INFO) {
        logger->log(INFO, "BgpFsm::handleRouteCollisionEvent: detecting collision with %s.\n", inet_ntoa(*(const struct in_addr*) &(ev.peer_bgp_id)));
    }
    return resloveCollision(ev.peer_bgp_id, false) == 1;
}

bool BgpFsm::handleRoute4AddEvent(const Route4AddEvent &ev) {
    if (state != ESTABLISHED) return false;
    if (!send_ipv4_routes) return false;

    logger->log(INFO, "BgpFsm::handleRoute4AddEvent: got route-add event with %zu routes.\n", ev.routes.size());

    BgpUpdateMessage update (logger, use_4b_asn);
    update.setAttribs(ev.attribs);

    for (const Prefix4 &route : ev.routes) {
        if (config.out_filters4.apply(route.getPrefix(), route.getLength()) == ACCEPT) {
            update.addNlri4(route);
        } else {
            LIBBGP_LOG(logger, INFO) {
                uint32_t prefix = route.getPrefix();
                char ip_str[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &prefix, ip_str, INET_ADDRSTRLEN);
                logger->log(INFO, "BgpFsm::handleRoute4AddEvent: route %s/%d filtered by out_filter.\n", ip_str, route.getLength());
            }
        }
    }

    if (update.nlri.size() <= 0) return false;

    prepareUpdateMessage(update, true);

    if(!writeMessage(update)) return false;
    return true;
}

bool BgpFsm::handleRoute4WithdrawEvent(const Route4WithdrawEvent &ev) {
    if (state != ESTABLISHED) return false;
    if (!send_ipv4_routes) return false;

    logger->log(INFO, "BgpFsm::handleRoute4AddEvent: got route-withdraw event with %zu routes.\n", ev.routes.size());


    BgpUpdateMessage withdraw (logger, use_4b_asn);
    withdraw.setWithdrawn4(ev.routes);

    if(!writeMessage(withdraw)) return false;
    return true;
}

void BgpFsm::prepareUpdateMessage(BgpUpdateMessage &update, bool alter_v4_nexthop) {
    update.dropNonTransitive();

    // ipv4 nexthop related stuffs
    if (alter_v4_nexthop) {
        if (config.forced_default_nexthop4 || !update.hasAttrib(NEXT_HOP)) {
            LIBBGP_LOG(logger, INFO) {
                char ip_str[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &(config.default_nexthop4), ip_str, INET_ADDRSTRLEN);
                if (config.forced_default_nexthop4) {
                    logger->log(INFO, "BgpFsm::prepareUpdateMessage: forced_default_nexthop4 set, using %s as nexthop.\n", ip_str);
                } else {
                    logger->log(INFO, "BgpFsm::prepareUpdateMessage: no nethop attribute set, using %s as nexthop.\n", ip_str);
                }
            }
            update.setNextHop(config.default_nexthop4);
        } else {
            BgpPathAttribNexthop &nh = dynamic_cast<BgpPathAttribNexthop &> (update.getAttrib(NEXT_HOP));
            if (!config.peering_lan4.includes(nh.next_hop)) {
                LIBBGP_LOG(logger, INFO) {
                    char def_nexthop[INET_ADDRSTRLEN];
                    char cur_nexthop[INET_ADDRSTRLEN];
                    inet_ntop(AF_INET, &(config.default_nexthop4), def_nexthop, INET_ADDRSTRLEN);
                    inet_ntop(AF_INET, &(nh.next_hop), cur_nexthop, INET_ADDRSTRLEN);
                    logger->log(INFO, "BgpFsm::prepareUpdateMessage: nexthop %s not in peering lan, using %s.\n", cur_nexthop, def_nexthop);
                }
                nh.next_hop = config.default_nexthop4;
            }
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

    if (config.mp_bgp_ipv4) {
        BgpCapabilityMpBgp *cap = new BgpCapabilityMpBgp(logger);
        cap->afi = IPV4;
        cap->afi = UNICAST;
        open_reply.addCapability(std::shared_ptr<BgpCapability>(cap));
    }

    if (config.mp_bgp_ipv6) {
        BgpCapabilityMpBgp *cap = new BgpCapabilityMpBgp(logger);
        cap->afi = IPV6;
        cap->afi = UNICAST;
        open_reply.addCapability(std::shared_ptr<BgpCapability>(cap));
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

    if (send_ipv4_routes) {
        std::vector<BgpRib4Entry>::const_iterator iter = rib4->get().begin();
        const std::vector<BgpRib4Entry>::const_iterator end = rib4->get().end();

        // group routes and and updates
        while (iter != end) {
            uint64_t cur_group_id = iter->update_id;
            BgpUpdateMessage update (logger, use_4b_asn);
            update.setAttribs(iter->attribs);
            prepareUpdateMessage(update, true);

            // length of the update message, 19: headers, 4: length fields
            size_t msg_len = 19 + 4;

            for (const std::shared_ptr<BgpPathAttrib> &attrib : update.path_attribute) {
                msg_len += attrib->length(); 
            }

            for (; iter != end && cur_group_id == iter->update_id && msg_len < 4096; iter++) {
                const Prefix4 &r = iter->route;
                if (config.out_filters4.apply(r.getPrefix(), r.getLength()) == ACCEPT) {
                    msg_len += 1 + (r.getLength() + 7) / 8;
                    if (msg_len > 4096) {
                        // size too big, roll back and break.
                        iter--;
                        break;
                    }
                    update.addNlri4(r);
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
                if(!writeMessage(update)) return -1;
            }
        }
    }

    if (send_ipv6_routes) {
        std::vector<BgpRib6Entry>::const_iterator iter = rib6->get().begin();
        std::vector<BgpRib6Entry>::const_iterator end = rib6->get().end();

        while (iter != end) {
            uint64_t cur_group_id = iter->update_id;
            const uint8_t *nh_global = iter->nexthop_global;
            const uint8_t *nh_linklocal = iter->nexthop_linklocal;
            BgpUpdateMessage update (logger, use_4b_asn);
            update.setAttribs(iter->attribs);
            prepareUpdateMessage(update, false);
            std::vector<Prefix6> filtered_nlri;

            size_t msg_len = 19 + 4 + 8; // 8: mp-reach-nlri headers (attrib hdr: 3, afi/safi/nh_len/res: 5)
            for (const std::shared_ptr<BgpPathAttrib> &attrib : update.path_attribute) {
                msg_len += attrib->length(); 
            }

            for (; iter != end && cur_group_id == iter->update_id && msg_len < 4096; iter++) {
                const Prefix6 &r = iter->route;
                if (config.out_filters6.apply(r) == ACCEPT) {
                    msg_len += 1 + (r.getLength() + 7) / 8;
                    if (msg_len > 4096) {
                        // size too big, roll back and break.
                        iter--;
                        break;
                    }
                    filtered_nlri.push_back(r);
                } else {
                    LIBBGP_LOG(logger, INFO) {
                        uint8_t prefix[16]; 
                        r.getPrefix(prefix);
                        char ip_str[INET6_ADDRSTRLEN];
                        inet_ntop(AF_INET6, &prefix, ip_str, INET6_ADDRSTRLEN);
                        logger->log(INFO, "BgpFsm::fsmEvalOpenConfirm: route %s/%d filtered by out_filter.\n", ip_str, r.getLength());
                    }
                }
            }

            if (filtered_nlri.size() > 0) {
                if (config.forced_default_nexthop6 && !config.peering_lan6.includes(nh_global)) {
                    LIBBGP_LOG(logger, INFO) {
                        char nh_old_str[INET6_ADDRSTRLEN];
                        char nh_def_str[INET6_ADDRSTRLEN];
                        inet_ntop(AF_INET6, &nh_global, nh_old_str, INET6_ADDRSTRLEN);
                        inet_ntop(AF_INET6, &nh_def_str, nh_def_str, INET6_ADDRSTRLEN);

                        if (config.forced_default_nexthop6) {
                            logger->log(INFO, "BgpFsm::fsmEvalOpenConfirm: forced_default_nexthop6 set, default (%s) will be used.\n", nh_def_str);
                        }
                        else logger->log(INFO, "BgpFsm::fsmEvalOpenConfirm: nexthop %s is not in peering lan, default (%s) will be used.\n", nh_old_str, nh_def_str);
                    }

                    update.setNlri6(filtered_nlri, config.default_nexthop6_global, config.default_nexthop6_linklocal);
                } else update.setNlri6(filtered_nlri, nh_global, nh_linklocal);

                
                if(!writeMessage(update)) return -1;
            }

        }
    }

    return 1;
}

int BgpFsm::fsmEvalEstablished(const BgpMessage *msg) {
    if (msg->type == KEEPALIVE) return 1;

    const BgpUpdateMessage *update = dynamic_cast<const BgpUpdateMessage *>(msg);

    if (send_ipv4_routes) {
        for (const Prefix4 &route : update->withdrawn_routes) {
            rib4->withdraw(peer_bgp_id, route);
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
        
            if (!config.no_nexthop_check4 && !config.peering_lan4.includes(nh.next_hop)) {
                LIBBGP_LOG(logger, WARN) {
                    char ip_str_nh[INET_ADDRSTRLEN];
                    char ip_str_lan[INET_ADDRSTRLEN];
                    inet_ntop(AF_INET, &(nh.next_hop), ip_str_nh, INET_ADDRSTRLEN);
                    uint32_t peering_lan_pfx = config.peering_lan4.getPrefix();
                    inet_ntop(AF_INET, &peering_lan_pfx, ip_str_lan, INET_ADDRSTRLEN);
                    logger->log(WARN, "BgpFsm::fsmEvalEstablished: ignored %zu routes with nexthop outside peering LAN. (%s not in %s/%d)\n", 
                        update->nlri.size(), ip_str_nh, ip_str_lan, peering_lan_pfx);
                }
                return 1;
            };
        }
        
        std::vector<Prefix4> routes = std::vector<Prefix4> ();
        for (const Prefix4 &route : update->nlri) {
            if(config.in_filters4.apply(route.getPrefix(), route.getLength()) == ACCEPT) {
                routes.push_back(route);
            } else {
                LIBBGP_LOG(logger, INFO) {
                    uint32_t prefix = route.getPrefix();
                    char ip_str[INET_ADDRSTRLEN];
                    inet_ntop(AF_INET, &prefix, ip_str, INET_ADDRSTRLEN);
                    logger->log(INFO, "BgpFsm::fsmEvalEstablished: route %s/%d filtered by in_filters4.\n", ip_str, route.getLength());
                }
            }
        }

        if (routes.size() > 0) {
            rib4->insert(peer_bgp_id, routes, update->path_attribute);
        }

        if (rev_bus_exist) {
            if (update->withdrawn_routes.size() > 0) {
                Route4WithdrawEvent wev = Route4WithdrawEvent();
                wev.routes = update->withdrawn_routes;
                config.rev_bus->publish(this, wev);
            }

            if (routes.size() > 0) {
                Route4AddEvent aev = Route4AddEvent();
                aev.routes = routes;
                aev.attribs = update->path_attribute;
                config.rev_bus->publish(this, aev);
            }
        }
    }

    if (send_ipv6_routes) {
        // TODO
    }

    return 1;
}

void BgpFsm::dropAllRoutes() {
    if (peer_bgp_id != 0) {
        std::vector<Prefix4> dropped_routes = rib4->discard(peer_bgp_id);
        if (rev_bus_exist && dropped_routes.size() > 0) {
            Route4WithdrawEvent wev;
            wev.routes = dropped_routes;
            config.rev_bus->publish(this, wev);
        }
    }
}

void BgpFsm::setState(BgpState new_state) {
    if (state == new_state) return;

    if (config.out_handler) config.out_handler->notifyStateChange(state, new_state);

    logger->log(INFO, "BgpFsm::setState: changing state: %s -> %s\n", bgp_fsm_state_str[state], bgp_fsm_state_str[new_state]); 

    if (state == ESTABLISHED) {
        logger->log(INFO, "BgpFsm::setState: dropping all routes received from peer...\n");

        // moved from ESTABLISHED to something else. Drop all routes.
        dropAllRoutes();
    }

    state = new_state;
}

bool BgpFsm::validAddr(uint32_t addr) const {
    if (addr == config.default_nexthop4 || addr == config.router_id) {
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