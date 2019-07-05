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
    std::vector<std::shared_ptr<BgpPathAttrib>> attribs;
    std::vector<Route> routes;
};

class RouteWithdrawEvent : public RouteEvent {
public:
    std::vector<Route> routes;
};

class RouteCollisionEvent : public RouteEvent {
public:
    uint32_t peer_bgp_id;
};

}

#endif // ROUTE_EV_H_