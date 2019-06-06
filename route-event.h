#ifndef ROUTE_EV_H_
#define ROUTE_EV_H_
#include <vector>
#include "protocol/bgp-path-attrib.h"
#include "protocol/bgp-update-message.h"

namespace bgpfsm {

enum RouteEventType {
    ADD, 
    WITHDRAW
};

class RouteEvent {
public:
    RouteEventType type;
};

class RouteAddEvent : public RouteEvent {
public:
    std::vector<BgpPathAttrib> attribs;
    std::vector<Route> routes;
};

class RouteWithdrawEvent : public RouteEvent {
public:
    std::vector<Route> routes;
};
}

#endif // ROUTE_EV_H_