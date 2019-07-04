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

    // disable collision detection? collision detection is done by sending 
    // "collision" event to event bus upon reception of an open message.
    bool no_collision_detection;

    // use 4 byte ASN?
    // false: don't send 4B ASN capability.
    // true: send 4B ASN capability.
    // this will also change behaviour of updates
    bool use_4b_asn;

    // local ASN, MUST be < 65535 if use_4b_asn != 1
    uint32_t asn;

    // peer ASN, MUST be < 65535 if use_4b_asn != 1
    uint32_t peer_asn;

    // BGP ID in network byte order
    uint32_t router_id;

    // when processing update from the peer, routes with nexthop outside the 
    // network specified by peering_lan_prefix and peering_lan_length will be
    // ignored. These two values also affect the nexthop selection behavior
    // when sending updates to the peer.

    // the prefix of the peering LAN in network-byte order.
    uint32_t peering_lan_prefix;

    // the mask of the peering LAN. (e.g., for /24, peering_lan_length should 
    // be 24)
    uint8_t peering_lan_length;

    // nexthop to use when advertising routes to peers. the peering_lan_prefix 
    // and peering_lan_length value will be used for nexthop selection. If an 
    // egress route has a nexthop inside the peering LAN, the nexthop will not
    // be changed. Otherwise, this value will be used as nexthop.
    uint32_t nexthop;

    // Hold timer
    uint16_t hold_timer;

    // pointer to clock object, if NULL, real time clock will be used
    Clock *clock;
} BgpConfig;

}

#endif // BGP_CONFIG_H_ 