/**
 * @file route-event.h
 * @author Nato Morichika <nat@nat.moe>
 * @brief The route events.
 * @version 0.1
 * @date 2019-07-07
 * 
 * @copyright Copyright (c) 2019
 * 
 */
#ifndef ROUTE_EV_H_
#define ROUTE_EV_H_
#include <vector>
#include "bgp-path-attrib.h"
#include "bgp-update-message.h"

namespace libbgp {

/* forward */
class BgpRib4Entry;
class BgpRib6Entry;

/**
 * @brief Type of route events.
 * 
 */
enum RouteEventType {
    ADD4, 
    WITHDRAW4,
    ADD6,
    WITHDRAW6,
    COLLISION
};

/**
 * @brief The RouteEvent base.
 * 
 */
class RouteEvent {
public:

    /**
     * @brief Type of this event.
     * 
     */
    RouteEventType type;
    virtual ~RouteEvent() {}
};

/**
 * @brief An Route4AddEvent.
 * 
 */
class Route4AddEvent : public RouteEvent {
public:
    Route4AddEvent () { 
        type = ADD4;
        ibgp_peer_asn = 0; 
        shared_attribs = NULL;
        new_routes = NULL;
        replaced_entries = NULL;
    }

    /**
     * @brief Shared attributes for the new routes
     * 
     */
    const std::vector<std::shared_ptr<BgpPathAttrib>> *shared_attribs;

    /**
     * @brief Newly added routes
     * 
     */
    const std::vector<Prefix4> *new_routes;

    /**
     * @brief Pointer to the route replacement entries vector.
     * 
     */
    const std::vector<BgpRib4Entry> *replaced_entries;

    /**
     * @brief ASN of the IBGP peer if the originating session is a IBGP session.
     * 
     * ASN of the IBGP peer. If the originating session is not IBGP, 
     * ibgp_peer_asn will be 0;
     */
    uint32_t ibgp_peer_asn;
};

/**
 * @brief An Route4WithdrawEvent. 
 * 
 */
class Route4WithdrawEvent : public RouteEvent {
public:
    Route4WithdrawEvent () { 
        type = WITHDRAW4; 
        routes = NULL;
    }

    /**
     * @brief Routes to withdraw.
     * 
     */
    std::vector<Prefix4> *routes;
};

/**
 * @brief An Route6AddEvent.
 * 
 */
class Route6AddEvent : public RouteEvent {
public:
    Route6AddEvent () { 
        type = ADD6; 
        ibgp_peer_asn = 0;
        shared_attribs = NULL; 
        new_routes = NULL;
        replaced_entries = NULL;
    }

    /**
     * @brief Path attribues of the route.
     * 
     */
    std::vector<std::shared_ptr<BgpPathAttrib>> *shared_attribs;

    /**
     * @brief Routes to add.
     * 
     */
    std::vector<Prefix6> *new_routes;

    /**
     * @brief Pointer to the route replacement entries vector.
     * 
     */
    const std::vector<BgpRib6Entry> *replaced_entries;

    /**
     * @brief Global IPv6 nexthop.
     * 
     */
    uint8_t nexthop_global[16];

    /**
     * @brief Link-local IPv6 nexthop.
     * 
     */
    uint8_t nexthop_linklocal[16];

    /**
     * @brief ASN of the IBGP peer if the originating session is a IBGP session.
     * 
     * ASN of the IBGP peer. If the originating session is not IBGP, 
     * ibgp_peer_asn will be 0;
     */
    uint32_t ibgp_peer_asn;
};

/**
 * @brief An Route6WithdrawEvent. 
 * 
 */
class Route6WithdrawEvent : public RouteEvent {
public:
    Route6WithdrawEvent () { type = WITHDRAW6; }

    /**
     * @brief Routes to withdraw.
     * 
     */
    std::vector<Prefix6> *routes;
};

/**
 * @brief Probe for collision detection.
 * 
 * When a BgpFsm receive RouteCollisionEvent, it will check if the peer BGP ID
 * is same as it's peer. If it is, collision resloution will be done. If someone
 * reported event handled, the sender of the event will go to IDLE.
 */
class RouteCollisionEvent : public RouteEvent {
public:
    RouteCollisionEvent () { type = COLLISION; }
    uint32_t peer_bgp_id;
};

/** 
 * @example route-event-bus.cc
 * Example of adding new routes to RIB while BGP FSM is running. Notify BGP FSM
 * to send updates to the peer with RouteEventBus. This example also shows how
 * you can implement your own BgpOutHandler and BgpLogHandler.
 * 
 */

}

#endif // ROUTE_EV_H_