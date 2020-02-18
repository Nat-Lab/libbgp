/**
 * @file bgp-rib6.cc
 * @author Nato Morichika <nat@nat.moe>
 * @brief The IPv6 BGP Routing Information Base.
 * @version 0.1
 * @date 2019-07-21
 * 
 * @copyright Copyright (c) 2019
 * 
 */
#include <string.h>
#include <arpa/inet.h>
#include "bgp-rib6.h"
#define MAKE_ENTRY6(r, e) std::make_pair(BgpRib6EntryKey(r), e)

namespace libbgp {

/**
 * @brief Construct a new BgpRib6Entry.
 * 
 * @param r Route of this entry.
 * @param src Originating BGP speaker's ID in network bytes order.
 * @param nexthop_global Global IPv6 address of nexthop.
 * @param nexthop_linklocal Link local IPv6 address of nexthop. (if none, use NULL)
 * @param attribs Path attributes for this entry.
 */
BgpRib6Entry::BgpRib6Entry (Prefix6 r, uint32_t src, const uint8_t nexthop_global[16], 
    const uint8_t nexthop_linklocal[16], const std::vector<std::shared_ptr<BgpPathAttrib>> attribs)
    : route(r) {

    memcpy(this->nexthop_global, nexthop_global, 16);
    if (nexthop_linklocal != NULL) memcpy(this->nexthop_linklocal, nexthop_linklocal, 16);
    else memset(this->nexthop_linklocal, 0, 16);
    src_router_id = src;
    this->attribs = attribs;
}

/**
 * @brief Construct a new BgpRib6 object with logging.
 * 
 * @param logger Log handler to use.
 */
BgpRib6::BgpRib6(BgpLogHandler *logger) {
    this->logger = logger;
    update_id = 0;
}

rib6_t::iterator BgpRib6::find_entry(const Prefix6 &prefix, uint32_t src) {
    std::pair<rib6_t::iterator, rib6_t::iterator> its = 
        rib.equal_range(BgpRib6EntryKey(prefix));

    if (its.first == rib.end()) return rib.end();

    for (rib6_t::iterator it = its.first; it != its.second; it++) {
        if (it->second.route == prefix && it->second.src_router_id == src) {
            return it;
        }
    }

    return rib.end();
}

rib6_t::iterator BgpRib6::find_best (const Prefix6 &prefix) {
    std::pair<rib6_t::iterator, rib6_t::iterator> its = 
        rib.equal_range(BgpRib6EntryKey(prefix));

    rib6_t::iterator best = rib.end();
    if (its.first == rib.end()) return rib.end();

    for (rib6_t::iterator it = its.first; it != its.second; it++) {
        if (it->second.route == prefix) {
            if (best == rib.end()) best = it;
            else {
                const BgpRib6Entry *best_ptr = selectEntry(&(best->second), &(it->second));
                best = best_ptr == &(best->second) ? best : it;
            }
        }
    }

    return best;
}

std::pair<const BgpRib6Entry*, bool> BgpRib6::insertPriv(uint32_t src_router_id, 
    const Prefix6 &route, 
    const uint8_t nexthop_global[16], const uint8_t nexthop_linklocal[16], 
    const std::vector<std::shared_ptr<BgpPathAttrib>> &attribs, 
    int32_t weight, uint32_t ibgp_asn) {
    std::lock_guard<std::recursive_mutex> lock(mutex);
    BgpRib6Entry new_entry(route, src_router_id, nexthop_global, nexthop_linklocal, attribs);
    new_entry.update_id = update_id;
    new_entry.weight = weight;
    new_entry.src = ibgp_asn > 0 ? SRC_IBGP : SRC_EBGP;
    new_entry.ibgp_peer_asn = ibgp_asn;

    const char *op = "new_entry";
    const char *act = "new_best";

    std::pair<rib6_t::iterator, rib6_t::iterator> entries = rib.equal_range(BgpRib6EntryKey(route));

    bool newly_inserted_is_best = false;
    bool best_changed = false;
    bool old_exist = entries.first != rib.end();
    BgpRib6Entry *new_best = NULL;

    // older route exist
    if (old_exist) {
        // find old best & route to replace
        rib6_t::const_iterator to_replace = rib.end();
        BgpRib6Entry *old_best = NULL;
        for (rib6_t::iterator it = entries.first; it != entries.second; it++) {
            if (it->second.route != route) continue;
            if (it->second.src_router_id == src_router_id) {
                to_replace = it;
                continue;
            }
            old_best = selectEntry(old_best, &(it->second));
            //if (it->second.status == RS_ACTIVE) old_best = &(it->second);
        }

        const BgpRib6Entry *candidate = selectEntry(&new_entry, old_best);
        if (candidate == old_best) {
            new_entry.status = RS_STANDBY;
            act = "not_new_best";
        } else {
            if (old_best != NULL) old_best->status = RS_STANDBY;
            best_changed = true;
        }

        if (to_replace != rib.end()) {
            const BgpRib6Entry *candidate = selectEntry(&(to_replace->second), old_best);
            if (candidate == &(to_replace->second)) {
                // the replaced route was the best route, now it is removed
                act = "new_best";
                best_changed = true;
            }
            // we need to replace a route
            op = "update";
            rib.erase(to_replace);
        }

        rib6_t::iterator inserted = rib.insert(MAKE_ENTRY6(route, new_entry));

        if (best_changed) {
            newly_inserted_is_best = candidate == &new_entry;
            new_best = newly_inserted_is_best ? &(inserted->second) : old_best;
        }

    } else { // no older route, new one is best
        best_changed = newly_inserted_is_best = true;
        rib6_t::iterator inserted = rib.insert(MAKE_ENTRY6(route, new_entry));
        new_best = &(inserted->second);
    }

    if (new_best != NULL) new_best->status = RS_ACTIVE;
    
    LIBBGP_LOG(logger, INFO) {
        uint8_t prefix_arr[16];
        route.getPrefix(prefix_arr);
        char src_router_id_str[INET_ADDRSTRLEN], prefix_str[INET6_ADDRSTRLEN];
        inet_ntop(AF_INET, &src_router_id, src_router_id_str, INET_ADDRSTRLEN);
        inet_ntop(AF_INET6, prefix_arr, prefix_str, INET6_ADDRSTRLEN);
        logger->log(INFO, "BgpRib6::insertPriv: (%s/%s) group %d, scope %s, route %s/%d\n", op, act, new_entry.update_id, src_router_id_str, prefix_str, route.getLength());
    }

    return std::make_pair(new_best, newly_inserted_is_best);
}

/**
 * @brief Insert a local route into RIB.
 * 
 * Local routes are routes inserted to the RIB by user. The scope (src_router_id)
 * of local routes are 0. This method will create necessary path attribues
 * before inserting entry to RIB (AS_PATH, ORIGIN, NEXT_HOP). 
 * 
 * The logger pointer passed in is for attribues. (so if a attribute failed to 
 * deserialize, it will print to the provided logger).
 * 
 * To remove an entry inserted with this method, use 0 as `src_router_id`.
 * 
 * This SHOULD NOT be called when the any of the upper FSM is running. 
 * 
 * @param logger Pointer to logger for the created path attributes to use. 
 * @param route Route.
 * @param nexthop_global Global IPv6 address of nexthop.
 * @param nexthop_linklocal Link local IPv6 address of nexthop. (if none, use NULL)
 * @param weight weight of this entry.
 * @return const BgpRib6Entry* Inserted route entry.
 * @retval NULL failed to insert.
 * @retval !=NULL Inserted route entry.
 */
const BgpRib6Entry* BgpRib6::insert(BgpLogHandler *logger, const Prefix6 &route,
    const uint8_t nexthop_global[16], const uint8_t nexthop_linklocal[16], int32_t weight) {
    std::vector<std::shared_ptr<BgpPathAttrib>> attribs;
    BgpPathAttribOrigin *origin = new BgpPathAttribOrigin(logger);
    BgpPathAttribAsPath *as_path = new BgpPathAttribAsPath(logger, true);
    origin->origin = IGP;

    attribs.push_back(std::shared_ptr<BgpPathAttrib>(origin));
    attribs.push_back(std::shared_ptr<BgpPathAttrib>(as_path));

    BgpRib6Entry new_entry(route, 0, nexthop_global, nexthop_linklocal, attribs);
    new_entry.weight = weight;
    uint64_t use_update_id = update_id;

    for (const auto &entry : rib) {
        if (entry.second.src_router_id == 0 && entry.second.route == route) {
            this->logger->log(ERROR, "BgpRib6::insert: route exists.\n");
            return NULL;
        }

        // see if we can group this entry to other local entries
        if (entry.second.src_router_id == 0) {
            if (memcmp(new_entry.nexthop_global, entry.second.nexthop_global, 16) == 0 &&
                memcmp(new_entry.nexthop_linklocal, entry.second.nexthop_linklocal, 16) == 0) {
                use_update_id = entry.second.update_id;
            }
        }
    }

    std::lock_guard<std::recursive_mutex> lock(mutex);
    new_entry.update_id = use_update_id;
    if (use_update_id == update_id) update_id++;
    rib6_t::const_iterator it = rib.insert(MAKE_ENTRY6(route, new_entry));

    return &(it->second);
}

/**
 * @brief Insert local routes into RIB.
 * 
 * Same as the other local insert, but this one insert mutiple routes.
 * 
 * This SHOULD NOT be called when the any of the upper FSM is running. 
 * 
 * @param logger Pointer to logger for the created path attributes to use. 
 * @param routes Routes.
 * @param nexthop_global Global IPv6 address of nexthop.
 * @param nexthop_linklocal Link local IPv6 address of nexthop. (if none, use NULL)
 * @param weight weight of this entry.
 * @return const std::vector<BgpRib6Entry*> Insert routes.
 */
const std::vector<BgpRib6Entry> BgpRib6::insert(BgpLogHandler *logger, 
        const std::vector<Prefix6> &routes, const uint8_t nexthop_global[16], 
        const uint8_t nexthop_linklocal[16], int32_t weight) {
    std::vector<BgpRib6Entry> inserted;
    std::vector<std::shared_ptr<BgpPathAttrib>> attribs;
    BgpPathAttribOrigin *origin = new BgpPathAttribOrigin(logger);
    BgpPathAttribAsPath *as_path = new BgpPathAttribAsPath(logger, true);
    origin->origin = IGP;

    attribs.push_back(std::shared_ptr<BgpPathAttrib>(origin));
    attribs.push_back(std::shared_ptr<BgpPathAttrib>(as_path));

    for (const Prefix6 &route : routes) {
        rib6_t::const_iterator it = find_entry(route, 0);

        if (it != rib.end()) continue;

        BgpRib6Entry new_entry (route, 0, nexthop_global, nexthop_linklocal, attribs);
        new_entry.update_id = update_id;
        new_entry.weight = weight;
        rib6_t::const_iterator isrt_it = rib.insert(MAKE_ENTRY6(route, new_entry));
        inserted.push_back(isrt_it->second);
    }

    update_id++;
    return inserted;
}

/**
 * @brief Insert new entries into RIB.
 * 
 * @param src_router_id Originating BGP speaker's ID in network bytes order.
 * @param route Route to be inserted.
 * @param nexthop_global Global IPv6 address of nexthop.
 * @param nexthop_linklocal Link local IPv6 address of nexthop. (if none, use NULL)
 * @param attrib Path attribute.
 * @param weight weight of this entry.
 * @param ibgp_asn ASN of the peer if the route is from an IBGP peer. 0 if not.
 * @return <const BgpRib6Entry*, bool> inserted info: <new_best_route, 
 * inserted_is_best>
 * @retval <const BgpRib6Entry*, true> inserted route is the new best route. 
 * const BgpRib6Entry* is the inserted route.
 * @retval <const BgpRib6Entry*, false> inseted route replaced current best
 * route, but new best route is NOT the inserted one. New best has been returned
 * in const BgpRib6Entry*.
 * @retval <NULL, false> inserted route is not the new best, and current best
 * has not changed.
 */
std::pair<const BgpRib6Entry*, bool> BgpRib6::insert(uint32_t src_router_id, 
    const Prefix6 &route, 
    const uint8_t nexthop_global[16], const uint8_t nexthop_linklocal[16], 
    const std::vector<std::shared_ptr<BgpPathAttrib>> &attribs, int32_t weight,
    uint32_t ibgp_asn) {

    update_id++;
    return insertPriv(src_router_id, route, nexthop_global, nexthop_linklocal, attribs, weight, ibgp_asn);
}

/**
 * @brief Insert new entries into RIB.
 * 
 * @param src_router_id Originating BGP speaker's ID in network bytes order.
 * @param routes List of routes.
 * @param nexthop_global Global IPv6 address of nexthop.
 * @param nexthop_linklocal Link local IPv6 address of nexthop. (if none, use NULL)
 * @param attrib Path attribute. 
 * @param weight weight of this entry.
 * @param ibgp_asn ASN of the peer if the route is from an IBGP peer. 0 if not.
 * @return <updated_routes, new_best_routes>
 */
std::pair<std::vector<BgpRib6Entry>, std::vector<Prefix6>> BgpRib6::insert(
    uint32_t src_router_id, const std::vector<Prefix6> &routes, 
    const uint8_t nexthop_global[16], const uint8_t nexthop_linklocal[16], 
    const std::vector<std::shared_ptr<BgpPathAttrib>> &attribs, int32_t weight, uint32_t ibgp_asn) {
    update_id++;
    std::vector<BgpRib6Entry> updated;
    std::vector<Prefix6> unchanged;
    for (const Prefix6 &route : routes) {
        std::pair<const BgpRib6Entry*, bool> rslt = insertPriv(src_router_id, route, nexthop_global, nexthop_linklocal, attribs, weight, ibgp_asn);
        if (rslt.first != NULL) {
            if (!rslt.second) updated.push_back(*(rslt.first));
            else unchanged.push_back(route);
        }
    }
    return std::make_pair(updated, unchanged);
}

/**
 * @brief Withdraw a route from RIB.
 * 
 * @param src_router_id Originating BGP speaker's ID in network bytes order.
 * @param route Route.
 * @return <bool, const void*> withdrawn information
 * @retval <false, NULL> if the withdrawed route is no longer reachable.
 * @retval <false, const Prefix6 *> if the withdrawed route is not in rib.
 * @retval <true, NULL> if the route withdrawed but still reachable with current
 * best route.
 * @retval <true, const BgpRib6Entry*> if the route withdrawed and that changes
 * the current best route.
 */
std::pair<bool, const void*> BgpRib6::withdraw(uint32_t src_router_id, const Prefix6 &route) {
    std::lock_guard<std::recursive_mutex> lock(mutex);

    std::pair<rib6_t::iterator, rib6_t::iterator> old_entries = 
        rib.equal_range(BgpRib6EntryKey(route));

    if (old_entries.first == rib.end()) 
        return std::make_pair<bool, const void*>(false, &route); // not in RIB.

    const char *op = "dropped/no_change";

    BgpRib6Entry *replacement = NULL;
    // uint64_t old_best_uid = 0;
    rib6_t::const_iterator to_remove = rib.end();
    
    for (rib6_t::iterator it = old_entries.first; it != old_entries.second; it++) {
        if (it->second.route == route) {
            if (it->second.src_router_id == src_router_id) {
                to_remove = it;
                continue;
            }
            replacement = selectEntry(replacement, &(it->second));
        }
    }

    bool reachabled = true;

    if (to_remove == rib.end()) 
        return std::make_pair<bool, const void*>(false, NULL);
    
    if (replacement != NULL) {
        if (to_remove->second.status == RS_ACTIVE) {
            op = "dropped/best_changed";
        } else replacement = NULL;
    } else {
        reachabled = false;
        op = "dropped/unreachabled";
    }

    rib.erase(to_remove);
    if (replacement != NULL) replacement->status = RS_ACTIVE;

    LIBBGP_LOG(logger, INFO) {
        uint8_t prefix_arr[16];
        route.getPrefix(prefix_arr);
        char src_router_id_str[INET_ADDRSTRLEN], prefix_str[INET6_ADDRSTRLEN];
        inet_ntop(AF_INET, &src_router_id, src_router_id_str, INET_ADDRSTRLEN);
        inet_ntop(AF_INET6, prefix_arr, prefix_str, INET6_ADDRSTRLEN);
        logger->log(INFO, "BgpRib6::withdraw: (%s) scope %s, route %s/%d\n", op, src_router_id_str, prefix_str, route.getLength());
    }

    return std::pair<bool, const BgpRib6Entry*>(reachabled, replacement);
}

/**
 * @brief Drop all routes from RIB that originated from a BGP speaker.
 * 
 * @param src_router_id src_router_id Originating BGP speaker's ID in network bytes order.
 * @return std::pair<std::vector<Prefix6>, std::vector<BgpRib6Entry>> 
 * <dropped_routes, updated_routes> pair. dropped_routes should be send as
 * withdrawn to peers, updated_routes should be send as update to peer.
 */
std::pair<std::vector<Prefix6>, std::vector<BgpRib6Entry>> BgpRib6::discard(uint32_t src_router_id) {
    std::lock_guard<std::recursive_mutex> lock(mutex);
    /*std::vector<Prefix6> dropped_routes;

    for (rib6_t::const_iterator it = rib.begin(); it != rib.end();) {
        if (it->second.src_router_id == src_router_id) {
            dropped_routes.push_back(it->second.route);
            LIBBGP_LOG(logger, INFO) {
                    uint8_t prefix_arr[16];
                    it->second.route.getPrefix(prefix_arr);
                    char src_router_id_str[INET_ADDRSTRLEN], prefix_str[INET6_ADDRSTRLEN];
                    inet_ntop(AF_INET, &src_router_id, src_router_id_str, INET_ADDRSTRLEN);
                    inet_ntop(AF_INET6, prefix_arr, prefix_str, INET6_ADDRSTRLEN);
                logger->log(INFO, "BgpRib6::discard: (dropped) scope %s, route %s/%d\n", src_router_id_str, prefix_str, it->second.route.getLength());
            }
            it = rib.erase(it);
        } else it++;
    }*/

    std::vector<Prefix6> reevaluate_routes;
    std::vector<Prefix6> dropped_routes;

    for (rib6_t::const_iterator it = rib.begin(); it != rib.end();) {
        const char *op = "dropped/silent";
        if (it->second.src_router_id != src_router_id) {
            it++;
            continue;
        }
        if (it->second.status == RS_ACTIVE) {
            reevaluate_routes.push_back(it->second.route);
            op = "dropped/pending-reevaluate";
        }
        LIBBGP_LOG(logger, INFO) {
            uint8_t prefix_arr[16];
            it->second.route.getPrefix(prefix_arr);
            char src_router_id_str[INET_ADDRSTRLEN], prefix_str[INET6_ADDRSTRLEN];
            inet_ntop(AF_INET, &src_router_id, src_router_id_str, INET_ADDRSTRLEN);
            inet_ntop(AF_INET6, prefix_arr, prefix_str, INET6_ADDRSTRLEN);
            logger->log(INFO, "BgpRib6::discard: (%s) scope %s, route %s/%d\n", op, src_router_id_str, prefix_str, it->second.route.getLength());
        }
        it = rib.erase(it);
    }

    std::vector<BgpRib6Entry> replacements;

    for (std::vector<Prefix6>::const_iterator it = reevaluate_routes.begin(); it != reevaluate_routes.end(); it++) {
        const char *op = "replacement found";
        const Prefix6 &prefix = *it;
        rib6_t::iterator replacement = find_best(prefix);
        if (replacement == rib.end()) { // no replacement.
            dropped_routes.push_back(prefix);
            op = "no available replacement";
        } else {
            replacement->second.status = RS_ACTIVE;
            replacements.push_back(replacement->second);
        }

        LIBBGP_LOG(logger, INFO) {
            uint8_t prefix_arr[16];
            prefix.getPrefix(prefix_arr);
            char src_router_id_str[INET_ADDRSTRLEN], prefix_str[INET6_ADDRSTRLEN];
            inet_ntop(AF_INET, &src_router_id, src_router_id_str, INET_ADDRSTRLEN);
            inet_ntop(AF_INET6, prefix_arr, prefix_str, INET6_ADDRSTRLEN);
            logger->log(INFO, "BgpRib6::discard: (%s) scope %s, route %s/%d\n", op, src_router_id_str, prefix_str, prefix.getLength());
        }
    }

    return std::make_pair(dropped_routes, replacements);

    // return dropped_routes;
}

/**
 * @brief Lookup a destination in RIB.
 * 
 * @param dest The destination address.
 * @return const BgpRib6::BgpRib6Entry* Matching entry.
 * @retval NULL no match found.
 * @retval BgpRib6Entry* Matching entry.
 */
const BgpRib6Entry* BgpRib6::lookup(const uint8_t dest[16]) const {
    const BgpRib6Entry *selected_entry = NULL;

    for (const auto &entry : rib) {
        if (entry.second.status != RS_ACTIVE) continue;
        const Prefix6 &route = entry.second.route;
        if (route.includes(dest)) 
            selected_entry = selectEntry(&entry.second, selected_entry);
    }

    return selected_entry;
}

/**
 * @brief Scoped RIB lookup.
 * 
 * Simular to lookup with only one argument but only lookup in routes from the
 * given BGP speaker.
 * 
 * @param src_router_id Originating BGP speaker's ID in network bytes order.
 * @param dest The destination address.
 * @return const BgpRib6::BgpRib6Entry* Matching entry.
 * @retval NULL no match found.
 * @retval BgpRib6Entry* Matching entry.
 */
const BgpRib6Entry* BgpRib6::lookup(uint32_t src_router_id, const uint8_t dest[16]) const {
    const BgpRib6Entry *selected_entry = NULL;

    for (const auto &entry : rib) {
        if (entry.second.status != RS_ACTIVE) continue;
        if (entry.second.src_router_id != src_router_id) continue;
        const Prefix6 &route = entry.second.route;
        if (route.includes(dest)) 
            selected_entry = selectEntry(&entry.second, selected_entry);
    }

    return selected_entry;
}

/**
 * @brief Get the RIB.
 * 
 * @return const rib6_t& The RIB.
 */
const rib6_t& BgpRib6::get() const {
    return rib;
}

}