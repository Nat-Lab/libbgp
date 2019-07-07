/**
 * @file bgp-rib.cc
 * @author Nato Morichika <nat@nat.moe>
 * @brief The BGP Routing Information Base.
 * @version 0.1
 * @date 2019-07-04
 * 
 * @copyright Copyright (c) 2019
 * 
 */
#include "bgp-rib.h"
#include <arpa/inet.h>

namespace libbgp {

/**
 * @brief Construct a new Bgp Rib Entry:: Bgp Rib Entry object
 * 
 * @param r Prefix of the entry.
 * @param src Originating BGP speaker's ID in network bytes order.
 * @param as Path attributes for this entry.
 */
BgpRibEntry::BgpRibEntry(Route r, uint32_t src, const std::vector<std::shared_ptr<BgpPathAttrib>> as) : route(r), attribs(as) {
    src_router_id = src;
}

/**
 * @brief Get metric of this entry.
 * 
 * Get metric of current entry. Currently, metric is AS_PATH length.
 * 
 * @return uint32_t Metric. Higher value means lower priority.
 */
uint32_t BgpRibEntry::getMetric() const {
    for (const std::shared_ptr<BgpPathAttrib> &attr : attribs) {
        if (attr->type_code == AS_PATH) {
            const BgpPathAttribAsPath &as_path = dynamic_cast<const BgpPathAttribAsPath &>(*attr);
            for (const BgpAsPathSegment &seg : as_path.as_paths) {
                if (seg.type == AS_SEQUENCE) return seg.value.size();
            }
        }
    }

    return 0;
}

/**
 * @brief Construct a new BgpRib object without logging.
 * 
 */
BgpRib::BgpRib() {
    logger = NULL;
}

/**
 * @brief Construct a new BgpRib object with logging.
 * 
 * @param logger Log handler to use.
 */
BgpRib::BgpRib(BgpLogHandler *logger) {
    this->logger = logger;
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
 * @param route Route.
 * @param nexthop Nexthop for the route.
 * @retval NULL failed to insert.
 * @retval !=NULL Inserted route.
 */
const BgpRibEntry* BgpRib::insert(BgpLogHandler *logger, const Route &route, uint32_t nexthop) {
    std::vector<std::shared_ptr<BgpPathAttrib>> attribs;
    BgpPathAttribOrigin *origin = new BgpPathAttribOrigin(logger);
    BgpPathAttribNexthop *nexhop_attr = new BgpPathAttribNexthop(logger);
    BgpPathAttribAsPath *as_path = new BgpPathAttribAsPath(logger, true);
    nexhop_attr->next_hop = nexthop;
    origin->origin = IGP;

    attribs.push_back(std::shared_ptr<BgpPathAttrib>(origin));
    attribs.push_back(std::shared_ptr<BgpPathAttrib>(nexhop_attr));
    attribs.push_back(std::shared_ptr<BgpPathAttrib>(as_path));

    for (const BgpRibEntry &entry : rib) {
        if (entry.src_router_id == 0 && entry.route == route) {
            if (logger) logger->stderr("BgpRib::insert: route exists.");
            return NULL;
        }
    }

    BgpRibEntry new_entry(route, 0, attribs);
    std::lock_guard<std::recursive_mutex> lock(mutex);
    rib.push_back(new_entry);

    return &(rib.back());
}

/**
 * @brief Insert a new entry into RIB.
 * 
 * @param src_router_id Originating BGP speaker's ID in network bytes order.
 * @param route Route.
 * @param attrib Path attribute.
 * @return true Route inserted/replaced.
 * @return false Route already exist and the existing one has lower metric. 
 */
bool BgpRib::insert(uint32_t src_router_id, const Route &route, const std::vector<std::shared_ptr<BgpPathAttrib>> &attrib) {
    BgpRibEntry new_entry(route, src_router_id, attrib);

    for (std::vector<BgpRibEntry>::const_iterator entry = rib.begin(); entry != rib.end(); entry++) {
        if (entry->route == route && entry->src_router_id == src_router_id) {
            if (entry->getMetric() > new_entry.getMetric()) {
                rib.erase(entry);
                rib.push_back(new_entry);

                if (logger) {
                    uint32_t prefix = route.getPrefix();
                    logger->stdout("BgpRib::insert: %s: ", inet_ntoa(*(const struct in_addr*) &src_router_id));
                    logger->stdout("updated entry: %s/%d\n", inet_ntoa(*(const struct in_addr*) &prefix), route.getLength());
                }

                return true;
            } else return false;
        }
    }

    if (logger) {
        uint32_t prefix = route.getPrefix();
        logger->stdout("BgpRib::insert: %s: ", inet_ntoa(*(const struct in_addr*) &src_router_id));
        logger->stdout("new entry: %s/%d\n", inet_ntoa(*(const struct in_addr*) &prefix), route.getLength());
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
ssize_t BgpRib::insert(uint32_t src_router_id, const std::vector<Route> &routes, const std::vector<std::shared_ptr<BgpPathAttrib>> &attrib) {
    size_t inserted = 0;
    for (const Route &r : routes) {
        if (insert(src_router_id, r, attrib)) inserted++;
    }
    return inserted;
}

/**
 * @brief Withdraw a route from RIB.
 * 
 * @param src_router_id Originating BGP speaker's ID in network bytes order.
 * @param route Route.
 * @return true Route dropped.
 * @return false Route not dropped. Likely becuase such route does not exist.
 */
bool BgpRib::withdraw(uint32_t src_router_id, const Route &route) {
    std::lock_guard<std::recursive_mutex> lock(mutex);
    for (std::vector<BgpRibEntry>::const_iterator entry = rib.begin(); entry != rib.end(); entry++) {
        if (entry->route == route && entry->src_router_id == src_router_id) {
            if (logger) {
                uint32_t prefix = route.getPrefix();
                logger->stdout("BgpRib::withdraw: %s: ", inet_ntoa(*(const struct in_addr*) &src_router_id));
                logger->stdout("dropped entry: %s/%d\n", inet_ntoa(*(const struct in_addr*) &prefix), route.getLength());
            }
            rib.erase(entry);
            return true;
        }
    }

    return false;
}

/**
 * @brief Drop all routes from RIB that originated from a BGP speaker.
 * 
 * @param src_router_id Originating BGP speaker's ID in network bytes order.
 * @return ssize_t Number of routes dropped.
 * @retval -1 Failed to drop routes.
 * @retval >=0 Number of routes dropped.
 */
ssize_t BgpRib::discard(uint32_t src_router_id) {
    std::lock_guard<std::recursive_mutex> lock(mutex);
    size_t n_erased = 0;
    for (std::vector<BgpRibEntry>::const_iterator entry = rib.begin(); entry != rib.end();) {
        if (entry->src_router_id == src_router_id) {
            rib.erase(entry);
            n_erased++;
            if (logger) {
                const Route &route = entry->route;
                uint32_t prefix = route.getPrefix();
                logger->stdout("BgpRib::discard: %s: ", inet_ntoa(*(const struct in_addr*) &src_router_id));
                logger->stdout("dropped entry: %s/%d\n", inet_ntoa(*(const struct in_addr*) &prefix), route.getLength());
            }
        } else entry++;
    }

    return n_erased;
}

const BgpRibEntry* BgpRib::selectEntry(const BgpRibEntry *a, const BgpRibEntry *b) {
    assert(!(a == NULL && b == NULL));

    if (a == NULL) return b;
    if (b == NULL) return a;

    const Route &ra = a->route;
    const Route &rb = b->route;

    // a is more specific, use a
    if (ra.getLength() > rb.getLength()) return a;

    // a, b are same level of specific, check metric
    if (ra.getLength() == rb.getLength()) {
        // return the one with lower metric
        return (a->getMetric() > b->getMetric()) ? b : a;
    }

    // b is more specific, use b
    return b;
}

/**
 * @brief Lookup a destination in RIB.
 * 
 * @param dest The destination address in network byte order.
 * @return const BgpRibEntry* Matching entry.
 * @retval NULL no match found.
 * @retval BgpRibEntry* Matching entry.
 */
const BgpRibEntry* BgpRib::lookup(uint32_t dest) const {
    const BgpRibEntry *selected_entry = NULL;

    for (const BgpRibEntry &entry : rib) {
        const Route &route = entry.route;
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
 * @return const BgpRibEntry* Matching entry.
 * @retval NULL no match found.
 * @retval BgpRibEntry* Matching entry.
 */
const BgpRibEntry* BgpRib::lookup(uint32_t src_router_id, uint32_t dest) const {
    const BgpRibEntry *selected_entry = NULL;

    for (const BgpRibEntry &entry : rib) {
        if (entry.src_router_id != src_router_id) continue;
        const Route &route = entry.route;
        if (route.includes(dest)) 
            selected_entry = selectEntry(&entry, selected_entry);
    }

    return selected_entry;
}

/**
 * @brief Get the RIB.
 * 
 * @return const std::vector<BgpRibEntry>& The RIB.
 */
const std::vector<BgpRibEntry>& BgpRib::get() const {
    return rib;
}

}