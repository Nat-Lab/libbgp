#include "bgp-update-message.h"

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


}
