#ifndef RIB_H_
#define RIB_H_
#include <stdint.h>
#include <vector>
#include <memory>
#include "protocol/bgp-path-attrib.h"

namespace bgpfsm {

typedef struct BgpRibEntry {
    Route route;
    uint32_t src_router_id;

    std::vector<std::shared_ptr<BgpPathAttrib>> attribs;
} RibEntry;

class BgpRib {
public:
    // insert a new route into RIB
    int insert(uint32_t src_router_id, const Route &route, const std::vector<std::shared_ptr<BgpPathAttrib>> &attrib);

    // insert new routes into RIB
    int insert(uint32_t src_router_id, const std::vector<Route> &route, const std::vector<std::shared_ptr<BgpPathAttrib>> &attrib);

    // remove a route from RIB
    int withdraw(uint32_t src_router_id, const Route &route);

    // remove all routes from a peer
    int discard(uint32_t src_router_id);

    // get RIB
    const std::vector<BgpRibEntry> &get() const;
private:
    std::vector<BgpRibEntry> rib;
};

}
#endif // RIB_H_