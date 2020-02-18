/**
 * @file bgp-rib4.cc
 * @author Nato Morichika <nat@nat.moe>
 * @brief The IPv4 BGP Routing Information Base.
 * @version 0.2
 * @date 2019-07-21
 * 
 * @copyright Copyright (c) 2019
 * 
 */
#include "bgp-rib4.h"
#include <arpa/inet.h>
#define MAKE_ENTRY4(r, e) std::make_pair(BgpRib4EntryKey(r), e)

namespace libbgp {

BgpRib4Entry::BgpRib4Entry() {
    src_router_id = 0;
}

/**
 * @brief Construct a new Bgp Rib Entry:: Bgp Rib Entry object
 * 
 * @param r Prefix of the entry.
 * @param src Originating BGP speaker's ID in network bytes order.
 * @param as Path attributes for this entry.
 */
BgpRib4Entry::BgpRib4Entry(Prefix4 r, uint32_t src, const std::vector<std::shared_ptr<BgpPathAttrib>> as) : route(r) {
    src_router_id = src;
    attribs = as;
}

/**
 * @brief Get nexthop for this entry.
 * 
 * @return uint32_t nexthop in network byte order.
 * @throws "no_nexthop" nexthop attribute does not exist.
 */
uint32_t BgpRib4Entry::getNexthop() const {
    for (const std::shared_ptr<BgpPathAttrib> &attr : attribs) {
        if (attr->type_code == NEXT_HOP) {
            const BgpPathAttribNexthop &nh = dynamic_cast<const BgpPathAttribNexthop &>(*attr);
            return nh.next_hop;
        }
    }

    throw "no_nexthop";
}

/**
 * @brief Construct a new BgpRib4 object with logging.
 * 
 * @param logger Log handler to use.
 */
BgpRib4::BgpRib4(BgpLogHandler *logger) {
    this->logger = logger;
    update_id = 0;    
}

rib4_t::iterator BgpRib4::find_best (const Prefix4 &prefix) {
    std::pair<rib4_t::iterator, rib4_t::iterator> its = 
        rib.equal_range(BgpRib4EntryKey(prefix));

    rib4_t::iterator best = rib.end();
    if (its.first == rib.end()) return rib.end();

    for (rib4_t::iterator it = its.first; it != its.second; it++) {
        if (it->second.route == prefix) {
            if (best == rib.end()) best = it;
            else {
                const BgpRib4Entry *best_ptr = selectEntry(&(best->second), &(it->second));
                best = best_ptr == &(best->second) ? best : it;
            }
        }
    }

    return best;
}

rib4_t::iterator BgpRib4::find_entry (const Prefix4 &prefix, uint32_t src) {
    std::pair<rib4_t::iterator, rib4_t::iterator> its = 
        rib.equal_range(BgpRib4EntryKey(prefix));

    if (its.first == rib.end()) return rib.end();

    for (rib4_t::iterator it = its.first; it != its.second; it++) {
        if (it->second.route == prefix && it->second.src_router_id == src) {
            return it;
        }
    }

    return rib.end();
}

/**
 * @brief The actual insert implementation.
 * 
 * @param src_router_id source router ID.
 * @param route route to insert.
 * @param attrib route attributes.
 * @param weight route weight.
 * @param ibgp_asn remote ASN, if IBGP.
 * @return <const BgpRib4Entry*, bool> inserted info: <new_best_route, 
 * inserted_is_best>
 * @retval <const BgpRib4Entry*, true> inserted route is the new best route. 
 * const BgpRib4Entry* is the inserted route.
 * @retval <const BgpRib4Entry*, false> inseted route replaced current best
 * route, but new best route is NOT the inserted one. New best has been returned
 * in const BgpRib4Entry*.
 * @retval <NULL, false> inserted route is not the new best, and current best
 * has not changed.
 */
std::pair<const BgpRib4Entry*, bool> BgpRib4::insertPriv(uint32_t src_router_id, const Prefix4 &route, const std::vector<std::shared_ptr<BgpPathAttrib>> &attrib, int32_t weight, uint32_t ibgp_asn) {
    std::lock_guard<std::recursive_mutex> lock(mutex);

    /* construct the new entry object */
    BgpRib4Entry new_entry(route, src_router_id, attrib);
    new_entry.update_id = update_id;
    new_entry.weight = weight;
    new_entry.src = ibgp_asn > 0 ? SRC_IBGP : SRC_EBGP;
    new_entry.ibgp_peer_asn = ibgp_asn;

    // for logging
    const char *op = "new_entry";
    const char *act = "new_best";

    std::pair<rib4_t::iterator, rib4_t::iterator> entries = 
        rib.equal_range(BgpRib4EntryKey(route));

    bool newly_inserted_is_best = false;
    bool best_changed = false;
    bool old_exist = entries.first != rib.end();
    BgpRib4Entry *new_best = NULL;

    // older route exist
    if (old_exist) {
        // find old best & route to replace
        rib4_t::const_iterator to_replace = rib.end();
        BgpRib4Entry *old_best = NULL;
        for (rib4_t::iterator it = entries.first; it != entries.second; it++) {
            if (it->second.route != route) continue;
            if (it->second.src_router_id == src_router_id) {
                to_replace = it;
                continue;
            }
            old_best = selectEntry(old_best, &(it->second));
            //if (it->second.status == RS_ACTIVE) old_best = &(it->second);
        }

        const BgpRib4Entry *candidate = selectEntry(&new_entry, old_best);
        if (candidate == old_best) {
            new_entry.status = RS_STANDBY;
            act = "not_new_best";
        } else {
            if (old_best != NULL) old_best->status = RS_STANDBY;
            best_changed = true;
        }

        if (to_replace != rib.end()) {
            const BgpRib4Entry *candidate = selectEntry(&(to_replace->second), old_best);
            if (candidate == &(to_replace->second)) {
                // the replaced route was the best route, now it is removed
                act = "new_best";
                best_changed = true;
            }
            // we need to replace a route
            op = "update";
            rib.erase(to_replace);
        }

        rib4_t::iterator inserted = rib.insert(MAKE_ENTRY4(route, new_entry));

        if (best_changed) {
            newly_inserted_is_best = candidate == &new_entry;
            new_best = newly_inserted_is_best ? &(inserted->second) : old_best;
        }

    } else { // no older route, new one is best
        best_changed = newly_inserted_is_best = true;
        rib4_t::iterator inserted = rib.insert(MAKE_ENTRY4(route, new_entry));
        new_best = &(inserted->second);
    }

    if (new_best != NULL) new_best->status = RS_ACTIVE;
    
    LIBBGP_LOG(logger, DEBUG) {
        uint32_t prefix = route.getPrefix();
        char src_router_id_str[INET_ADDRSTRLEN], prefix_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &src_router_id, src_router_id_str, INET_ADDRSTRLEN);
        inet_ntop(AF_INET, &prefix, prefix_str, INET_ADDRSTRLEN);
        logger->log(DEBUG, "BgpRib4::insertPriv: (%s/%s) group %d, scope %s, route %s/%d\n", op, act, new_entry.update_id, src_router_id_str, prefix_str, route.getLength());
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
 * @param route Prefix4.
 * @param nexthop Nexthop for the route.
 * @param weight weight of this entry.
 * @retval NULL failed to insert.
 * @retval !=NULL Inserted route.
 */
const BgpRib4Entry* BgpRib4::insert(BgpLogHandler *logger, const Prefix4 &route, uint32_t nexthop, int32_t weight) {
    std::vector<std::shared_ptr<BgpPathAttrib>> attribs;
    BgpPathAttribOrigin *origin = new BgpPathAttribOrigin(logger);
    BgpPathAttribNexthop *nexhop_attr = new BgpPathAttribNexthop(logger);
    BgpPathAttribAsPath *as_path = new BgpPathAttribAsPath(logger, true);
    nexhop_attr->next_hop = nexthop;
    origin->origin = IGP;

    attribs.push_back(std::shared_ptr<BgpPathAttrib>(origin));
    attribs.push_back(std::shared_ptr<BgpPathAttrib>(nexhop_attr));
    attribs.push_back(std::shared_ptr<BgpPathAttrib>(as_path));

    uint64_t use_update_id = update_id;

    for (const auto &entry : rib) {
        if (entry.second.src_router_id == 0 && entry.second.route == route) {
            this->logger->log(ERROR, "BgpRib4::insert: route exists.\n");
            return NULL;
        }

        // see if we can group this entry to other local entries
        if (entry.second.src_router_id == 0) {
            for (const std::shared_ptr<BgpPathAttrib> &attr : entry.second.attribs) {
                if (attr->type_code == NEXT_HOP) {
                    const BgpPathAttribNexthop &nh = dynamic_cast<const BgpPathAttribNexthop &>(*attr);
                    if (nh.next_hop == nexthop) use_update_id = entry.second.update_id;
                }
            }
        }
    }

    BgpRib4Entry new_entry(route, 0, attribs);
    std::lock_guard<std::recursive_mutex> lock(mutex);
    new_entry.update_id = use_update_id;
    new_entry.weight = weight;
    if (use_update_id == update_id) update_id++;
    rib4_t::const_iterator it = rib.insert(MAKE_ENTRY4(route, new_entry));

    return &(it->second);
}

/**
 * @brief Insert local routes into RIB.
 * 
 * ame as the other local insert, but this one insert mutiple routes.
 * 
 * This SHOULD NOT be called when the any of the upper FSM is running. 
 * 
 * @param logger Pointer to logger for the created path attributes to use. 
 * @param routes Routes.
 * @param nexthop Nexthop for the route.
 * @param weight weight of this entry.
 * @return const std::vector<const BgpRib4Entry*> Inserted routes.
 */
const std::vector<BgpRib4Entry> BgpRib4::insert(BgpLogHandler *logger, const std::vector<Prefix4> &routes, uint32_t nexthop, int32_t weight) {
    std::vector<BgpRib4Entry> inserted;
    std::vector<std::shared_ptr<BgpPathAttrib>> attribs;
    BgpPathAttribOrigin *origin = new BgpPathAttribOrigin(logger);
    BgpPathAttribNexthop *nexhop_attr = new BgpPathAttribNexthop(logger);
    BgpPathAttribAsPath *as_path = new BgpPathAttribAsPath(logger, true);
    nexhop_attr->next_hop = nexthop;
    origin->origin = IGP;

    attribs.push_back(std::shared_ptr<BgpPathAttrib>(origin));
    attribs.push_back(std::shared_ptr<BgpPathAttrib>(nexhop_attr));
    attribs.push_back(std::shared_ptr<BgpPathAttrib>(as_path));

    for (const Prefix4 &route : routes) {
        rib4_t::const_iterator it = find_entry(route, 0);

        if (it != rib.end()) continue;

        BgpRib4Entry new_entry (route, 0, attribs);
        new_entry.update_id = update_id;
        new_entry.weight = weight;
        rib4_t::const_iterator isrt_it = rib.insert(MAKE_ENTRY4(route, new_entry));
        inserted.push_back(isrt_it->second);
    }

    update_id++;
    return inserted;
}

/**
 * @brief Insert a new entry into RIB.
 * 
 * @param src_router_id Originating BGP speaker's ID in network bytes order.
 * @param route Prefix4.
 * @param attrib Path attribute.
 * @param weight weight of this entry.
 * @param ibgp_asn ASN of the peer if the route is from an IBGP peer. 0 if not.
 * @return <const BgpRib4Entry*, bool> entry that should be send to peer. (NULL-able)
 */
std::pair<const BgpRib4Entry*, bool> BgpRib4::insert(uint32_t src_router_id, const Prefix4 &route, const std::vector<std::shared_ptr<BgpPathAttrib>> &attrib, int32_t weight, uint32_t ibgp_asn) {
    update_id++;
    return insertPriv(src_router_id, route, attrib, weight, ibgp_asn);
}

/**
 * @brief Insert new entries into RIB.
 * 
 * @param src_router_id Originating BGP speaker's ID in network bytes order.
 * @param routes Routes.
 * @param attrib Path attribs.
 * @param weight weight of this entry.
 * @param ibgp_asn ASN of the peer if the route is from an IBGP peer. 0 if not.
 * @return std::pair<std::vector<BgpRib4Entry>, std::vector<Prefix4>> pair of
 * vectors. <updated_entries, unchanged_entries>.
 */
std::pair<std::vector<BgpRib4Entry>, std::vector<Prefix4>> BgpRib4::insert(uint32_t src_router_id, const std::vector<Prefix4> &routes, const std::vector<std::shared_ptr<BgpPathAttrib>> &attrib, int32_t weight,  uint32_t ibgp_asn) {
    update_id++;
    std::vector<BgpRib4Entry> updated;
    std::vector<Prefix4> unchanged;
    for (const Prefix4 &route : routes) {
        std::pair<const BgpRib4Entry*, bool> rslt = insertPriv(src_router_id, route, attrib, weight, ibgp_asn);
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
 * @param route Prefix4.
 * @return <bool, const void*> withdrawn information
 * @retval <false, NULL> if the withdrawed route is no longer reachable.
 * @retval <false, const Prefix4*> if the withdrawed route is not in rib.
 * @retval <true, NULL> if the route withdrawed but still reachable with current
 * best route.
 * @retval <true, const BgpRib4Entry*> if the route withdrawed and that changes
 * the current best route.
 */
std::pair<bool, const void*> BgpRib4::withdraw(uint32_t src_router_id, const Prefix4 &route) {
    std::lock_guard<std::recursive_mutex> lock(mutex);
    std::pair<rib4_t::iterator, rib4_t::iterator> old_entries = 
        rib.equal_range(BgpRib4EntryKey(route));

    if (old_entries.first == rib.end()) {
        LIBBGP_LOG(logger, DEBUG) {
            uint32_t prefix = route.getPrefix();
            char src_router_id_str[INET_ADDRSTRLEN], prefix_str[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &src_router_id, src_router_id_str, INET_ADDRSTRLEN);
            inet_ntop(AF_INET, &prefix, prefix_str, INET_ADDRSTRLEN);
            logger->log(DEBUG, "BgpRib4::withdraw: scope %s, route %s/%d: not found.\n", src_router_id_str, prefix_str, route.getLength());
        }

        return std::make_pair<bool, const void*>(false, &route); // not in RIB.
    }

    const char *op = "dropped/no_change";
    BgpRib4Entry *replacement = NULL;
    // uint64_t old_best_uid = 0;
    rib4_t::const_iterator to_remove = rib.end();
    
    for (rib4_t::iterator it = old_entries.first; it != old_entries.second; it++) {
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
        // const BgpRib4Entry *candidate = selectEntry(replacement, &(to_remove->second));
        if (to_remove->second.status == RS_ACTIVE) {
            op = "dropped/best_changed";
        } else replacement = NULL;
    } else {
        reachabled = false;
        op = "dropped/unreachabled";
    }

    rib.erase(to_remove);
    if (replacement != NULL) replacement->status = RS_ACTIVE;

    LIBBGP_LOG(logger, DEBUG) {
        uint32_t prefix = route.getPrefix();
        char src_router_id_str[INET_ADDRSTRLEN], prefix_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &src_router_id, src_router_id_str, INET_ADDRSTRLEN);
        inet_ntop(AF_INET, &prefix, prefix_str, INET_ADDRSTRLEN);
        logger->log(DEBUG, "BgpRib4::withdraw: (%s) scope %s, route %s/%d\n", op, src_router_id_str, prefix_str, route.getLength());
    }

    return std::pair<bool, const void*>(reachabled, replacement);
}

/**
 * @brief Drop all routes from RIB that originated from a BGP speaker.
 * 
 * @param src_router_id src_router_id Originating BGP speaker's ID in network bytes order.
 * @return std::pair<std::vector<Prefix4>, std::vector<BgpRib4Entry>> 
 * <dropped_routes, updated_routes> pair. dropped_routes should be send as
 * withdrawn to peers, updated_routes should be send as update to peer.
 * 
 */
std::pair<std::vector<Prefix4>, std::vector<BgpRib4Entry>> BgpRib4::discard(uint32_t src_router_id) {
    std::lock_guard<std::recursive_mutex> lock(mutex);
    std::vector<Prefix4> reevaluate_routes;
    std::vector<Prefix4> dropped_routes;

    for (rib4_t::const_iterator it = rib.begin(); it != rib.end();) {
        const char *op = "dropped/silent";
        if (it->second.src_router_id != src_router_id) {
            it++;
            continue;
        }
        if (it->second.status == RS_ACTIVE) {
            reevaluate_routes.push_back(it->second.route);
            op = "dropped/pending-reevaluate";
        }
        LIBBGP_LOG(logger, DEBUG) {
            uint32_t prefix = it->second.route.getPrefix();
            char src_router_id_str[INET_ADDRSTRLEN], prefix_str[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &src_router_id, src_router_id_str, INET_ADDRSTRLEN);
            inet_ntop(AF_INET, &prefix, prefix_str, INET_ADDRSTRLEN);
            logger->log(DEBUG, "BgpRib4::discard: (%s) scope %s, route %s/%d\n", op, src_router_id_str, prefix_str, it->second.route.getLength());
        }
        it = rib.erase(it);
    }

    std::vector<BgpRib4Entry> replacements;

    for (std::vector<Prefix4>::const_iterator it = reevaluate_routes.begin(); it != reevaluate_routes.end(); it++) {
        const char *op = "replacement found";
        const Prefix4 &prefix = *it;
        rib4_t::iterator replacement = find_best(prefix);
        if (replacement == rib.end()) { // no replacement.
            dropped_routes.push_back(prefix);
            op = "no available replacement";
        } else {
            replacement->second.status = RS_ACTIVE;
            replacements.push_back(replacement->second);
        }

        LIBBGP_LOG(logger, DEBUG) {
            uint32_t pfx = prefix.getPrefix();
            char prefix_str[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &pfx, prefix_str, INET_ADDRSTRLEN);
            logger->log(DEBUG, "BgpRib4::discard: %s for route %s/%d\n", op, prefix_str, prefix.getLength());
        }
    }

    return std::make_pair(dropped_routes, replacements);
}

/**
 * @brief Lookup a destination in RIB.
 * 
 * @param dest The destination address in network byte order.
 * @return const BgpRib4Entry* Matching entry.
 * @retval NULL no match found.
 * @retval BgpRib4Entry* Matching entry.
 */
const BgpRib4Entry* BgpRib4::lookup(uint32_t dest) const {
    const BgpRib4Entry *selected_entry = NULL;

    for (const auto &entry : rib) {
        if (entry.second.status != RS_ACTIVE) continue;
        const Prefix4 &route = entry.second.route;
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
 * @param dest The destination address in network byte order.
 * @return const BgpRib4Entry* Matching entry.
 * @retval NULL no match found.
 * @retval BgpRib4Entry* Matching entry.
 */
const BgpRib4Entry* BgpRib4::lookup(uint32_t src_router_id, uint32_t dest) const {
    const BgpRib4Entry *selected_entry = NULL;

    for (const auto &entry : rib) {
        if (entry.second.src_router_id != src_router_id) continue;
        if (entry.second.status != RS_ACTIVE) continue;
        const Prefix4 &route = entry.second.route;
        if (route.includes(dest)) 
            selected_entry = selectEntry(&entry.second, selected_entry);
    }

    return selected_entry;
}

/**
 * @brief Get the RIB.
 * 
 * @return const rib4_t& The RIB.
 */
const rib4_t& BgpRib4::get() const {
    return rib;
}

}