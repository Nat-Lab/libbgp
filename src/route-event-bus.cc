/**
 * @file route-event-bus.cc
 * @author Nato Morichika <nat@nat.moe>
 * @brief The route event bus.
 * @version 0.1
 * @date 2019-07-07
 * 
 * @copyright Copyright (c) 2019
 * 
 */
#include "route-event-bus.h"

namespace libbgp {

RouteEventBus::RouteEventBus() {
    subscription_id = 0;
}

/**
 * @brief Publish a route event. 
 * 
 * @param recv Pointer to the RouteEventReceiver object that published the
 * event. It is for ensuring publisher of the event does not receive the event 
 * it published itself. Can be NULL.
 * @param ev The event to publish;
 * @return int Number of subscribers report event received.
 */
int RouteEventBus::publish(RouteEventReceiver *recv, const RouteEvent &ev) {
    int n = 0;

    for (RouteEventReceiver* &subscriber : subscribers) {
        if (recv == NULL || subscriber->subscription_id != recv->subscription_id) {
            if (subscriber->handleRouteEvent(ev)) n++;
        }
    }

    return n;
}

/**
 * @brief Subscribe from this event bus.
 * 
 * @param recv The receiver.
 * @return true Subscribed.
 * @return false Failed to subscribe.
 */
bool RouteEventBus::subscribe(RouteEventReceiver *recv) {
    recv->subscription_id = ++subscription_id;
    subscribers.push_back(recv);
    return true;
}

/**
 * @brief Unsubscribe to this event bus.
 * 
 * @param recv The receiver.
 * @return true Unsubscribed.
 * @return false Failed to Unsubscribe.
 */
bool RouteEventBus::unsubscribe(RouteEventReceiver *recv) {
    for (auto it = subscribers.begin(); it != subscribers.end(); it++) {
        if ((*it)->subscription_id == recv->subscription_id) {
            subscribers.erase(it);
            return true;
        }
    }

    return false;
}

}
