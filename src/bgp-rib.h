/**
 * @file bgp-rib.h
 * @author Nato Morichika <nat@nat.moe>
 * @brief The BGP Routing Information Base.
 * @version 0.1
 * @date 2019-07-04
 * 
 * @copyright Copyright (c) 2019
 * 
 */
#ifndef RIB_H_
#define RIB_H_
#include <stdint.h>
#include <vector>
#include <memory>
#include "route.h"
#include "bgp-path-attrib.h"

namespace libbgp {

/**
 * @brief The BgpRibEntry class.
 * 
 */
class BgpRibEntry {
public:
    BgpRibEntry (Route r, uint32_t src, const std::vector<std::shared_ptr<BgpPathAttrib>> attribs);

    /**
     * @brief The prefix of this entry.
     * 
     */
    Route route;

    /**
     * @brief The originating BGP speaker's ID of this entry. (network bytes order)
     * 
     */
    uint32_t src_router_id;

    /**
     * @brief Path attributes for this entry.
     * 
     */
    std::vector<std::shared_ptr<BgpPathAttrib>> attribs;

    // compute metric based on path attribute. (currently based on AS_PATH only)
    uint32_t getMetric() const;
};

/**
 * @brief The BgpRib (BGP Routing Information Base) class.
 * 
 */
class BgpRib {
public:
    // insert a new route into RIB, return true if success.
    bool insert(uint32_t src_router_id, const Route &route, const std::vector<std::shared_ptr<BgpPathAttrib>> &attrib);

    // insert new routes into RIB, return number of routes inserted on success,
    // -1 on error.
    ssize_t insert(uint32_t src_router_id, const std::vector<Route> &routes, const std::vector<std::shared_ptr<BgpPathAttrib>> &attrib);

    // remove a route from RIB, return true if route removed, false if not exist.
    bool withdraw(uint32_t src_router_id, const Route &route);

    // remove all routes from a peer, return number of routes remvoed on success, 
    // -1 on error.
    ssize_t discard(uint32_t src_router_id);

    // lookup in rib, return null if not found
    const BgpRibEntry* lookup(uint32_t dest) const;

    // scoped lookup in rib, return null if not found
    const BgpRibEntry* lookup(uint32_t src_router_id, uint32_t dest) const;

    // get RIB
    const std::vector<BgpRibEntry> &get() const;
private:

    static const BgpRibEntry* selectEntry(const BgpRibEntry *a, const BgpRibEntry *b);
    std::vector<BgpRibEntry> rib;
};

}
#endif // RIB_H_