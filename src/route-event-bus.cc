#include "route-event-bus.h"

namespace libbgp {

RouteEventBus::RouteEventBus() {
    subscription_id = 0;
}

int RouteEventBus::publish(RouteEventReceiver *recv, const RouteEvent &ev) {
    int n = 0;

    for (RouteEventReceiver* &subscriber : subscribers) {
        if (recv == NULL || subscriber->subscription_id != recv->subscription_id) {
            if (subscriber->handleRouteEvent(ev)) n++;
        }
    }

    return n;
}

bool RouteEventBus::subscribe(RouteEventReceiver *recv) {
    recv->subscription_id = ++subscription_id;
    subscribers.push_back(recv);
    return true;
}

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
