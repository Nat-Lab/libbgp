#include "route-event-bus.h"

namespace libbgp
{

int RouteEventBus::publish(RouteEventReceiver *recv, const RouteEvent ev) {
    int n = 0;

    for (RouteEventReceiver* &subscriber : subscribers) {
        if (subscriber != recv) {
            if (recv->handleRouteEvent(ev)) n++;
        }
    }

    return n;
}

bool RouteEventBus::subscribe(RouteEventReceiver *recv) {
    for (RouteEventReceiver* &subscriber : subscribers) {
        if (recv == subscriber) return false;
    }

    subscribers.push_back(recv);
    return true;
}

bool RouteEventBus::unsubscribe(RouteEventReceiver *recv) {
    for (auto it = subscribers.begin(); it != subscribers.end(); it++) {
        if (*it == recv) {
            subscribers.erase(it);
            return true;
        }
    }

    return false;
}

}
