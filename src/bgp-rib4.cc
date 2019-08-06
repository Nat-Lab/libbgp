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
#define MAKE_ENTRY4(r, s, e) std::make_pair(BgpRib4EntryKey(r, s), e)
#define FIND_ENTRY4(rib, r, s) rib.find(BgpRib4EntryKey(r, s))

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

bool BgpRib4::insertPriv(uint32_t src_router_id, const Prefix4 &route, const std::vector<std::shared_ptr<BgpPathAttrib>> &attrib, int32_t weight) {
    std::lock_guard<std::recursive_mutex> lock(mutex);
    BgpRib4Entry new_entry(route, src_router_id, attrib);
    new_entry.update_id = update_id;
    new_entry.weight = weight;

    for (rib4_t::const_iterator entry = rib.begin(); entry != rib.end(); entry++) {
        if (entry->second.route == route && entry->second.src_router_id == src_router_id) {
            if (new_entry > entry->second) {
                rib.erase(entry);
                rib.insert(MAKE_ENTRY4(route, src_router_id, new_entry));

                LIBBGP_LOG(logger, INFO) {
                    uint32_t prefix = route.getPrefix();
                    char src_router_id_str[INET_ADDRSTRLEN], prefix_str[INET_ADDRSTRLEN];
                    inet_ntop(AF_INET, &src_router_id, src_router_id_str, INET_ADDRSTRLEN);
                    inet_ntop(AF_INET, &prefix, prefix_str, INET_ADDRSTRLEN);
                    logger->log(INFO, "BgpRib4::insert: (updated) group %d, scope %s, route %s/%d\n", new_entry.update_id, src_router_id_str, prefix_str, route.getLength());
                }

                return true;
            } else return false;
        }
    }

    LIBBGP_LOG(logger, INFO) {
        uint32_t prefix = route.getPrefix();
        char src_router_id_str[INET_ADDRSTRLEN], prefix_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &src_router_id, src_router_id_str, INET_ADDRSTRLEN);
        inet_ntop(AF_INET, &prefix, prefix_str, INET_ADDRSTRLEN);
        logger->log(INFO, "BgpRib4::insert: (new_entry) group %d, scope %s, route %s/%d\n", new_entry.update_id, src_router_id_str, prefix_str, route.getLength());
    }

    rib.insert(MAKE_ENTRY4(route, src_router_id, new_entry));
    return true;
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
            logger->log(ERROR, "BgpRib4::insert: route exists.");
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
    rib4_t::const_iterator it = rib.insert(MAKE_ENTRY4(route, 0, new_entry));

    return &(it->second);
}

/**
 * @brief Insert a local route into RIB.
 * 
 * Same as the other local insert, but this one notify the rev_bus after
 * inserting route.
 * 
 * @param logger Pointer to logger for the created path attributes to use. 
 * @param route Prefix4.
 * @param nexthop Nexthop for the route.
 * @param rev_bus event bus to publish to add event.
 * @param weight weight of this entry.
 * @retval NULL failed to insert.
 * @retval !=NULL Inserted route.
 */
const BgpRib4Entry* BgpRib4::insert(BgpLogHandler *logger, const Prefix4 &route, uint32_t nexthop, RouteEventBus *rev_bus, int32_t weight) {
    const BgpRib4Entry *entry = insert(logger, route, nexthop, weight);

    if (entry != NULL) {
        Route4AddEvent add_event;
        add_event.routes.push_back(entry->route);
        add_event.attribs = entry->attribs;
        rev_bus->publish(NULL, add_event);
    }

    return entry;
}

/**
 * @brief Insert local routes into RIB.
 * 
 * ame as the other local insert, but this one insert mutiple routes.
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
        rib4_t::const_iterator it = FIND_ENTRY4(rib, route, 0);

        if (it != rib.end()) continue;

        BgpRib4Entry new_entry (route, 0, attribs);
        new_entry.update_id = update_id;
        new_entry.weight = weight;
        rib4_t::const_iterator isrt_it = rib.insert(MAKE_ENTRY4(route, 0, new_entry));
        inserted.push_back(isrt_it->second);
    }

    update_id++;
    return inserted;
}

/**
 * @brief Insert local routes into RIB.
 * 
 * ame as the other local insert, but this one insert mutiple routes and notify
 * the route event bus.
 * 
 * @param logger Pointer to logger for the created path attributes to use. 
 * @param routes Routes.
 * @param nexthop Nexthop for the route.
 * @param rev_bus event bus to publish to add event.
 * @param weight weight of this entry.
 * @return const std::vector<const BgpRib4Entry*> Inserted routes.
 */
const std::vector<BgpRib4Entry> BgpRib4::insert(BgpLogHandler *logger, const std::vector<Prefix4> &routes, uint32_t nexthop, RouteEventBus *rev_bus, int32_t weight) {
    const std::vector<BgpRib4Entry> inserted =
        insert(logger, routes, nexthop, weight);
    
    if (inserted.size() > 0) {
        Route4AddEvent add_event;
        add_event.attribs = (inserted.begin())->attribs;

        for (const BgpRib4Entry &entry : inserted) {
            add_event.routes.push_back(entry.route);
        }

        rev_bus->publish(NULL, add_event);
    }

    return inserted;
}

/**
 * @brief Insert a new entry into RIB.
 * 
 * @param src_router_id Originating BGP speaker's ID in network bytes order.
 * @param route Prefix4.
 * @param attrib Path attribute.
 * @param weight weight of this entry.
 * @return true Prefix4 inserted/replaced.
 * @return false Prefix4 already exist and the existing one has lower metric. 
 */
bool BgpRib4::insert(uint32_t src_router_id, const Prefix4 &route, const std::vector<std::shared_ptr<BgpPathAttrib>> &attrib, int32_t weight) {
    bool inserted = insertPriv(src_router_id, route, attrib, weight);
    if (inserted) update_id++;
    return inserted;
}

/**
 * @brief Insert new entries into RIB.
 * 
 * @param src_router_id Originating BGP speaker's ID in network bytes order.
 * @param routes List of routes.
 * @param attrib Path attribute.
 * @param weight weight of this entry.
 * @return ssize_t Number of routes inserted.
 * @retval -1 Failed to insert routes.
 * @retval >=0 Number of routes inserted.
 */
ssize_t BgpRib4::insert(uint32_t src_router_id, const std::vector<Prefix4> &routes, const std::vector<std::shared_ptr<BgpPathAttrib>> &attrib, int32_t weight) {
    size_t inserted = 0;
    for (const Prefix4 &r : routes) {
        if (insertPriv(src_router_id, r, attrib, weight)) inserted++;
    }
    update_id++;
    return inserted;
}

/**
 * @brief Withdraw a route from RIB.
 * 
 * @param src_router_id Originating BGP speaker's ID in network bytes order.
 * @param route Prefix4.
 * @return true Prefix4 dropped.
 * @return false Prefix4 not dropped. Likely becuase such route does not exist.
 */
bool BgpRib4::withdraw(uint32_t src_router_id, const Prefix4 &route) {
    std::lock_guard<std::recursive_mutex> lock(mutex);

    rib4_t::iterator it = FIND_ENTRY4(rib, route, src_router_id);
    if (it == rib.end()) return false;

    LIBBGP_LOG(logger, INFO) {
        uint32_t prefix = route.getPrefix();
        char src_router_id_str[INET_ADDRSTRLEN], prefix_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &src_router_id, src_router_id_str, INET_ADDRSTRLEN);
        inet_ntop(AF_INET, &prefix, prefix_str, INET_ADDRSTRLEN);
        logger->log(INFO, "BgpRib4::withdraw: (dropped) scope %s, route %s/%d\n", src_router_id_str, prefix_str, route.getLength());
    }

    rib.erase(it);
    return true;
}

/**
 * @brief Withdrawn a route from RIB and send event to event bus.
 * 
 * @param src_router_id Originating BGP speaker's ID in network bytes order.
 * @param route Prefix4.
 * @param rev_bus Event bus to send the drop event
 * @return true Prefix4 dropped.
 * @return false Prefix4 not dropped. Likely becuase such route does not exist.
 */
bool BgpRib4::withdraw(uint32_t src_router_id, const Prefix4 &route, RouteEventBus *rev_bus) {
    Route4WithdrawEvent drop_ev;
    drop_ev.routes.push_back(route);
    rev_bus->publish(NULL, drop_ev);
    return withdraw(src_router_id, route);
}

/**
 * @brief Withdraw routes from RIB.
 * 
 * @param src_router_id Originating BGP speaker's ID in network bytes order.
 * @param routes Routes.
 * @return ssize_t Number of routes dropped.
 * @retval -1 Failed to drop routes.
 * @retval >=0 Number of routes dropped.
 */
ssize_t BgpRib4::withdraw(uint32_t src_router_id, const std::vector<Prefix4> &routes) {
    size_t dropped = 0;
    for (const Prefix4 &r : routes) {
        if (withdraw(src_router_id, r)) dropped++;
    }
    return dropped;
}

/**
 * @brief Withdraw routes from RIB and send event to event bus.
 * 
 * @param src_router_id Originating BGP speaker's ID in network bytes order.
 * @param routes Routes.
 * @param rev_bus Event bus to send the drop event
 * @return ssize_t Number of routes dropped.
 * @retval -1 Failed to drop routes.
 * @retval >=0 Number of routes dropped.
 */
ssize_t BgpRib4::withdraw(uint32_t src_router_id, const std::vector<Prefix4> &routes, RouteEventBus *rev_bus) {
    Route4WithdrawEvent drop_ev;
    drop_ev.routes = routes;
    rev_bus->publish(NULL, drop_ev);
    return withdraw(src_router_id, routes);
}

/**
 * @brief Drop all routes from RIB that originated from a BGP speaker.
 * 
 * @param src_router_id src_router_id Originating BGP speaker's ID in network bytes order.
 * @return std::vector<Prefix4> Dropped routes.
 */
std::vector<Prefix4> BgpRib4::discard(uint32_t src_router_id) {
    std::lock_guard<std::recursive_mutex> lock(mutex);
    std::vector<Prefix4> dropped_routes;
    std::vector<BgpRib4EntryKey> to_remove;

    for (auto &&ribval_pair : rib) {
        if (ribval_pair.second.src_router_id == src_router_id) {
            dropped_routes.push_back(ribval_pair.second.route);
            LIBBGP_LOG(logger, INFO) {
                uint32_t prefix = ribval_pair.second.route.getPrefix();
                char src_router_id_str[INET_ADDRSTRLEN], prefix_str[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &src_router_id, src_router_id_str, INET_ADDRSTRLEN);
                inet_ntop(AF_INET, &prefix, prefix_str, INET_ADDRSTRLEN);
                logger->log(INFO, "BgpRib4::discard: (to_drop) scope %s, route %s/%d\n", src_router_id_str, prefix_str, ribval_pair.second.route.getLength());
            }
            to_remove.push_back(ribval_pair.first);
        }
    }

    for (auto &&key : to_remove) rib.erase(key);

    return dropped_routes;
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