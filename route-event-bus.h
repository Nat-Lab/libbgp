#ifndef ROUTE_EV_BUS_H_
#define ROUTE_EV_BUS_H_
#include "route-event.h"
#include "bgp-fsm.h"
#include <vector>
namespace bgpfsm {

class BgpFsm;

class RouteEventBus {
public:
    // publish a route event. For non FSM (administratively/other proto), use fsm = NULL
    // return number of subscriber reached, or -1 on error
    int publish(BgpFsm *fsm, RouteEvent ev);

    // subscribe to event bus, return true if success
    bool subscribe(BgpFsm *fsm);

    // unsubscribe from event bus, return true if success
    bool unsubscribe(BgpFsm *fsm);
private:
    std::vector<BgpFsm *> subscribers;
};

}

#endif // ROUTE_EV_BUS_H_