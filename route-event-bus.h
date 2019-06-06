#ifndef ROUTE_EV_BUS_H_
#define ROUTE_EV_BUS_H_
#include "route-event.h"
#include "route-event-receiver.h"
#include <vector>
namespace bgpfsm {

class RouteEventReceiver;

class RouteEventBus {
public:
    // publish a route event. For non FSM (administratively/other proto), use fsm = NULL
    // return number of subscriber reached, or -1 on error
    int publish(RouteEventReceiver *recv, RouteEvent ev);

    // subscribe to event bus, return true if success
    bool subscribe(RouteEventReceiver *recv);

    // unsubscribe from event bus, return true if success
    bool unsubscribe(RouteEventReceiver *recv);
private:
    std::vector<RouteEventReceiver *> subscribers;
};

}

#endif // ROUTE_EV_BUS_H_