#ifndef ROUTE_EV_RECV_H_
#define ROUTE_EV_RECV_H_
#include "route-event-bus.h"

namespace libbgp {

class RouteEventBus;

class RouteEventReceiver {
    friend class RouteEventBus;

public:
    RouteEventReceiver() { subscription_id = 0; }
    virtual ~RouteEventReceiver() {};

protected:

    // handle a route event
    // return:
    // true: event handled
    // false: event not handled
    virtual bool handleRouteEvent(const RouteEvent &ev) = 0;
private:
    int subscription_id;
};

/** 
 * @example route-event-bus.cc
 * Example of adding new routes to RIB while BGP FSM is running. Notify BGP FSM
 * to send updates to the peer with RouteEventBus. This example also shows how
 * you can implement your own BgpOutHandler and BgpLogHandler.
 * 
 */

}

#endif // ROUTE_EV_RECV_H_