/**
 * @file bgp-rib6.h
 * @author Nato Morichika <nat@nat.moe>
 * @brief The BGP Routing Information Base for IPv6.
 * @version 0.1
 * @date 2019-07-21
 * 
 * @copyright Copyright (c) 2019
 * 
 */
#ifndef RIB6_H_
#define RIB6_H_
#include <stdint.h>
#include <vector>
#include <memory>
#include <mutex>
#include "route6.h"
#include "bgp-path-attrib.h"

namespace libbgp {

/**
 * @brief The BgpRib6Entry class.
 * 
 */
class BgpRib6Entry {
public:
    BgpRib6Entry (Route6 r, uint32_t src, const uint8_t nexthop_global[16], 
        const uint8_t nexthop_linklocal[16], 
        const std::vector<std::shared_ptr<BgpPathAttrib>> attribs);

    /**
     * @brief The prefix of this entry.
     * 
     */
    Route6 route;

    /**
     * @brief Global IPv6 address of the next hop in network btyes order.
     * 
     */
    uint8_t nexthop_global[16];

    /**
     * @brief Link local IPv6 address of the next hop in network btyes order.
     * (all 0 if not avaliable)
     * 
     */
    uint8_t nexthop_linklocal[16];

    /**
     * @brief The originating BGP speaker's ID of this entry. (network bytes order)
     * 
     */
    uint32_t src_router_id;

    /**
     * @brief The update ID. BgpRibEntry with same update ID are received from
     * the same update and their path attributes are therefore same. Note that
     * entries with different update_id may still have same path attributes.
     * 
     */
    uint64_t update_id;

    /**
     * @brief Path attributes for this entry.
     * 
     */
    std::vector<std::shared_ptr<BgpPathAttrib>> attribs;

    // compute metric based on path attribute. (currently based on AS_PATH only)
    uint32_t getMetric() const;
};

/**
 * @brief The BgpRib6 (IPv6 BGP Routing Information Base) class.
 * 
 */
class BgpRib6 {
public:
    BgpRib6(BgpLogHandler *logger);

    // insert a route as local routing information
    const BgpRib6Entry* insert(BgpLogHandler *logger, 
        const Route6 &route, const uint8_t nexthop_global[16], 
        const uint8_t nexthop_linklocal[16]);

    // insert a new route into RIB, return true if success.
    bool insert(uint32_t src_router_id, const Route6 &route, 
        const uint8_t nexthop_global[16], const uint8_t nexthop_linklocal[16], 
        const std::vector<std::shared_ptr<BgpPathAttrib>> &attrib);

    // insert new routes into RIB, return number of routes inserted on success,
    // -1 on error.
    ssize_t insert(uint32_t src_router_id, const std::vector<Route6> &routes, 
        const uint8_t nexthop_global[16], const uint8_t nexthop_linklocal[16], 
        const std::vector<std::shared_ptr<BgpPathAttrib>> &attrib);

    // remove a route from RIB, return true if route removed, false if not exist.
    bool withdraw(uint32_t src_router_id, const Route6 &route);

    // remove routes from RIB, return number of routes removed on success, -1
    // on error
    ssize_t withdraw(uint32_t src_router_id, const std::vector<Route6> &routes);

    // remove all routes from a peer, return all discarded routes on success.
    std::vector<Route6> discard(uint32_t src_router_id);

    // lookup in rib, return null if not found
    const BgpRib6Entry* lookup(const uint8_t dest[16]) const;

    // scoped lookup in rib, return null if not found
    const BgpRib6Entry* lookup(uint32_t src_router_id, const uint8_t dest[16]) const;

    // get RIB
    const std::vector<BgpRib6Entry> &get() const;
private:

    static const BgpRib6Entry* selectEntry(const BgpRib6Entry *a, const BgpRib6Entry *b);
    std::vector<BgpRib6Entry> rib;
    std::recursive_mutex mutex;
    BgpLogHandler *logger;
    uint64_t update_id;
};

}
#endif // RIB6_H_