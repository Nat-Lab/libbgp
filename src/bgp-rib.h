#ifndef BGP_RIB_H_
#define BGP_RIB_H_
#include <stdint.h>
#include <vector>
#include <memory>
#include <arpa/inet.h>
#include "bgp-path-attrib.h"
#include "bgp-log-handler.h"

namespace libbgp {

/**
 * @brief Source of the RIB entry.
 * 
 */
enum BgpRouteSource {
    SRC_EBGP = 0,
    SRC_IBGP = 1
};

/**
 * @brief Status of the RIB entry.
 * 
 */
enum BgpRouteStatus {
    RS_STANDBY = 0,
    RS_ACTIVE = 1
};

/**
 * @brief The base of BGP RIB entry.
 * 
 * @tparam T Type of BgpRibEntry
 */
template<typename T> class BgpRibEntry {
public:
    /**
     * @brief Construct a new BgpRibEntry
     * 
     * source default to SRC_EBGP 
     * 
     */
    BgpRibEntry () { src = SRC_EBGP; status = RS_ACTIVE; }

    /**
     * @brief The originating BGP speaker's ID of this entry. (network bytes order)
     * 
     */
    uint32_t src_router_id;

    /**
     * @brief The update ID. 
     * 
     * BgpRib4Entry with same update ID are received from the same update and 
     * their path attributes are therefore same. Note that entries with 
     * different update_id may still have same path attributes.
     * 
     */
    uint64_t update_id;

    /**
     * @brief Weight of this entry. 
     * 
     * Entry with higher weight will be prefered. Weight is only compared when
     * selecting entry with equal routes.
     * 
     */
    int32_t weight;

    /**
     * @brief Path attributes for this entry.
     * 
     */
    std::vector<std::shared_ptr<BgpPathAttrib>> attribs;

    /**
     * @brief Source of this entry.
     * 
     * If the route is from an IBGP peer, the nexthop might not be reachable
     * directly. You may need recursive nexthop or info from other routing
     * protocols.
     */
    BgpRouteSource src;

    /**
     * @brief Status of this entry.
     * 
     * Active routes are routes currently picked as best routes. 
     * 
     */
    BgpRouteStatus status;

    /**
     * @brief ASN of the IBGP peer. (Valid iff src == SRC_IBGP)
     * 
     * The ibgp_peer_asn field can be used to identify which IBGP session does 
     * this route belongs to. This is needed since a BGP speaker can have
     * multiple IBGP sessions with different ASNS.
     */
    uint32_t ibgp_peer_asn;

    /**
     * @brief Test if this entry has greater weight then anoter entry. 
     * Please note that weight are only calculated based on path attribues. 
     * (i.e., you need to compare route prefix first)
     * 
     * @param other The other entry.
     * @return true This entry has higher weight.
     * @return false This entry has lower or equals weight.
     */
    bool operator> (const T &other) const {
        // perfer ebgp
        if (this->src > other.src) return false;

        // prefer higher weight
        if (this->weight > other.weight) return true;
        if (this->weight < other.weight) return false;

        uint32_t other_med = 0;
        uint32_t this_med = 0;

        uint32_t other_orig_as = 0;
        uint32_t this_orig_as = 0;

        uint8_t other_origin = 0;
        uint8_t this_origin = 0;

        uint8_t other_as_path_len = 0;
        uint8_t this_as_path_len = 0;

        uint32_t other_local_pref = 100;
        uint32_t this_local_pref = 100;

        // grab attributes
        for (const std::shared_ptr<BgpPathAttrib> &attr : other.attribs) {
            if (attr->type_code == MULTI_EXIT_DISC) {
                const BgpPathAttribMed &med = dynamic_cast<const BgpPathAttribMed &>(*attr);
                other_med = med.med;
            }

            if (attr->type_code == ORIGIN) {
                const BgpPathAttribOrigin &orig = dynamic_cast<const BgpPathAttribOrigin &>(*attr);
                other_origin = orig.origin;
                continue;
            }

            if (attr->type_code == AS_PATH) {
                const BgpPathAttribAsPath &as_path = dynamic_cast<const BgpPathAttribAsPath &>(*attr);
                for (const BgpAsPathSegment &seg : as_path.as_paths) {
                    if (seg.type == AS_SEQUENCE) {
                        other_as_path_len = seg.value.size();
                        other_orig_as = seg.value.back();
                    }
                }
                continue;
            }

            if (attr->type_code == LOCAL_PREF) {
                const BgpPathAttribLocalPref &pref = dynamic_cast<const BgpPathAttribLocalPref &>(*attr);
                other_local_pref = pref.local_pref;
                continue;
            }
        }

        for (const std::shared_ptr<BgpPathAttrib> &attr : attribs) {
            if (attr->type_code == MULTI_EXIT_DISC) {
                const BgpPathAttribMed &med = dynamic_cast<const BgpPathAttribMed &>(*attr);
                this_med = med.med;
            }

            if (attr->type_code == ORIGIN) {
                const BgpPathAttribOrigin &orig = dynamic_cast<const BgpPathAttribOrigin &>(*attr);
                this_origin = orig.origin;
                continue;
            }

            if (attr->type_code == AS_PATH) {
                const BgpPathAttribAsPath &as_path = dynamic_cast<const BgpPathAttribAsPath &>(*attr);
                for (const BgpAsPathSegment &seg : as_path.as_paths) {
                    if (seg.type == AS_SEQUENCE) {
                        this_as_path_len = seg.value.size();
                        this_orig_as = seg.value.back();
                    }
                }
                continue;
            }

            if (attr->type_code == LOCAL_PREF) {
                const BgpPathAttribLocalPref &pref = dynamic_cast<const BgpPathAttribLocalPref &>(*attr);
                this_local_pref = pref.local_pref;
                continue;
            }
        }

        /**/ if (this_local_pref > other_local_pref) return true;
        else if (this_local_pref < other_local_pref) return false;
        else if (other_as_path_len > this_as_path_len) return true;
        else if (other_as_path_len < this_as_path_len) return false;
        else if (other_origin > this_origin) return true;
        else if (other_origin < this_origin) return false;
        else if (other_orig_as == this_orig_as && other_med > this_med) return true;
        else if (other_orig_as == this_orig_as && other_med < this_med) return false;
        else if (other.update_id > update_id) return true;
        else if (other.update_id < update_id) return false;
        else if (htonl(other.src_router_id) > htonl(src_router_id)) return true;

        return false;
    }

};

#ifdef SWIG
class BgpRib6Entry;
class BgpRib4Entry;
%template(Rib6Entry) BgpRibEntry<BgpRib6Entry>;
%template(Rib4Entry) BgpRibEntry<BgpRib4Entry>;
#endif

/**
 * @brief The Base of BGP RIB.
 * 
 * @tparam T Type of BgpRibEntry.
 */
template<typename T> class BgpRib {
protected:
    /**
     * @brief Select an entry from two to use.
     * 
     * @param a Entry A
     * @param b Entry B
     * @return const T* Selected entry. One of A and B. 
     */
    static const T* selectEntry (const T *a, const T *b) {
        if (a == NULL) return b;
        if (b == NULL) return a;

        auto &ra = a->route;
        auto &rb = b->route;

        // a is more specific, use a
        if (ra.getLength() > rb.getLength()) return a;

        // a, b are same level of specific, check metric
        if (ra.getLength() == rb.getLength()) {
            // return the one with higher weight
            return (*b > *a) ? b : a;
        }

        // b is more specific, use b
        return b;
    }

    /**
     * @brief Select an entry from two to use.
     * 
     * @param a Entry A
     * @param b Entry B
     * @return T* Selected entry. One of A and B. 
     */
    static T* selectEntry (T *a, T *b) {
        if (a == NULL) return b;
        if (b == NULL) return a;

        auto &ra = a->route;
        auto &rb = b->route;

        // a is more specific, use a
        if (ra.getLength() > rb.getLength()) return a;

        // a, b are same level of specific, check metric
        if (ra.getLength() == rb.getLength()) {
            // return the one with higher weight
            return (*b > *a) ? b : a;
        }

        // b is more specific, use b
        return b;
    }
};

}

#endif // BGP_RIB_H_