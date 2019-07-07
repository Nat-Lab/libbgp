/**
 * @file route-event-receiver.h
 * @author Nato Morichika <nat@nat.moe>
 * @brief Route event receiver.
 * @version 0.1
 * @date 2019-07-07
 * 
 * @copyright Copyright (c) 2019
 * 
 */
#ifndef ROUTE_EV_RECV_H_
#define ROUTE_EV_RECV_H_
#include "route-event-bus.h"

namespace libbgp {

class RouteEventBus;

/**
 * @brief The RouteEventReceiver interface.
 * 
 * RouteEventReceiver is an interface for RouteEventBus participant. 
 * RouteEventBus is usually used by BGP FSMs to communicate with each other. 
 * (since every FSM handle a single BGP session, and there might be multiple BGP
 * sessions running at a time) RouteEventBus allow BGP FSMs to pass route 
 * add/withdrawn updates to other FSMs. (collision detection is also done 
 * through RouteEventBus)
 * 
 * You may implement your own RouteEventReceiver and subscribe it to a event
 * bus to get notifed when route changes.
 */
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

    /**
     * @brief Handle the route event.
     * 
     * @param ev The event.
     * @return true Event handled.
     * @return false Event not handled.
     */
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