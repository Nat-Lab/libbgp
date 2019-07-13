/**
 * @file route-event-bus.h
 * @author Nato Morichika <nat@nat.moe>
 * @brief The route event bus.
 * @version 0.1
 * @date 2019-07-07
 * 
 * @copyright Copyright (c) 2019
 * 
 */
#ifndef ROUTE_EV_BUS_H_
#define ROUTE_EV_BUS_H_
#include "route-event.h"
#include "route-event-receiver.h"
#include <vector>
namespace libbgp {

class RouteEventReceiver;

/**
 * @brief The RouteEventBus class.
 * 
 * The route event bus is used to share information and communicate with other 
 * BGP FSMs. For example, route add/withdrawn events are sent to other FSMs with
 * route event bus. Collision resolution also depends on the route event bus. 
 */
class RouteEventBus {
public:
    RouteEventBus();

    // publish a route event. For non FSM (administratively/other proto), use fsm = NULL
    // return number of subscriber reached, or -1 on error
    int publish(RouteEventReceiver *recv, const RouteEvent &ev);

    // subscribe to event bus, return true if success
    bool subscribe(RouteEventReceiver *recv);

    // unsubscribe from event bus, return true if success
    bool unsubscribe(RouteEventReceiver *recv);
private:
    std::vector<RouteEventReceiver *> subscribers;
    int subscription_id;
};

/** 
 * @example route-event-bus.cc
 * Example of adding new routes to RIB while BGP FSM is running. Notify BGP FSM
 * to send updates to the peer with RouteEventBus. This example also shows how
 * you can implement your own BgpOutHandler and BgpLogHandler.
 * 
 * @example route-server.cc
 * Simple BGP route server implements with libbgp. Use of RouteEventBus and 
 * shared BgpRib is demoed in this example. This example also shows how you can
 * implement your own BgpLogHandler.
 */

}

#endif // ROUTE_EV_BUS_H_