#ifndef ROUTE_EV_RECV_H_
#define ROUTE_EV_RECV_H_
#include "route-event-bus.h"

namespace bgpfsm {

class RouteEventBus;

class RouteEventReceiver {
    friend class RouteEventBus;

protected:
    virtual bool handleRouteEvent (const RouteEvent ev) = 0;
    virtual ~RouteEventReceiver() {};
};

}

#endif // ROUTE_EV_RECV_H_