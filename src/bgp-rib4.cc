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

namespace libbgp {

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
 * @retval NULL failed to insert.
 * @retval !=NULL Inserted route.
 */
const BgpRib4Entry* BgpRib4::insert(BgpLogHandler *logger, const Prefix4 &route, uint32_t nexthop) {
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

    for (const BgpRib4Entry &entry : rib) {
        if (entry.src_router_id == 0 && entry.route == route) {
            logger->log(ERROR, "BgpRib4::insert: route exists.");
            return NULL;
        }

        // see if we can group this entry to other local entries
        if (entry.src_router_id == 0) {
            for (const std::shared_ptr<BgpPathAttrib> &attr : entry.attribs) {
                if (attr->type_code == NEXT_HOP) {
                    const BgpPathAttribNexthop &nh = dynamic_cast<const BgpPathAttribNexthop &>(*attr);
                    if (nh.next_hop == nexthop) use_update_id = entry.update_id;
                }
            }
        }
    }

    BgpRib4Entry new_entry(route, 0, attribs);
    std::lock_guard<std::recursive_mutex> lock(mutex);
    new_entry.update_id = use_update_id;
    if (use_update_id == update_id) update_id++;
    rib.push_back(new_entry);

    return &(rib.back());
}

/**
 * @brief Insert a new entry into RIB.
 * 
 * @param src_router_id Originating BGP speaker's ID in network bytes order.
 * @param route Prefix4.
 * @param attrib Path attribute.
 * @return true Prefix4 inserted/replaced.
 * @return false Prefix4 already exist and the existing one has lower metric. 
 */
bool BgpRib4::insert(uint32_t src_router_id, const Prefix4 &route, const std::vector<std::shared_ptr<BgpPathAttrib>> &attrib) {
    BgpRib4Entry new_entry(route, src_router_id, attrib);

    for (std::vector<BgpRib4Entry>::const_iterator entry = rib.begin(); entry != rib.end(); entry++) {
        if (entry->route == route && entry->src_router_id == src_router_id) {
            if (new_entry > *entry) {
                rib.erase(entry);
                new_entry.update_id = update_id++;
                rib.push_back(new_entry);

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

    new_entry.update_id = update_id++;
    LIBBGP_LOG(logger, INFO) {
        uint32_t prefix = route.getPrefix();
        char src_router_id_str[INET_ADDRSTRLEN], prefix_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &src_router_id, src_router_id_str, INET_ADDRSTRLEN);
        inet_ntop(AF_INET, &prefix, prefix_str, INET_ADDRSTRLEN);
        logger->log(INFO, "BgpRib4::insert: (new_entry) group %d, scope %s, route %s/%d\n", new_entry.update_id, src_router_id_str, prefix_str, route.getLength());
    }

    mutex.lock();
    rib.push_back(new_entry);
    mutex.unlock();
    return true;
}

/**
 * @brief Insert new entries into RIB.
 * 
 * @param src_router_id Originating BGP speaker's ID in network bytes order.
 * @param routes List of routes.
 * @param attrib Path attribute.
 * @return ssize_t Number of routes inserted.
 * @retval -1 Failed to insert routes.
 * @retval >=0 Number of routes inserted.
 */
ssize_t BgpRib4::insert(uint32_t src_router_id, const std::vector<Prefix4> &routes, const std::vector<std::shared_ptr<BgpPathAttrib>> &attrib) {
    size_t inserted = 0;
    uint64_t hold_id = update_id;
    for (const Prefix4 &r : routes) {
        if (insert(src_router_id, r, attrib)) inserted++;
        update_id = hold_id;
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
    for (std::vector<BgpRib4Entry>::const_iterator entry = rib.begin(); entry != rib.end(); entry++) {
        if (entry->route == route && entry->src_router_id == src_router_id) {
            LIBBGP_LOG(logger, INFO) {
                uint32_t prefix = route.getPrefix();
                char src_router_id_str[INET_ADDRSTRLEN], prefix_str[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &src_router_id, src_router_id_str, INET_ADDRSTRLEN);
                inet_ntop(AF_INET, &prefix, prefix_str, INET_ADDRSTRLEN);
                logger->log(INFO, "BgpRib4::withdraw: (dropped) scope %s, route %s/%d\n", src_router_id_str, prefix_str, route.getLength());
            }
            rib.erase(entry);
            return true;
        }
    }

    return false;
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
 * @brief Drop all routes from RIB that originated from a BGP speaker.
 * 
 * @param src_router_id src_router_id Originating BGP speaker's ID in network bytes order.
 * @return std::vector<Prefix4> Dropped routes.
 */
std::vector<Prefix4> BgpRib4::discard(uint32_t src_router_id) {
    std::lock_guard<std::recursive_mutex> lock(mutex);
    std::vector<Prefix4> dropped_routes;
    for (std::vector<BgpRib4Entry>::const_iterator entry = rib.begin(); entry != rib.end();) {
        if (entry->src_router_id == src_router_id) {
            dropped_routes.push_back(entry->route);
            LIBBGP_LOG(logger, INFO) {
                uint32_t prefix = entry->route.getPrefix();
                char src_router_id_str[INET_ADDRSTRLEN], prefix_str[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &src_router_id, src_router_id_str, INET_ADDRSTRLEN);
                inet_ntop(AF_INET, &prefix, prefix_str, INET_ADDRSTRLEN);
                logger->log(INFO, "BgpRib4::discard: (dropped) scope %s, route %s/%d\n", src_router_id_str, prefix_str, entry->route.getLength());
            }
            rib.erase(entry);
        } else entry++;
    }

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

    for (const BgpRib4Entry &entry : rib) {
        const Prefix4 &route = entry.route;
        if (route.includes(dest)) 
            selected_entry = selectEntry(&entry, selected_entry);
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

    for (const BgpRib4Entry &entry : rib) {
        if (entry.src_router_id != src_router_id) continue;
        const Prefix4 &route = entry.route;
        if (route.includes(dest)) 
            selected_entry = selectEntry(&entry, selected_entry);
    }

    return selected_entry;
}

/**
 * @brief Get the RIB.
 * 
 * @return const std::vector<BgpRib4Entry>& The RIB.
 */
const std::vector<BgpRib4Entry>& BgpRib4::get() const {
    return rib;
}

}