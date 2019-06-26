#include "bgp-update-message.h"
#include "bgp-error.h"
#include "bgp-errcode.h"
#include "value-op.h"
#include <arpa/inet.h>
#include <assert.h>

namespace bgpfsm {

BgpUpdateMessage::BgpUpdateMessage(bool use_4b_asn) {
    this->use_4b_asn = use_4b_asn;
}

BgpPathAttrib& BgpUpdateMessage::getAttrib(uint8_t type) {
    for (BgpPathAttrib &attrib : path_attribute) {
        if (attrib.type_code == type) return attrib;
    }

    throw "no such attribute";
}

const BgpPathAttrib& BgpUpdateMessage::getAttrib(uint8_t type) const {
    for (const BgpPathAttrib &attrib : path_attribute) {
        if (attrib.type_code == type) return attrib;
    }

    throw "no such attribute";
}

bool BgpUpdateMessage::hasAttrib(uint8_t type) const {
    for (const BgpPathAttrib &attrib : path_attribute) {
        if (attrib.type_code == type) return true;
    }

    return false;
}

bool BgpUpdateMessage::addAttrib(const BgpPathAttrib &attrib) {
    if (hasAttrib(attrib.type_code)) return false;

    path_attribute.push_back(attrib);
    return true;
}

bool BgpUpdateMessage::setAttribs(const std::vector<BgpPathAttrib> &attrs) {
    path_attribute = attrs;
    return true;
}

bool BgpUpdateMessage::dropAttrib(uint8_t type) {
    for (auto attr = path_attribute.begin(); attr != path_attribute.end(); attr++) {
        if (attr->type_code == type) {
            path_attribute.erase(attr);
            return true;
        }
    }

    return false;
}

bool BgpUpdateMessage::dropNonTransitive() {
    bool removed = false;

    for (auto attr = path_attribute.begin(); attr != path_attribute.end();) {
        if (!attr->transitive) {
            removed = true;
            path_attribute.erase(attr);
        } else attr++;
    }

    return removed;
}

bool BgpUpdateMessage::updateAttribute(const BgpPathAttrib &attrib) {
    dropAttrib(attrib.type_code);
    return addAttrib(attrib);
}

bool BgpUpdateMessage::setNextHop(uint32_t nexthop) {
    BgpPathAttribNexthop nh = BgpPathAttribNexthop();
    nh.next_hop = nexthop;
    return updateAttribute(nh);
}

bool BgpUpdateMessage::prepend(uint32_t asn) {
    if (use_4b_asn) {
        // in 4b mode, prepend 4b asn to AS_PATH directly.

        // AS4_PATH can't exist in 4b mode
        if (hasAttrib(AS4_PATH)) {
            _bgp_error("BgpUpdateMessage::prepend: we have AS4_PATH attribute but we are running in 4b mode. " 
                       "consider restoreAsPath().\n");
            return false;
        }

        if (!hasAttrib(AS_PATH)) {
            BgpPathAttribAsPath path(use_4b_asn);
            path.prepend(asn);
            path_attribute.push_back(path);
            return true;
        }

        BgpPathAttribAsPath &path = dynamic_cast<BgpPathAttribAsPath &>(getAttrib(AS_PATH));
        if (!path.is_4b) {
            _bgp_error("BgpUpdateMessage::prepend: existing AS_PATH is 2b but we are running in 4b mode. " 
                       "consider restoreAsPath().\n");
            return false;
        }

        return path.prepend(asn);
    } else {
        // in 2b-mode, prepend 2b asn to AS_PATH and update AS4_PATH.
        // (yes, you don't update AS4_PATH as a 2b-speaker, but simplicity we do that for now)
        // FIXME: don't change as4_path if both side disabled 4b support

        uint16_t prep_asn = asn >= 0xffff ? 23456 : asn;

        if (!hasAttrib(AS_PATH)) {
            BgpPathAttribAsPath path(use_4b_asn);
            path.prepend(prep_asn);
            path_attribute.push_back(path);
        } else {
            BgpPathAttribAsPath &path = dynamic_cast<BgpPathAttribAsPath &>(getAttrib(AS_PATH));
            if (path.is_4b) {
                _bgp_error("BgpUpdateMessage::prepend: existing AS_PATH is 4b but we are running in 2b mode. " 
                           "consider downgradeAsPath().\n");
                return false;
            }
            if(!path.prepend(prep_asn)) return false;
        }

        if (hasAttrib(AS4_PATH)) {
            BgpPathAttribAs4Path &path4 = dynamic_cast<BgpPathAttribAs4Path &>(getAttrib(AS4_PATH));
            if(!path4.prepend(prep_asn)) return false;
        }

        return true;
    }
}


bool BgpUpdateMessage::restoreAsPath() {
    if (!hasAttrib(AS_PATH)) return true;

    BgpPathAttribAsPath &path = dynamic_cast<BgpPathAttribAsPath &>(getAttrib(AS_PATH));
    if (path.is_4b) {
        _bgp_error("BgpUpdateMessage::restoreAsPath: AS_PATH is already 4B.\n");
        return false;
    }

    // no AS4_PATH, just make AS_PATH 4b
    if (!hasAttrib(AS4_PATH)) {
        std::vector<BgpAsPathSegment> new_segs;

        for (const BgpAsPathSegment &seg : path.as_paths) {
            if (seg.is_4b) {
                _bgp_error("BgpUpdateMessage::restoreAsPath: 4b seg found in 2b attrib.\n");
                return false;
            }

            BgpAsPathSegment4b new_seg (seg.type);
            const BgpAsPathSegment2b &seg2 = dynamic_cast<const BgpAsPathSegment2b &>(seg);
            for (uint16_t asn : seg2.value) {
                if (asn == 23456) {
                    _bgp_error("BgpUpdateMessage::restoreAsPath: warning: AS_TRANS found but no AS4_PATH.\n");
                }
                if(!new_seg.prepend(asn)) return false;
            }

            new_segs.push_back(new_seg);
        }

        path.as_paths = new_segs;
        path.is_4b = true;
        return true;
    }

    // we have AS4_PATH recorver AS_TRANS.
    std::vector<uint32_t> full_as_path;
    const BgpPathAttribAs4Path &as4_path = dynamic_cast<const BgpPathAttribAs4Path &>(getAttrib(AS4_PATH));
    for (const BgpAsPathSegment &seg : as4_path.as4_paths) {
        if (!seg.is_4b) {
            _bgp_error("BgpUpdateMessage::restoreAsPath: bad as4_path: found 2b seg.\n");
            return false;
        }
        
        if (seg.type == AS_SEQUENCE) {
            const BgpAsPathSegment4b &seg4 = dynamic_cast<const BgpAsPathSegment4b &>(seg);
            const std::vector<uint32_t> &part = seg4.value;
            full_as_path.insert(full_as_path.end(), part.begin(), part.end());
        }
    }

    // AS4_PATH should be removed.
    dropAttrib(AS4_PATH);

    bool has_4b = false;

    // find the iterator to first asn > 0xffff
    std::vector<uint32_t>::const_iterator iter_4b = full_as_path.begin();

    for(; iter_4b != full_as_path.end(); iter_4b++) {
        has_4b = true;
        if (*iter_4b > 0xffff) break;
    }

    std::vector<BgpAsPathSegment> new_segs;

    for (const BgpAsPathSegment &seg : path.as_paths) {
        std::vector<uint32_t>::const_iterator local_iter = iter_4b;
        if (seg.is_4b) {
            _bgp_error("BgpUpdateMessage::restoreAsPath: 4b seg found in 2b attrib.\n");
            return false;
        }

        BgpAsPathSegment4b new_seg (seg.type);
        const BgpAsPathSegment2b &seg2 = dynamic_cast<const BgpAsPathSegment2b &>(seg);

        // increment the local_iter iterator?
        bool incr_iter = false;
        for (uint16_t asn : seg2.value) {
            uint32_t new_asn = asn;

            // AS4_PATH avaliale & not ended?
            if (has_4b && local_iter != full_as_path.end()) {

                // we found AS_TRANS, we need to replace it with last asn in AS_TRANS
                if (new_asn == 23456) {

                    // we have hit our first AS_TRANS. from now on, we need to move the AS4_PATH
                    // iterator too.
                    incr_iter = true;
                    new_asn = *local_iter;
                } else if (new_asn != *local_iter) {
                    _bgp_error("BgpUpdateMessage::restoreAsPath: warning: AS_PATH and AS4_PATH does not match.\n");
                }

                if (incr_iter) local_iter++;
            }
            
            if(!new_seg.prepend(new_asn)) return false;
        }

        new_segs.push_back(new_seg);
    }

    path.is_4b = true;
    path.as_paths = new_segs;
    return true;
    
}

bool BgpUpdateMessage::downgradeAsPath() {
    if (!hasAttrib(AS_PATH)) return true;

    BgpPathAttribAsPath &path = dynamic_cast<BgpPathAttribAsPath &>(getAttrib(AS_PATH));
    if (!path.is_4b) {
        _bgp_error("BgpUpdateMessage::downgradeAsPath: AS_PATH is already 2B.\n");
        return false;
    }

    std::vector<BgpAsPathSegment> new_segs;
    BgpPathAttribAs4Path path4;

    for (const BgpAsPathSegment &seg : path.as_paths) {
        if (!seg.is_4b) {
            _bgp_error("BgpUpdateMessage::downgradeAsPath: 2b seg found in 4b attrib.\n");
            return false;
        }

        BgpAsPathSegment2b new_seg (seg.type);
        const BgpAsPathSegment4b &seg4 = dynamic_cast<const BgpAsPathSegment4b &>(seg);
        for (uint32_t asn : seg4.value) {
            uint16_t new_as = asn >= 0xffff ? 23456 : asn;
            if(!new_seg.prepend(new_as)) return false;
        }

        path4.as4_paths.push_back(seg4);
        new_segs.push_back(new_seg);
    }
    
    updateAttribute(path4);
    path.is_4b = false;
    path.as_paths = new_segs;
    return true;
}

bool BgpUpdateMessage::restoreAggregator() {
    if (!hasAttrib(AGGREATOR)) return true;

    BgpPathAttribAggregator &aggr = dynamic_cast<BgpPathAttribAggregator &>(getAttrib(AGGREATOR));
    aggr.is_4b = true;

    if (!hasAttrib(AS4_AGGREGATOR)) return true;
    
    const BgpPathAttribAs4Aggregator &aggr4 = dynamic_cast<const BgpPathAttribAs4Aggregator &>(getAttrib(AS4_AGGREGATOR));
    aggr.aggregator = aggr4.aggregator;
    aggr.aggregator_asn = aggr4.aggregator_asn4;

    return true;
}

bool BgpUpdateMessage::downgradeAggregator() {
    if (!hasAttrib(AGGREATOR)) return true;
    
    BgpPathAttribAggregator &aggr = dynamic_cast<BgpPathAttribAggregator &>(getAttrib(AGGREATOR));
    aggr.is_4b = false;

    BgpPathAttribAs4Aggregator aggr4;
    aggr4.aggregator = aggr.aggregator;
    aggr4.aggregator_asn4 = aggr.aggregator_asn;
    updateAttribute(aggr4);

    if (aggr.aggregator_asn >= 0xffff) aggr.aggregator_asn = 23456;

    return true;
}

bool BgpUpdateMessage::setWithdrawn(const std::vector<Route> &routes) {
    withdrawn_routes = routes;
    return true;
}

bool BgpUpdateMessage::addWithdrawn(uint32_t prefix, uint8_t length) {
    Route route;
    route.length = length;
    route.prefix = prefix;
    withdrawn_routes.push_back(route);
    return true;
}

bool BgpUpdateMessage::addWithdrawn(const Route &route) {
    withdrawn_routes.push_back(route);
    return true;
}

bool BgpUpdateMessage::setNlri(const std::vector<Route> &routes) {
    nlri = routes;
    return true;
}

bool BgpUpdateMessage::addNlri(uint32_t prefix, uint8_t length) {
    Route route;
    route.length = length;
    route.prefix = prefix;
    nlri.push_back(route);
    return true;
}

bool BgpUpdateMessage::addNlri(const Route &route) {
    nlri.push_back(route);
    return true;
}

void BgpUpdateMessage::forwardParseError(const BgpPathAttrib &attrib) {
    setError(attrib.getErrorCode(), attrib.getErrorSubCode(), attrib.getError(), attrib.getErrorLength());
}

ssize_t BgpUpdateMessage::parse(const uint8_t *from, size_t msg_sz) {
    if (msg_sz < 4) {
        uint8_t _err_data = msg_sz;
        setError(E_HEADER, E_LENGTH, &_err_data, sizeof(uint8_t));
        _bgp_error("BgpUpdateMessage::parse: invalid open message size: %d.\n", msg_sz);
        return -1;
    }

    const uint8_t *buffer = from;

    uint16_t withdrawn_len = ntohs(getValue<uint16_t>(&buffer)); // len: 2

    if (withdrawn_len > msg_sz - 4) { // -4: two length fields (withdrawn len + attrib len)
        _bgp_error("BgpUpdateMessage::parse: withdrawn routes length overflows message.\n");
        setError(E_UPDATE, E_UNSPEC, NULL, 0);
        return -1;
    }

    uint16_t parsed_withdrawn_len = 0;
    
    while (parsed_withdrawn_len < withdrawn_len) {
        if (withdrawn_len - parsed_withdrawn_len < 1) {
            _bgp_error("BgpUpdateMessage::parse: unexpected end of withdrawn routes list.\n");
            setError(E_UPDATE, E_UNSPEC, NULL, 0);
            return -1;
        }

        uint8_t route_len = getValue<uint8_t>(&buffer); // len2: 1
        parsed_withdrawn_len++;
        if (route_len > 32) {
            _bgp_error("BgpUpdateMessage::parse: invalid route len in withdrawn routes: %d\n", route_len);
            setError(E_UPDATE, E_UNSPEC, NULL, 0);
            return -1;
        }

        size_t route_buffer_len = (route_len + 7) / 8;
        if (parsed_withdrawn_len + route_buffer_len > withdrawn_len) {
            _bgp_error("BgpUpdateMessage::parse: withdrawn route overflows routes list.\n");
            setError(E_UPDATE, E_UNSPEC, NULL, 0);
            return -1;
        }

        Route route;
        route.length = route_len;
        route.prefix = 0;
        memcpy(&(route.prefix), buffer, route_buffer_len);
        withdrawn_routes.push_back(route);

        buffer += route_buffer_len; // len2: route_buffer_len
        parsed_withdrawn_len += route_buffer_len;
    }

    assert(parsed_withdrawn_len == withdrawn_len);

    uint16_t attribute_len = ntohs(getValue<uint16_t>(&buffer)); // len: 2
    if ((size_t) (attribute_len + withdrawn_len + 4) > msg_sz) {
        _bgp_error("BgpUpdateMessage::parse: attribute list length overflows message buffer.\n");
        setError(E_UPDATE, E_ATTR_LIST, NULL, 0);
        return -1;
    }

    uint16_t parsed_attribute_len = 0;

    while (parsed_attribute_len < attribute_len) {
        if (attribute_len - parsed_attribute_len < 3) {
            _bgp_error("BgpUpdateMessage::parse: unexpected end of attribute list.\n");
            setError(E_UPDATE, E_UNSPEC, NULL, 0);
            return -1;
        }

        int8_t attr_type = BgpPathAttrib::GetTypeFromBuffer(buffer, attribute_len - parsed_attribute_len);

        if (attr_type < 0) {
            _bgp_error("BgpUpdateMessage::parse: failed to parse attribute type.\n");
            setError(E_UPDATE, E_UNSPEC, NULL, 0);
            return -1;
        }

        BgpPathAttrib *attrib = NULL;

        switch(attr_type) {
            case ORIGIN: attrib = new BgpPathAttribOrigin(); break;
            case AS_PATH: attrib = new BgpPathAttribAsPath(use_4b_asn); break;
            case NEXT_HOP: attrib = new BgpPathAttribNexthop(); break;
            case MULTI_EXIT_DISC: attrib = new BgpPathAttribMed(); break;
            case LOCAL_PREF: attrib = new BgpPathAttribLocalPref(); break;
            case ATOMIC_AGGREGATE: attrib =  new BgpPathAttribAtomicAggregate(); break;
            case AGGREATOR: attrib = new BgpPathAttribAggregator(use_4b_asn); break;
            case COMMUNITY: attrib = new BgpPathAttribCommunity(); break;
            case AS4_PATH: attrib = new BgpPathAttribAs4Path(); break;
            case AS4_AGGREGATOR: attrib = new BgpPathAttribAs4Aggregator(); break;
            default: attrib = new BgpPathAttribUnknow(); break;
        }

        assert(attrib != NULL);

        ssize_t attrib_parsed = attrib->parse(buffer, attribute_len - parsed_attribute_len);

        if (attrib_parsed < 0) {
            forwardParseError(*attrib);
            return -1;
        }

        buffer += attrib_parsed;
        parsed_attribute_len += attrib_parsed;
        path_attribute.push_back(*attrib);
        delete attrib;
    }

    assert(parsed_attribute_len == attribute_len);

    // 4: len fields (withdrawn len & attrib len)
    size_t nlri_len = msg_sz - 4 - parsed_attribute_len - parsed_withdrawn_len;
    size_t parsed_nlri_len = 0;

    while (parsed_nlri_len < nlri_len) {
        if (nlri_len - parsed_nlri_len < 1) {
            _bgp_error("BgpOpenMessage::parse: unexpected end of nlri.\n");
            setError(E_UPDATE, E_UNSPEC, NULL, 0);
            return -1;
        }

        uint8_t route_len = getValue<uint8_t>(&buffer); // len2: 1
        parsed_nlri_len++;
        if (route_len > 32) {
            _bgp_error("BgpUpdateMessage::parse: invalid route len in nlri routes: %d\n", route_len);
            setError(E_UPDATE, E_UNSPEC, NULL, 0);
            return -1;
        }

        size_t route_buffer_len = (route_len + 7) / 8;
        if (parsed_nlri_len + route_buffer_len > nlri_len) {
            _bgp_error("BgpUpdateMessage::parse: nlri route overflows routes list.\n");
            setError(E_UPDATE, E_UNSPEC, NULL, 0);
            return -1;
        }

        Route route;
        route.length = route_len;
        route.prefix = 0;
        memcpy(&(route.prefix), buffer, route_buffer_len);
        nlri.push_back(route);

        buffer += route_buffer_len; // len2: route_buffer_len
        parsed_nlri_len += route_buffer_len;
    }

    assert(parsed_nlri_len + parsed_attribute_len + parsed_withdrawn_len + 4 == msg_sz);

    return msg_sz;
}

}
