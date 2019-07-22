#include <string.h>
#include "bgp-rib6.h"

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
BgpRib6Entry::BgpRib6Entry (Route6 r, uint32_t src, const uint8_t nexthop_global[16], 
    const uint8_t nexthop_linklocal[16], const std::vector<std::shared_ptr<BgpPathAttrib>> attribs)
    : route(r), attribs(attribs) {

    memcpy(this->nexthop_global, nexthop_global, 16);
    if (nexthop_linklocal != NULL) memcpy(this->nexthop_linklocal, nexthop_linklocal, 16);
    else memset(this->nexthop_linklocal, 0, 16);
    src_router_id = src;
}

/**
 * @brief Get metric of this entry.
 * 
 * Get metric of current entry. Currently, metric is AS_PATH length.
 * 
 * @return uint32_t Metric. Higher value means lower priority.
 */
uint32_t BgpRib6Entry::getMetric() const {
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
 * @brief Construct a new BgpRib6 object with logging.
 * 
 * @param logger Log handler to use.
 */
BgpRib6::BgpRib6(BgpLogHandler *logger) {
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
 * @param route Route.
 * @param nexthop_global Global IPv6 address of nexthop.
 * @param nexthop_linklocal Link local IPv6 address of nexthop. (if none, use NULL)
 * @return const BgpRib6Entry* Inserted route entry.
 * @retval NULL failed to insert.
 * @retval !=NULL Inserted route entry.
 */
const BgpRib6Entry* BgpRib6::insert(BgpLogHandler *logger, const Route6 &route,
    const uint8_t nexthop_global[16], const uint8_t nexthop_linklocal[16]) {
    std::vector<std::shared_ptr<BgpPathAttrib>> attribs;
    BgpPathAttribOrigin *origin = new BgpPathAttribOrigin(logger);
    BgpPathAttribAsPath *as_path = new BgpPathAttribAsPath(logger, true);
    origin->origin = IGP;

    attribs.push_back(std::shared_ptr<BgpPathAttrib>(origin));
    attribs.push_back(std::shared_ptr<BgpPathAttrib>(as_path));

    BgpRib6Entry new_entry(route, 0, nexthop_global, nexthop_linklocal, attribs);
    uint64_t use_update_id = update_id;

    for (const BgpRib6Entry &entry : rib) {
        if (entry.src_router_id == 0 && entry.route == route) {
            logger->log(ERROR, "BgpRib6::insert: route exists.");
            return NULL;
        }

        // see if we can group this entry to other local entries
        if (entry.src_router_id == 0) {
            if (memcmp(new_entry.nexthop_global, entry.nexthop_global, 16) == 0 &&
                memcmp(new_entry.nexthop_linklocal, entry.nexthop_linklocal, 16) == 0) {
                use_update_id = entry.update_id;
            }
        }
    }

    std::lock_guard<std::recursive_mutex> lock(mutex);
    new_entry.update_id = use_update_id;
    if (use_update_id == update_id) update_id++;
    rib.push_back(new_entry);

    return &(rib.back());
}

/**
 * @brief Insert new entries into RIB.
 * 
 * @param src_router_id Originating BGP speaker's ID in network bytes order.
 * @param route Route to be inserted.
 * @param nexthop_global Global IPv6 address of nexthop.
 * @param nexthop_linklocal Link local IPv6 address of nexthop. (if none, use NULL)
 * @param attrib Path attribute.
 * @return true Route inserted/replaced.
 * @return false Route already exist and the existing one has lower metric.
 */
bool BgpRib6::insert(uint32_t src_router_id, const Route6 &route, 
    const uint8_t nexthop_global[16], const uint8_t nexthop_linklocal[16], 
    const std::vector<std::shared_ptr<BgpPathAttrib>> &attrib) {

}

}