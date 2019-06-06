#include "route-event-bus.h"

namespace bgpfsm
{

int RouteEventBus::publish(BgpFsm *fsm, RouteEvent ev) {
    int n = 0;

    for (BgpFsm* &subscriber : subscribers) {
        if (subscriber != fsm) {
            if (fsm->handleRouteEvent(ev)) n++;
        }
    }

    return n;
}

bool RouteEventBus::subscribe(BgpFsm *fsm) {
    for (BgpFsm* &subscriber : subscribers) {
        if (fsm == subscriber) return false;
    }

    subscribers.push_back(fsm);
    return true;
}

bool RouteEventBus::unsubscribe(BgpFsm *fsm) {
    for (auto it = subscribers.begin(); it != subscribers.end(); it++) {
        if (*it == fsm) {
            subscribers.erase(it);
            return true;
        }
    }

    return false;
}

}
