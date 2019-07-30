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

/**
 * @brief Type of route events.
 * 
 */
enum RouteEventType {
    ADD4, 
    WITHDRAW4,
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
    Route4AddEvent () { type = ADD4; }

    /**
     * @brief Path attribues of the route.
     * 
     */
    std::vector<std::shared_ptr<BgpPathAttrib>> attribs;

    /**
     * @brief Routes to add.
     * 
     */
    std::vector<Prefix4> routes;
};

/**
 * @brief An Route4WithdrawEvent. 
 * 
 */
class Route4WithdrawEvent : public RouteEvent {
public:
    Route4WithdrawEvent () { type = WITHDRAW4; }

    /**
     * @brief Routes to withdraw.
     * 
     */
    std::vector<Prefix4> routes;
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