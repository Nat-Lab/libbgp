#include "bgp-update-message.h"
#include "bgp-error.h"

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

}
