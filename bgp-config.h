#ifndef BGP_CONFIG_H_
#define BGP_CONFIG_H_
#include <stdint.h>
#include "bgp-rib.h"
#include "bgp-filter.h"
#include "bgp-out-handler.h"

namespace bgpfsm {

typedef struct BgpConfig {
    BgpFilterRules in_filters;
    BgpFilterRules out_filters;
    BgpOutHandler *out_handler;
    BgpRib *rib;
    bool use_4b_asn;
    uint32_t asn;
    uint32_t peer_asn;
    uint32_t router_id;
} BgpConfig;

}

#endif // BGP_CONFIG_H_ 