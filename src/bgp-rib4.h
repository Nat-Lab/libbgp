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
#include <memory>
#include <mutex>
#include "route4.h"
#include "bgp-path-attrib.h"

namespace libbgp {

/**
 * @brief The BgpRib4Entry class.
 * 
 */
class BgpRib4Entry {
public:
    BgpRib4Entry (Route4 r, uint32_t src, const std::vector<std::shared_ptr<BgpPathAttrib>> attribs);

    /**
     * @brief The prefix of this entry.
     * 
     */
    Route4 route;

    /**
     * @brief The originating BGP speaker's ID of this entry. (network bytes order)
     * 
     */
    uint32_t src_router_id;

    /**
     * @brief The update ID. BgpRib4Entry with same update ID are received from
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

    // get nexthop of this entry.
    uint32_t getNexthop() const;
};

/**
 * @brief The BgpRib4 (IPv4 BGP Routing Information Base) class.
 * 
 */
class BgpRib4 {
public:
    BgpRib4(BgpLogHandler *logger);

    // insert a route as local routing information
    const BgpRib4Entry* insert(BgpLogHandler *logger, const Route4 &route, uint32_t nexthop);

    // insert a new route into RIB, return true if success.
    bool insert(uint32_t src_router_id, const Route4 &route, const std::vector<std::shared_ptr<BgpPathAttrib>> &attrib);

    // insert new routes into RIB, return number of routes inserted on success,
    // -1 on error.
    ssize_t insert(uint32_t src_router_id, const std::vector<Route4> &routes, const std::vector<std::shared_ptr<BgpPathAttrib>> &attrib);

    // remove a route from RIB, return true if route removed, false if not exist.
    bool withdraw(uint32_t src_router_id, const Route4 &route);

    // remove routes from RIB, return number of routes removed on success, -1
    // on error
    ssize_t withdraw(uint32_t src_router_id, const std::vector<Route4> &routes);

    // remove all routes from a peer, return all discarded routes on success.
    std::vector<Route4> discard(uint32_t src_router_id);

    // lookup in rib, return null if not found
    const BgpRib4Entry* lookup(uint32_t dest) const;

    // scoped lookup in rib, return null if not found
    const BgpRib4Entry* lookup(uint32_t src_router_id, uint32_t dest) const;

    // get RIB
    const std::vector<BgpRib4Entry> &get() const;
private:

    static const BgpRib4Entry* selectEntry(const BgpRib4Entry *a, const BgpRib4Entry *b);
    std::vector<BgpRib4Entry> rib;
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