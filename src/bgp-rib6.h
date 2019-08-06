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
#include <string.h>
#include <vector>
#include <unordered_map>
#include <memory>
#include <mutex>
#include "bgp-rib.h"
#include "prefix6.h"
#include "bgp-path-attrib.h"
#include "route-event-bus.h"

namespace libbgp {

/**
 * @brief Key for the Rib6 entry map.
 * 
 */
class BgpRib6EntryKey {
public:
    BgpRib6EntryKey() {}
    BgpRib6EntryKey(const Prefix6 &prefix, uint32_t src) {
        prefix.getPrefix(this->prefix);
        length = prefix.getLength();
        this->src = src;

        uint64_t prefix_pre = 0;
        memcpy(&prefix_pre, this->prefix, 8);
        hash = (prefix_pre ^ length) | src << 4;
    }

    bool operator== (const BgpRib6EntryKey &other) const {
        return memcmp(other.prefix, prefix, 16) == 0 && 
            length == other.length && src == other.src;
    }

    uint8_t prefix[16];
    uint8_t length;
    uint32_t src;
    uint64_t hash;
};

/**
 * @brief Hasher for the Rib6 entry key.
 * 
 */
struct BgpRib6EntryHash {
    std::size_t operator()(const BgpRib6EntryKey &key) const {
        return key.hash;
    }
};

/**
 * @brief The BgpRib6Entry class.
 * 
 */
class BgpRib6Entry : public BgpRibEntry<BgpRib6Entry> {
public:
    BgpRib6Entry (Prefix6 r, uint32_t src, const uint8_t nexthop_global[16], 
        const uint8_t nexthop_linklocal[16], 
        const std::vector<std::shared_ptr<BgpPathAttrib>> attribs);

    /**
     * @brief The prefix of this entry.
     * 
     */
    Prefix6 route;

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
};

typedef std::unordered_multimap<BgpRib6EntryKey, BgpRib6Entry, BgpRib6EntryHash> rib6_t;

/**
 * @brief The BgpRib6 (IPv6 BGP Routing Information Base) class.
 * 
 */
class BgpRib6 : BgpRib<BgpRib6Entry> {
public:
    BgpRib6(BgpLogHandler *logger);

    // insert a route as local routing information
    const BgpRib6Entry* insert(BgpLogHandler *logger, 
        const Prefix6 &route, const uint8_t nexthop_global[16], 
        const uint8_t nexthop_linklocal[16], int32_t weight = 0);

    const BgpRib6Entry* insert(BgpLogHandler *logger, 
        const Prefix6 &route, const uint8_t nexthop_global[16], 
        const uint8_t nexthop_linklocal[16], RouteEventBus *rev_bus,
        int32_t weight = 0);

    const std::vector<BgpRib6Entry> insert(BgpLogHandler *logger, 
        const std::vector<Prefix6> &routes, const uint8_t nexthop_global[16], 
        const uint8_t nexthop_linklocal[16], int32_t weight = 0);

    const std::vector<BgpRib6Entry> insert(BgpLogHandler *logger, 
        const std::vector<Prefix6> &routes, const uint8_t nexthop_global[16], 
        const uint8_t nexthop_linklocal[16], RouteEventBus *rev_bus,
        int32_t weight = 0);

    // insert a new route into RIB, return true if success.
    bool insert(uint32_t src_router_id, const Prefix6 &route, 
        const uint8_t nexthop_global[16], const uint8_t nexthop_linklocal[16], 
        const std::vector<std::shared_ptr<BgpPathAttrib>> &attrib, int32_t weight);

    // insert new routes into RIB, return number of routes inserted on success,
    // -1 on error.
    ssize_t insert(uint32_t src_router_id, const std::vector<Prefix6> &routes, 
        const uint8_t nexthop_global[16], const uint8_t nexthop_linklocal[16], 
        const std::vector<std::shared_ptr<BgpPathAttrib>> &attrib, int32_t weight);

    // remove a route from RIB, return true if route removed, false if not exist.
    bool withdraw(uint32_t src_router_id, const Prefix6 &route);

    // remove routes from RIB, return number of routes removed on success, -1
    // on error
    ssize_t withdraw(uint32_t src_router_id, const std::vector<Prefix6> &routes);

    // remove all routes from a peer, return all discarded routes on success.
    std::vector<Prefix6> discard(uint32_t src_router_id);

    // lookup in rib, return null if not found
    const BgpRib6Entry* lookup(const uint8_t dest[16]) const;

    // scoped lookup in rib, return null if not found
    const BgpRib6Entry* lookup(uint32_t src_router_id, const uint8_t dest[16]) const;

    // get RIB
    const rib6_t &get() const;
private:
    bool insertPriv(uint32_t src_router_id, const Prefix6 &route, 
        const uint8_t nexthop_global[16], const uint8_t nexthop_linklocal[16], 
        const std::vector<std::shared_ptr<BgpPathAttrib>> &attrib, int32_t weight);

    rib6_t rib;
    std::recursive_mutex mutex;
    BgpLogHandler *logger;
    uint64_t update_id;
};

}
#endif // RIB6_H_