/**
 * @file bgp-rib4.h
 * @author Nato Morichika <nat@nat.moe>
 * @brief The IPv4 BGP Routing Information Base.
 * @version 0.2
 * @date 2019-07-21
 * 
 * @copyright Copyright (c) 2019
 * 
 */
#ifndef RIB_H_
#define RIB_H_
#include <stdint.h>
#include <vector>
#include <unordered_map>
#include <tuple>
#include <memory>
#include <mutex>
#include "bgp-rib.h"
#include "prefix4.h"
#include "bgp-path-attrib.h"

namespace libbgp {

/**
 * @brief Key for the Rib4 entry map.
 * 
 */
class BgpRib4EntryKey {
public:
    BgpRib4EntryKey() {}
    BgpRib4EntryKey(const Prefix4 &prefix) {
        this->prefix = prefix.getPrefix();
        this->length = prefix.getLength();
        hash = this->prefix | (this->length << 4);
    }
    BgpRib4EntryKey(uint32_t prefix, uint32_t length) {
        this->prefix = prefix;
        this->length = length;
        hash = this->prefix | (this->length << 4);
    }

    bool operator== (const BgpRib4EntryKey &other) const {
        return prefix == other.prefix && length == other.length;
    }

    uint64_t hash;
    uint32_t prefix;
    uint8_t length;
};

/**
 * @brief Hasher for the Rib4 entry key.
 * 
 */
struct BgpRib4EntryHash {
    std::size_t operator()(const BgpRib4EntryKey &key) const {
        return key.hash;
    }
};

/**
 * @brief The BgpRib4Entry class.
 * 
 */
class BgpRib4Entry : public BgpRibEntry<BgpRib4Entry> {
public:
    BgpRib4Entry ();
    BgpRib4Entry (Prefix4 r, uint32_t src, const std::vector<std::shared_ptr<BgpPathAttrib>> attribs);

    /**
     * @brief The prefix of this entry.
     * 
     */
    Prefix4 route;

    // get nexthop of this entry.
    uint32_t getNexthop() const;
};

typedef std::unordered_multimap<BgpRib4EntryKey, BgpRib4Entry, BgpRib4EntryHash> rib4_t;

/**
 * @brief The BgpRib4 (IPv4 BGP Routing Information Base) class.
 * 
 */
class BgpRib4 : private BgpRib<BgpRib4Entry> {
public:
    BgpRib4(BgpLogHandler *logger);

    // insert a route as local routing information base. This MUST NOT be called when FSM is running.
    const BgpRib4Entry* insert(BgpLogHandler *logger, const Prefix4 &route, uint32_t nexthop, int32_t weight = 0);
    const std::vector<BgpRib4Entry> insert(BgpLogHandler *logger, const std::vector<Prefix4> &routes, uint32_t nexthop, int32_t weight = 0);

    // insert a new route into RIB, return BgpRib4Entry that should be send to other peers.
    // <NULL, false> if a better route is already exist
    // <BgpRib4Entry*, false> if inserted route replaced current best route, and another route become the new best
    // <BgpRib4Entry*, true> if inserted route become the new best route
    std::pair<const BgpRib4Entry*, bool> insert(uint32_t src_router_id, const Prefix4 &route, const std::vector<std::shared_ptr<BgpPathAttrib>> &attrib, int32_t weight,  uint32_t ibgp_asn);

    // insert new routes w/ common attribs.
    // returns a pair: <updated_routes, new_best_routes> where updated_routes is a vector
    // containing routes with different attribute then provided.
    std::pair<std::vector<BgpRib4Entry>, std::vector<Prefix4>> insert(uint32_t src_router_id, const std::vector<Prefix4> &routes, const std::vector<std::shared_ptr<BgpPathAttrib>> &attrib, int32_t weight,  uint32_t ibgp_asn);

    // remove a route from RIB
    std::pair<bool, const void*> withdraw(uint32_t src_router_id, const Prefix4 &route);

    // remove all routes from a peer, return <unreachabled routes, updated_routes>.
    std::pair<std::vector<Prefix4>, std::vector<BgpRib4Entry>> discard(uint32_t src_router_id);

    // lookup in rib, return null if not found
    const BgpRib4Entry* lookup(uint32_t dest) const;

    // scoped lookup in rib, return null if not found
    const BgpRib4Entry* lookup(uint32_t src_router_id, uint32_t dest) const;

    // get RIB
    const rib4_t &get() const;
private:
    rib4_t::iterator find_best (const Prefix4 &prefix);
    rib4_t::iterator find_entry (const Prefix4 &prefix, uint32_t src);
    std::pair<const BgpRib4Entry*, bool> insertPriv(uint32_t src_router_id, const Prefix4 &route, const std::vector<std::shared_ptr<BgpPathAttrib>> &attrib, int32_t weight, uint32_t ibgp_asn);
    rib4_t rib;
    std::recursive_mutex mutex;
    BgpLogHandler *logger;
    uint64_t update_id;
};

/**
 * @example peer-and-print.cc
 * A simple BGP speaker listen on TCP 0.0.0.0:179, wait for a peer, and print 
 * all BGP messages sent/received with BgpFsm. 
 * 
 * @example route-event-bus.cc
 * Example of adding new routes to RIB while BGP FSM is running. Notify BGP FSM
 * to send updates to the peer with RouteEventBus. This example also shows how
 * you can implement your own BgpOutHandler and BgpLogHandler.
 * 
 * @example route-server.cc
 * Simple BGP route server implements with libbgp. Use of RouteEventBus and 
 * shared BgpRib4 is demoed in this example. This example also shows how you can
 * implement your own BgpLogHandler.
 * 
 */

}
#endif // RIB_H_