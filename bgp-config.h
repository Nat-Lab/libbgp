#ifndef BGP_CONFIG_H_
#define BGP_CONFIG_H_
#include <stdint.h>
#include "clock.h"
#include "bgp-rib.h"
#include "bgp-filter.h"
#include "bgp-out-handler.h"
#include "route-event-bus.h"

namespace bgpfsm {

typedef struct BgpConfig {
    // ingress route filters
    BgpFilterRules in_filters;

    // egress route filters
    BgpFilterRules out_filters;

    // pointer to output handler
    BgpOutHandler *out_handler;

    // pointer to RIB, you can share multiple RIB across different FSM, a local
    // RIB will be created if NULL
    BgpRib *rib;

    // pointer to event bus, route add/withdraw events will be sent to other
    // FSM thru event bus, won't use if NULL
    RouteEventBus *rev_bus;

    // mark the transport layer this session as connectionless.
    // marking the session as connectionless makes bgp-fsm do collision 
    // detection. normally, one connection is associated with an FSM, bgp-fsm
    // won't be able to do collision detection in this case. in connectionless
    // mode, each remote host is associated with an FSM.
    bool connectionless;

    // use 4 byte ASN?
    // false: don't send 4B ASN capability.
    // true: send 4B ASN capability.
    bool use_4b_asn;

    // local ASN, MUST be < 65535 if use_4b_asn != 1
    uint32_t asn;

    // peer ASN, MUST be < 65535 if use_4b_asn != 1
    uint32_t peer_asn;

    // BGP ID
    uint32_t router_id;

    // Hold timer
    uint16_t hold_timer;

    // pointer to clock object, if NULL, real time clock will be used
    Clock *clock;
} BgpConfig;

}

#endif // BGP_CONFIG_H_ 