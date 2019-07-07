#ifndef ROUTE_EV_H_
#define ROUTE_EV_H_
#include <vector>
#include "bgp-path-attrib.h"
#include "bgp-update-message.h"

namespace libbgp {

enum RouteEventType {
    ADD, 
    WITHDRAW,
    COLLISION
};

class RouteEvent {
public:
    RouteEventType type;
    virtual ~RouteEvent() {}
};

class RouteAddEvent : public RouteEvent {
public:
    RouteAddEvent () { type = ADD; }
    std::vector<std::shared_ptr<BgpPathAttrib>> attribs;
    std::vector<Route> routes;
};

class RouteWithdrawEvent : public RouteEvent {
public:
    RouteWithdrawEvent () { type = WITHDRAW; }
    std::vector<Route> routes;
};

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