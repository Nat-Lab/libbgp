#include "bgp-rib.h"

namespace libbgp {

BgpRibEntry::BgpRibEntry(Route r, uint32_t src, const std::vector<std::shared_ptr<BgpPathAttrib>> as) : route(r), attribs(as) {
    src_router_id = src;
}

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

bool BgpRib::insert(uint32_t src_router_id, const Route &route, const std::vector<std::shared_ptr<BgpPathAttrib>> &attrib) {
    BgpRibEntry new_entry(route, src_router_id, attrib);

    for (std::vector<BgpRibEntry>::const_iterator entry = rib.begin(); entry != rib.end(); entry++) {
        if (entry->route == route && entry->src_router_id == src_router_id) {
            if (entry->getMetric() > new_entry.getMetric()) {
                rib.erase(entry);
                rib.push_back(new_entry);
                return true;
            } else return false;
        }
    }

    rib.push_back(new_entry);
    return true;
}

ssize_t BgpRib::insert(uint32_t src_router_id, const std::vector<Route> &routes, const std::vector<std::shared_ptr<BgpPathAttrib>> &attrib) {
    size_t inserted = 0;
    for (const Route &r : routes) {
        if (insert(src_router_id, r, attrib)) inserted++;
    }
    return inserted;
}

bool BgpRib::withdraw(uint32_t src_router_id, const Route &route) {
    for (std::vector<BgpRibEntry>::const_iterator entry = rib.begin(); entry != rib.end(); entry++) {
        if (entry->route == route && entry->src_router_id == src_router_id) {
            rib.erase(entry);
            return true;
        }
    }

    return false;
}

ssize_t BgpRib::discard(uint32_t src_router_id) {
    size_t n_erased = 0;
    for (std::vector<BgpRibEntry>::const_iterator entry = rib.begin(); entry != rib.end();) {
        if (entry->src_router_id == src_router_id) {
            rib.erase(entry);
            n_erased++;
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

const BgpRibEntry* BgpRib::lookup(uint32_t dest) const {
    const BgpRibEntry *selected_entry = NULL;

    for (const BgpRibEntry &entry : rib) {
        const Route &route = entry.route;
        if (route.includes(dest)) 
            selected_entry = selectEntry(&entry, selected_entry);
    }

    return selected_entry;
}

const BgpRibEntry* BgpRib::lookup(uint32_t src_router_id, uint32_t dest) const {
    uint8_t max_length = 0;
    const BgpRibEntry *selected_entry = NULL;

    for (const BgpRibEntry &entry : rib) {
        if (entry.src_router_id != src_router_id) continue;
        const Route &route = entry.route;
        if (route.includes(dest)) 
            selected_entry = selectEntry(&entry, selected_entry);
    }

    return selected_entry;
}

const std::vector<BgpRibEntry>& BgpRib::get() const {
    return rib;
}

}