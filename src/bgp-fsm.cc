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
    return config.asn;
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

BgpRib6& BgpFsm::getRib6() const {
    return rib4_local ? *rib6 : *(config.rib6);
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
    
    logger->log(DEBUG, "BgpFsm::start: sending OPEN message to peer.\n");

    uint16_t my_asn_2b = config.asn >= 0xffff ? 23456 : config.asn;

    BgpOpenMessage msg(logger, config.use_4b_asn, my_asn_2b, config.hold_timer, config.router_id);
    if (config.use_4b_asn) {
        msg.setAsn(config.asn);
    }

    if (config.mp_bgp_ipv4) {
        BgpCapabilityMpBgp *cap = new BgpCapabilityMpBgp(logger);
        cap->afi = IPV4;
        cap->safi = UNICAST;
        msg.addCapability(std::shared_ptr<BgpCapability>(cap));
    }

    if (config.mp_bgp_ipv6) {
        BgpCapabilityMpBgp *cap = new BgpCapabilityMpBgp(logger);
        cap->afi = IPV6;
        cap->safi = UNICAST;
        msg.addCapability(std::shared_ptr<BgpCapability>(cap));
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
    if (!config.no_autotick) {
        int tick_ret = tick();
        if (tick_ret <= 0) return tick_ret;
    }
    
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
    uint64_t now = clock->getTime();
    if (hold_timer > 0 && now - last_recv > hold_timer) {
        logger->log(ERROR, "BgpFsm::tick: peer hold timer expired (last_recv: %d, now: %d, diff: %d, hold: %d).\n", last_recv, now, now - last_recv, hold_timer);
        BgpNotificationMessage notify (logger, E_HOLD, 0, NULL, 0);
        setState(IDLE);
        if(!writeMessage(notify)) return -1;
        return 0;
    }

    // send keepalive? 
    if (hold_timer > 0 && now - last_sent > hold_timer / 3) {
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

    peer_asn = open_msg->getAsn();

    if (config.peer_asn != 0 && peer_asn != config.peer_asn) {
        BgpNotificationMessage notify (logger, E_OPEN, E_PEER_AS, NULL, 0);
        setState(IDLE);
        if(!writeMessage(notify)) return -1;
        return 0;
    }

    ibgp = config.asn == peer_asn;

    if (!validAddr4(open_msg->bgp_id)) {
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
    use_4b_asn = open_msg->hasCapability(ASN_4B) && config.use_4b_asn;
    send_ipv4_routes = true;
    if (open_msg->hasCapability(MP_BGP) && (config.mp_bgp_ipv6 || config.mp_bgp_ipv4)) {
        send_ipv4_routes = send_ipv6_routes = false;

        const std::vector<std::shared_ptr<BgpCapability>> &capabilities = open_msg->getCapabilities();

        for (const std::shared_ptr<BgpCapability> &cap : capabilities) {
            if (cap->code == MP_BGP) {
                const BgpCapabilityMpBgp &mp_cap = dynamic_cast<const BgpCapabilityMpBgp &> (*cap);
                if (mp_cap.safi != UNICAST) continue;
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
    if (ev.type == ADD6) return handleRoute6AddEvent(dynamic_cast <const Route6AddEvent&>(ev));
    if (ev.type == WITHDRAW6) return handleRoute6WithdrawEvent(dynamic_cast <const Route6WithdrawEvent&>(ev));
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

bool BgpFsm::handleRoute6AddEvent(const Route6AddEvent &ev) {
    if (state != ESTABLISHED) return false;
    if (!send_ipv6_routes) return false; 
    if ((ev.shared_attribs == NULL || ev.new_routes == NULL) && ev.replaced_entries == NULL) return false;

    size_t nroutes = 0;
    if (ev.replaced_entries != NULL) nroutes += ev.replaced_entries->size();
    if (ev.new_routes != NULL) nroutes += ev.new_routes->size();

    logger->log(DEBUG, "BgpFsm::handleRoute6AddEvent: got route-add event with %zu routes.\n", nroutes);

    if (ev.new_routes != NULL && ev.shared_attribs != NULL) {
        if (!ibgp || ev.ibgp_peer_asn != peer_asn) {
            std::vector<Prefix6> routes;
            const uint8_t *nh_global = ev.nexthop_global;
            const uint8_t *nh_local = ev.nexthop_linklocal;
            alterNexthop6(nh_local, nh_global);

            for (const Prefix6 &route : *(ev.new_routes)) {
                if (config.out_filters6.apply(route, *(ev.shared_attribs))) {
                    routes.push_back(route);
                } else {
                    LIBBGP_LOG(logger, DEBUG) {
                        uint8_t prefix[16];
                        route.getPrefix(prefix);
                        char prefix_str[INET6_ADDRSTRLEN];
                        inet_ntop(AF_INET6, &prefix, prefix_str, INET6_ADDRSTRLEN);
                        logger->log(DEBUG, "BgpFsm::handleRoute6AddEvent: route %s/%d filtered by out_filter.\n", prefix_str, route.getLength());
                    }
                }
            }

            if (routes.size() > 0) {
                BgpUpdateMessage update (logger, use_4b_asn);
                update.setAttribs(*(ev.shared_attribs));
                prepareUpdateMessage(update);
                update.setNlri6(routes, nh_global, nh_local);

                if(!writeMessage(update)) return false;
            }

        } else {
            logger->log(DEBUG, "BgpFsm::handleRoute6AddEvent: ignoring new_routes in add event since remote is IBGP.\n");
        }
    } else {
        logger->log(DEBUG, "BgpFsm::handleRoute6AddEvent: new_routes or shared_attribs is NULL.\n");
    }

    if (ev.replaced_entries == NULL) {
        logger->log(DEBUG, "BgpFsm::handleRoute6AddEvent: replaced_entries is NULL.\n");
        return true;
    }

    // consider merging of replaced_entries?
    for (const BgpRib6Entry &entry : *(ev.replaced_entries)) {
        if (entry.src_router_id == peer_bgp_id) continue;

        if (ibgp && entry.ibgp_peer_asn == peer_asn) {
            LIBBGP_LOG(logger, DEBUG) {
                uint8_t prefix[16];
                entry.route.getPrefix(prefix);
                char prefix_str[INET6_ADDRSTRLEN];
                inet_ntop(AF_INET6, &prefix, prefix_str, INET6_ADDRSTRLEN);
                logger->log(DEBUG, "BgpFsm::handleRoute6AddEvent: route %s/%d in replaced_entries ignored as it is IBGP.\n", prefix_str, entry.route.getLength());
            }
            
            continue;
        }

        if (config.out_filters6.apply(entry.route, entry.attribs) != ACCEPT) {
            LIBBGP_LOG(logger, DEBUG) {
                uint8_t prefix[16];
                entry.route.getPrefix(prefix);
                char prefix_str[INET6_ADDRSTRLEN];
                inet_ntop(AF_INET6, &prefix, prefix_str, INET6_ADDRSTRLEN);
                logger->log(DEBUG, "BgpFsm::handleRoute6AddEvent: route %s/%d in replaced_entries filtered.\n", prefix_str, entry.route.getLength());
            }

            continue;
        }

        BgpUpdateMessage update (logger, use_4b_asn);
        update.setAttribs(entry.attribs);
        const uint8_t *nh_global = entry.nexthop_global;
        const uint8_t *nh_local = entry.nexthop_linklocal;
        alterNexthop6(nh_local, nh_global);
        std::vector<Prefix6> routes;
        routes.push_back(entry.route);
        update.setNlri6(routes, nh_global, nh_local);
        prepareUpdateMessage(update);
        if(!writeMessage(update)) return false;
    }

    return true;
}

bool BgpFsm::handleRoute6WithdrawEvent(const Route6WithdrawEvent &ev) {
    if (state != ESTABLISHED) return false;
    if (!send_ipv6_routes) return false;
    if (ev.routes == NULL) return false;

    logger->log(DEBUG, "BgpFsm::handleRoute6WithdrawEvent: got route-withdraw event with %zu routes.\n", ev.routes->size());


    BgpUpdateMessage withdraw (logger, use_4b_asn);
    withdraw.setWithdrawn6(*(ev.routes));

    if(!writeMessage(withdraw)) return false;
    return true;
}

bool BgpFsm::handleRoute4AddEvent(const Route4AddEvent &ev) {
    if (state != ESTABLISHED) return false;
    if (!send_ipv4_routes) return false;
    if ((ev.shared_attribs == NULL || ev.new_routes == NULL) && ev.replaced_entries == NULL) return false;

    size_t nroutes = 0;
    if (ev.replaced_entries != NULL) nroutes += ev.replaced_entries->size();
    if (ev.new_routes != NULL) nroutes += ev.new_routes->size();

    logger->log(DEBUG, "BgpFsm::handleRoute4AddEvent: got route-add event with %zu routes.\n", nroutes);

    if (ev.new_routes != NULL && ev.shared_attribs != NULL) {
        if (!ibgp || ev.ibgp_peer_asn != peer_asn) {
            BgpUpdateMessage update (logger, use_4b_asn);
            update.setAttribs(*(ev.shared_attribs));

            for (const Prefix4 &route : *(ev.new_routes)) {
                if (config.out_filters4.apply(route, *(ev.shared_attribs)) == ACCEPT) {
                    update.addNlri4(route);
                } else {
                    LIBBGP_LOG(logger, DEBUG) {
                        uint32_t prefix = route.getPrefix();
                        char ip_str[INET_ADDRSTRLEN];
                        inet_ntop(AF_INET, &prefix, ip_str, INET_ADDRSTRLEN);
                        logger->log(DEBUG, "BgpFsm::handleRoute4AddEvent: route %s/%d filtered by out_filter.\n", ip_str, route.getLength());
                    }
                }
            }

            if (update.nlri.size() > 0) {
                alterNexthop4(update);
                prepareUpdateMessage(update);

                if(!writeMessage(update)) return false;
            }
        } else {
            logger->log(DEBUG, "BgpFsm::handleRoute4AddEvent: ignoring new_routes in add event since remote is IBGP.\n");
        }
    } else {
        logger->log(DEBUG, "BgpFsm::handleRoute4AddEvent: new_routes or shared_attribs is NULL.\n");
    }

    if (ev.replaced_entries == NULL) {
        logger->log(DEBUG, "BgpFsm::handleRoute4AddEvent: replaced_entries is NULL.\n");
        return true;
    }

    // consider merging of replaced_entries?
    for (const BgpRib4Entry &entry : *(ev.replaced_entries)) {
        if (entry.src_router_id == peer_bgp_id) continue;

        if (ibgp && entry.ibgp_peer_asn == peer_asn) {
            LIBBGP_LOG(logger, DEBUG) {
                uint32_t prefix = entry.route.getPrefix();
                char ip_str[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &prefix, ip_str, INET_ADDRSTRLEN);
                logger->log(DEBUG, "BgpFsm::handleRoute4AddEvent: route %s/%d ignored since remote is IBGP.\n", ip_str, entry.route.getLength());
            }
            
            continue;
        }

        if (config.out_filters4.apply(entry.route, entry.attribs) != ACCEPT) {
            LIBBGP_LOG(logger, DEBUG) {
                uint32_t prefix = entry.route.getPrefix();
                char ip_str[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &prefix, ip_str, INET_ADDRSTRLEN);
                logger->log(DEBUG, "BgpFsm::handleRoute4AddEvent: route %s/%d filtered by out_filter.\n", ip_str, entry.route.getLength());
            }

            continue;
        }

        BgpUpdateMessage update (logger, use_4b_asn);
        update.setAttribs(entry.attribs);
        update.addNlri4(entry.route);
        alterNexthop4(update);
        prepareUpdateMessage(update);
        if(!writeMessage(update)) return false;
    }

    return true;
}

bool BgpFsm::handleRoute4WithdrawEvent(const Route4WithdrawEvent &ev) {
    if (state != ESTABLISHED) return false;
    if (!send_ipv4_routes) return false;
    if (ev.routes == NULL) return false;

    logger->log(DEBUG, "BgpFsm::handleRoute4AddEvent: got route-withdraw event with %zu routes.\n", ev.routes->size());

    BgpUpdateMessage withdraw (logger, use_4b_asn);
    withdraw.setWithdrawn4(*(ev.routes));

    if(!writeMessage(withdraw)) return false;
    return true;
}

void BgpFsm::alterNexthop4 (BgpUpdateMessage &update) {
    // ibgp
    if (ibgp && !config.ibgp_alter_nexthop) return;

    // configured to fource default nexthop, or does not have a nexthop attribute
    if (config.forced_default_nexthop4 || !update.hasAttrib(NEXT_HOP)) {
        LIBBGP_LOG(logger, INFO) {
            char ip_str[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &(config.default_nexthop4), ip_str, INET_ADDRSTRLEN);
            if (config.forced_default_nexthop4) {
                logger->log(INFO, "BgpFsm::alterNexthop4: forced_default_nexthop4 set, using %s as nexthop.\n", ip_str);
            } else {
                logger->log(INFO, "BgpFsm::alterNexthop4: no nethop attribute set, using %s as nexthop.\n", ip_str);
            }
        }
        update.setNextHop(config.default_nexthop4);
    } else {
        // nexthop not forced, check w/ peering LAN
        BgpPathAttribNexthop &nh = dynamic_cast<BgpPathAttribNexthop &> (update.getAttrib(NEXT_HOP));
        if (!config.peering_lan4.includes(nh.next_hop)) {
            LIBBGP_LOG(logger, INFO) {
                char def_nexthop[INET_ADDRSTRLEN];
                char cur_nexthop[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &(config.default_nexthop4), def_nexthop, INET_ADDRSTRLEN);
                inet_ntop(AF_INET, &(nh.next_hop), cur_nexthop, INET_ADDRSTRLEN);
                logger->log(INFO, "BgpFsm::alterNexthop4: nexthop %s not in peering lan, using %s.\n", cur_nexthop, def_nexthop);
            }
            nh.next_hop = config.default_nexthop4;
        }
    }
}

void BgpFsm::alterNexthop6 (const uint8_t* &nh_global, const uint8_t* &nh_local) {
    if (config.forced_default_nexthop6 && !config.peering_lan6.includes(nh_global) && !ibgp && !config.ibgp_alter_nexthop) {
        LIBBGP_LOG(logger, INFO) {
            char nh_old_str[INET6_ADDRSTRLEN];
            char nh_def_str[INET6_ADDRSTRLEN];
            inet_ntop(AF_INET6, &nh_global, nh_old_str, INET6_ADDRSTRLEN);
            inet_ntop(AF_INET6, &nh_def_str, nh_def_str, INET6_ADDRSTRLEN);

            if (config.forced_default_nexthop6) {
                logger->log(INFO, "BgpFsm::alterNexthop6: forced_default_nexthop6 set, default (%s) will be used.\n", nh_def_str);
            }
            else logger->log(INFO, "BgpFsm::alterNexthop6: nexthop %s is not in peering lan, default (%s) will be used.\n", nh_old_str, nh_def_str);
        }

        nh_global = config.default_nexthop6_global;
        nh_local = config.default_nexthop6_linklocal;
    }
}

void BgpFsm::prepareUpdateMessage(BgpUpdateMessage &update) {
    update.dropNonTransitive();

    if (config.use_4b_asn && use_4b_asn) {                
        update.restoreAsPath();
        update.restoreAggregator();
    } else {
        update.downgradeAsPath();
        update.downgradeAggregator();
    }

    if (!ibgp) update.prepend(config.asn);
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
        cap->safi = UNICAST;
        open_reply.addCapability(std::shared_ptr<BgpCapability>(cap));
    }

    if (config.mp_bgp_ipv6) {
        BgpCapabilityMpBgp *cap = new BgpCapabilityMpBgp(logger);
        cap->afi = IPV6;
        cap->safi = UNICAST;
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
        rib4_t::const_iterator iter = rib4->get().begin();
        rib4_t::const_iterator last_iter = iter;
        const rib4_t::const_iterator end = rib4->get().end();

        // group routes and and updates
        while (iter != end) {
            uint64_t cur_group_id = iter->second.update_id;
            BgpUpdateMessage update (logger, use_4b_asn);
            update.setAttribs(iter->second.attribs);
            alterNexthop4(update);
            prepareUpdateMessage(update);

            // length of the update message, 19: headers, 4: length fields
            size_t msg_len = 19 + 4;

            for (const std::shared_ptr<BgpPathAttrib> &attrib : update.path_attribute) {
                msg_len += attrib->length(); 
            }

            for (; iter != end && cur_group_id == iter->second.update_id && msg_len < 4096; iter++) {
                const BgpRib4Entry &e = iter->second;
                const Prefix4 &r = e.route;
                if (e.status == RS_STANDBY) continue;

                if (ibgp && e.src == SRC_IBGP && e.ibgp_peer_asn == peer_asn) {
                    LIBBGP_LOG(logger, DEBUG) {
                        uint32_t prefix = r.getPrefix();
                        char ip_str[INET_ADDRSTRLEN];
                        inet_ntop(AF_INET, &prefix, ip_str, INET_ADDRSTRLEN);
                        logger->log(DEBUG, "BgpFsm::fsmEvalOpenConfirm: ignored IBGP route %s/%d.\n", ip_str, r.getLength());
                    }
                    last_iter = iter;
                    continue;
                }

                if (iter->second.src_router_id == peer_bgp_id) {
                    LIBBGP_LOG(logger, WARN) {
                        uint32_t prefix = r.getPrefix();
                        char ip_str[INET_ADDRSTRLEN];
                        inet_ntop(AF_INET, &prefix, ip_str, INET_ADDRSTRLEN);
                        logger->log(WARN, "BgpFsm::fsmEvalOpenConfirm: route %s/%d has src_bgp_id same as peer, ignore.\n", ip_str, r.getLength());
                    }
                    last_iter = iter;
                    continue;
                }
                if (config.out_filters4.apply(r, update.path_attribute) == ACCEPT) {
                    msg_len += 1 + (r.getLength() + 7) / 8;
                    if (msg_len > 4096) {
                        // size too big, roll back and break.
                        iter = last_iter;
                        break;
                    }
                    update.addNlri4(r);
                } else {
                    LIBBGP_LOG(logger, DEBUG) {
                        uint32_t prefix = r.getPrefix();
                        char ip_str[INET_ADDRSTRLEN];
                        inet_ntop(AF_INET, &prefix, ip_str, INET_ADDRSTRLEN);
                        logger->log(DEBUG, "BgpFsm::fsmEvalOpenConfirm: route %s/%d filtered by out_filter.\n", ip_str, r.getLength());
                    }
                }
                last_iter = iter;
            }

            if (update.nlri.size() > 0) {
                if(!writeMessage(update)) return -1;
            }
        }
    }

    if (send_ipv6_routes) {
        rib6_t::const_iterator iter = rib6->get().begin();
        rib6_t::const_iterator last_iter = iter;
        const rib6_t::const_iterator end = rib6->get().end();

        while (iter != end) {
            uint64_t cur_group_id = iter->second.update_id;
            const uint8_t *nh_global = iter->second.nexthop_global;
            const uint8_t *nh_linklocal = iter->second.nexthop_linklocal;
            BgpUpdateMessage update (logger, use_4b_asn);
            update.setAttribs(iter->second.attribs);

            prepareUpdateMessage(update);
            std::vector<Prefix6> filtered_nlri;

            // 8: mp-reach-nlri headers (attrib hdr: 3, afi/safi/nh_len/res: 5)
            // 32: max nexthop len
            size_t msg_len = 19 + 4 + 8 + 32;
            for (const std::shared_ptr<BgpPathAttrib> &attrib : update.path_attribute) {
                msg_len += attrib->length(); 
            }

            for (; iter != end && cur_group_id == iter->second.update_id && msg_len < 4096; iter++) {
                const BgpRib6Entry &e = iter->second;
                const Prefix6 &r = e.route;
                if (e.status != RS_ACTIVE) continue;
                if (ibgp && e.src == SRC_IBGP && e.ibgp_peer_asn == peer_asn) {
                    LIBBGP_LOG(logger, DEBUG) {
                        uint8_t prefix[16]; 
                        r.getPrefix(prefix);
                        char ip_str[INET6_ADDRSTRLEN];
                        inet_ntop(AF_INET6, &prefix, ip_str, INET6_ADDRSTRLEN);
                        logger->log(DEBUG, "BgpFsm::fsmEvalOpenConfirm: ignored IBGP route %s/%d.\n", ip_str, r.getLength());
                    }
                    last_iter = iter;
                    continue;
                }
                if (iter->second.src_router_id == peer_bgp_id) {
                    LIBBGP_LOG(logger, WARN) {
                        uint8_t prefix[16]; 
                        r.getPrefix(prefix);
                        char ip_str[INET6_ADDRSTRLEN];
                        inet_ntop(AF_INET6, &prefix, ip_str, INET6_ADDRSTRLEN);
                        logger->log(WARN, "BgpFsm::fsmEvalOpenConfirm: route %s/%d has src_bgp_id same as peer, ignore.\n", ip_str, r.getLength());
                    }
                    last_iter = iter;
                    continue;
                }

                if (config.out_filters6.apply(r, update.path_attribute) == ACCEPT) {
                    msg_len += 1 + (r.getLength() + 7) / 8;
                    if (msg_len > 4096) {
                        // size too big, roll back and break.
                        iter = last_iter;
                        break;
                    }
                    filtered_nlri.push_back(r);
                } else {
                    LIBBGP_LOG(logger, DEBUG) {
                        uint8_t prefix[16]; 
                        r.getPrefix(prefix);
                        char ip_str[INET6_ADDRSTRLEN];
                        inet_ntop(AF_INET6, &prefix, ip_str, INET6_ADDRSTRLEN);
                        logger->log(DEBUG, "BgpFsm::fsmEvalOpenConfirm: route %s/%d filtered by out_filter.\n", ip_str, r.getLength());
                    }
                }
                last_iter = iter;
            }

            if (filtered_nlri.size() > 0) {
                alterNexthop6(nh_global, nh_linklocal);
                update.setNlri6(filtered_nlri, nh_global, nh_linklocal);
                if(!writeMessage(update)) return -1;
            }

        }
    }

    return 1;
}

int BgpFsm::fsmEvalEstablished(const BgpMessage *msg) {
    if (msg->type == KEEPALIVE) return 1;

    const BgpUpdateMessage *update = dynamic_cast<const BgpUpdateMessage *>(msg);

    bool ignore_routes = false;

    // checks
    if (update->hasAttrib(AS_PATH) && update->nlri.size() > 0) {
        const BgpPathAttribAsPath &as_path = dynamic_cast<const BgpPathAttribAsPath&>(update->getAttrib(AS_PATH));

        for (const BgpAsPathSegment &seg : as_path.as_paths) {
            int8_t local_count = 0;

            for (uint32_t asn : seg.value) {
                if (asn == config.asn) local_count++;
            }

            if (local_count > config.allow_local_as) {
                logger->log(WARN, "BgpFsm::fsmEvalEstablished: ignoring routes with %d local asn in as_path (max %d are allowed).\n", local_count, config.allow_local_as);
                ignore_routes = true;
                break;
            }
        }
    } else if (update->nlri.size() > 0) ignore_routes = true; // since no AS_PATH and nlri non empty. (should be handleded by update-msg already tho)

    if (send_ipv4_routes) {
        std::vector<Prefix4> unreach;
        std::vector<BgpRib4Entry> changed_entries;
        for (const Prefix4 &r : update->withdrawn_routes) {
            std::pair<bool, const void*> w_ret = rib4->withdraw(peer_bgp_id, r);
            if (!rev_bus_exist) continue;
            if (!w_ret.first) {
                if (w_ret.second == NULL) unreach.push_back(r);
            }
            else if (w_ret.second != NULL) {
                changed_entries.push_back(*((BgpRib4Entry *) w_ret.second));
            }
        }

        // more checks
        if (update->nlri.size() > 0) {
            const BgpPathAttribNexthop &nh = dynamic_cast<const BgpPathAttribNexthop &>(update->getAttrib(NEXT_HOP));

            if (!ignore_routes && !validAddr4(nh.next_hop)) {
                LIBBGP_LOG(logger, WARN) {
                    char ip_str[INET_ADDRSTRLEN];
                    inet_ntop(AF_INET, &(nh.next_hop), ip_str, INET_ADDRSTRLEN);
                    logger->log(WARN, "BgpFsm::fsmEvalEstablished: ignored %zu routes with invalid nexthop %s\n", update->nlri.size(), ip_str);
                }
                ignore_routes = true;
            }
        
            if (!ignore_routes && !config.no_nexthop_check4 && !config.peering_lan4.includes(nh.next_hop) && !ibgp) {
                LIBBGP_LOG(logger, WARN) {
                    char ip_str_nh[INET_ADDRSTRLEN];
                    char ip_str_lan[INET_ADDRSTRLEN];
                    inet_ntop(AF_INET, &(nh.next_hop), ip_str_nh, INET_ADDRSTRLEN);
                    uint32_t peering_lan_pfx = config.peering_lan4.getPrefix();
                    inet_ntop(AF_INET, &peering_lan_pfx, ip_str_lan, INET_ADDRSTRLEN);
                    logger->log(WARN, "BgpFsm::fsmEvalEstablished: ignored %zu routes with nexthop outside peering LAN. (%s not in %s/%d)\n", 
                        update->nlri.size(), ip_str_nh, ip_str_lan, config.peering_lan4.getLength());
                }
                ignore_routes = true;
            };
        }

        // filter & insert to rib
        if (!ignore_routes) {
            std::vector<Prefix4> routes = std::vector<Prefix4> ();
            for (const Prefix4 &route : update->nlri) {
                if(config.in_filters4.apply(route, update->path_attribute) == ACCEPT) {
                    routes.push_back(route);
                } else {
                    LIBBGP_LOG(logger, DEBUG) {
                        uint32_t prefix = route.getPrefix();
                        char ip_str[INET_ADDRSTRLEN];
                        inet_ntop(AF_INET, &prefix, ip_str, INET_ADDRSTRLEN);
                        logger->log(DEBUG, "BgpFsm::fsmEvalEstablished: route %s/%d filtered by in_filters4.\n", ip_str, route.getLength());
                    }
                }
            }

            std::pair<std::vector<BgpRib4Entry>, std::vector<Prefix4>> rslt;
            if (routes.size() > 0) {
                rslt = rib4->insert(peer_bgp_id, routes, update->path_attribute, config.weight, ibgp ? peer_asn : 0);
                for (const BgpRib4Entry &entry : rslt.first) {
                    changed_entries.push_back(entry);
                }
                logger->log(DEBUG, "BgpFsm::fsmEvalEstablished: rib4.insert(): %zu altered and %zu added in %zu routes.\n", rslt.first.size(), rslt.second.size(), routes.size());
            }

            if (rev_bus_exist && (changed_entries.size() > 0 || rslt.second.size() > 0)) {
                logger->log(DEBUG, "BgpFsm::fsmEvalEstablished: publishing new v4 routes on event bus...\n");
                Route4AddEvent aev = Route4AddEvent();
                aev.replaced_entries = changed_entries.size() > 0 ? &changed_entries : NULL;
                aev.shared_attribs = &(update->path_attribute);
                aev.new_routes = rslt.second.size() > 0 ? &(rslt.second) : NULL;
                if (ibgp) aev.ibgp_peer_asn = peer_asn;
                config.rev_bus->publish(this, aev);
            }

            if (rev_bus_exist && unreach.size() > 0) {
                logger->log(DEBUG, "BgpFsm::fsmEvalEstablished: publishing dropped v4 routes on event bus...\n");
                Route4WithdrawEvent wev = Route4WithdrawEvent();
                wev.routes = &unreach;
                config.rev_bus->publish(this, wev);
            }
        }
    }

    if (send_ipv6_routes) {
        std::vector<Prefix6> unreach;
        std::vector<BgpRib6Entry> changed_entries;
        if (update->hasAttrib(MP_UNREACH_NLRI)) {
            const BgpPathAttrib &attr = update->getAttrib(MP_UNREACH_NLRI);
            const BgpPathAttribMpNlriBase &mp_unreach = dynamic_cast<const BgpPathAttribMpNlriBase &>(attr);
            if (mp_unreach.afi == IPV6 && mp_unreach.safi == UNICAST) {
                const BgpPathAttribMpUnreachNlriIpv6 &u = dynamic_cast<const BgpPathAttribMpUnreachNlriIpv6 &>(mp_unreach);

                for (const Prefix6 &r : u.withdrawn_routes) {
                    std::pair<bool, const void*> w_ret = rib6->withdraw(peer_bgp_id, r);
                    if (!rev_bus_exist) continue;
                    if (!w_ret.first) {
                        if (w_ret.second == NULL) unreach.push_back(r);
                    }
                    else if (w_ret.second != NULL) {
                        changed_entries.push_back(*((BgpRib6Entry *) w_ret.second));
                    }
                }
            }
        }

        if (!ignore_routes && update->hasAttrib(MP_REACH_NLRI)) {
            const BgpPathAttrib &attr = update->getAttrib(MP_REACH_NLRI);
            const BgpPathAttribMpNlriBase &mp_reach = dynamic_cast<const BgpPathAttribMpNlriBase &>(attr);
            if (mp_reach.afi == IPV6 && mp_reach.safi == UNICAST) {
                const BgpPathAttribMpReachNlriIpv6 &reach = dynamic_cast<const BgpPathAttribMpReachNlriIpv6 &>(mp_reach);

                if (!validAddr6(reach.nexthop_global) || (!v6addr_is_zero(reach.nexthop_linklocal) && !validAddr6(reach.nexthop_linklocal))) {
                    logger->log(WARN, "BgpFsm::fsmEvalEstablished: ignored %zu routes with invalid nexthop:\n", reach.nlri.size());
                    logger->log(WARN, reach);
                    return 1;
                }

                // filter toures
                std::vector<Prefix6> filtered_routes;
                for (const Prefix6 &route : reach.nlri) {
                    if (config.in_filters6.apply(route, update->path_attribute) == ACCEPT) filtered_routes.push_back(route);
                }

                if (filtered_routes.size() <= 0) return 1;

                // TODO verify with no_nexthop_check6

                // remove MP_* & nexthop attribute
                std::vector<std::shared_ptr<BgpPathAttrib>> attrs;

                for (const std::shared_ptr<BgpPathAttrib> &attr : update->path_attribute) {
                    if (attr->type_code == MP_REACH_NLRI || attr->type_code == MP_UNREACH_NLRI || attr->type_code == NEXT_HOP) continue;
                    attrs.push_back(attr);
                }

                std::pair<std::vector<BgpRib6Entry>, std::vector<Prefix6>> rslt = rib6->insert(peer_bgp_id, filtered_routes, reach.nexthop_global, reach.nexthop_linklocal, attrs, config.weight, ibgp ? peer_asn : 0);
                logger->log(DEBUG, "BgpFsm::fsmEvalEstablished: rib6.insert(): %zu altered and %zu added in %zu routes.\n", rslt.first.size(), rslt.second.size(), filtered_routes.size());

                for (const BgpRib6Entry &e : rslt.first) {
                    changed_entries.push_back(e);
                }

                if (rev_bus_exist && (changed_entries.size() > 0 || rslt.second.size() > 0)) {
                    Route6AddEvent aev = Route6AddEvent();
                    memcpy(aev.nexthop_global, reach.nexthop_global, 16);
                    memcpy(aev.nexthop_linklocal, reach.nexthop_linklocal, 16);
                    aev.new_routes = rslt.second.size() > 0 ? &(rslt.second) : NULL;
                    aev.replaced_entries = changed_entries.size() > 0 ? &changed_entries : NULL;
                    aev.shared_attribs = &attrs;
                    if (ibgp) aev.ibgp_peer_asn = peer_asn;
                    config.rev_bus->publish(this, aev);
                }

                if (rev_bus_exist && unreach.size() > 0) {
                    logger->log(DEBUG, "BgpFsm::fsmEvalEstablished: publishing dropped v6 routes on event bus...\n");
                    Route6WithdrawEvent wev = Route6WithdrawEvent();
                    wev.routes = &unreach;
                    config.rev_bus->publish(this, wev);
                }
            }
        }
    }

    return 1;
}

void BgpFsm::dropAllRoutes() {
    if (peer_bgp_id != 0) {
        std::pair<std::vector<Prefix4>, std::vector<BgpRib4Entry>> rslt4 = rib4->discard(peer_bgp_id);
        if (rev_bus_exist && rslt4.first.size() > 0) {
            Route4WithdrawEvent wev;
            wev.routes = &(rslt4.first);
            config.rev_bus->publish(this, wev);
        }
        if (rev_bus_exist && rslt4.second.size() > 0) {
            Route4AddEvent aev;
            aev.replaced_entries = &(rslt4.second);
            config.rev_bus->publish(this, aev);
        }
        std::pair<std::vector<Prefix6>, std::vector<BgpRib6Entry>> rslt6 = rib6->discard(peer_bgp_id);
        if (rev_bus_exist && rslt6.first.size() > 0) {
            Route6WithdrawEvent wev;
            wev.routes = &(rslt6.first);
            config.rev_bus->publish(this, wev);
        }
        if (rev_bus_exist && rslt6.second.size() > 0) {
            Route6AddEvent aev;
            aev.replaced_entries = &(rslt6.second);
            config.rev_bus->publish(this, aev);
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

bool BgpFsm::validAddr4(uint32_t addr) const {
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

bool BgpFsm::validAddr6(const uint8_t addr[16]) const {
    static Prefix6 bad_range("0000::", 8);

    return !bad_range.includes(addr);
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
