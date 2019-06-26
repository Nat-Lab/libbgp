#ifndef BGP_UPDATE_MSG_H_
#define BGP_UPDATE_MSG_H_
#include <vector>
#include <unistd.h>
#include "bgp-message.h"
#include "bgp-path-attrib.h"

namespace bgpfsm {

class BgpUpdateMessage : public BgpMessage {
public:
    std::vector<Route> withdrawn_routes;
    std::vector<BgpPathAttrib> path_attribute;
    std::vector<Route> nlri;

    BgpUpdateMessage(bool use_4b_asn);

    // get attribute by type, if attrib of that type does not exist, exception
    // will be thrown
    BgpPathAttrib &getAttrib(uint8_t type);

    // get const attribute by type, if attrib of that type does not exist, 
    // exception will be thrown
    const BgpPathAttrib &getAttrib(uint8_t type) const; 

    // return true if this type of attribute is in the message
    bool hasAttrib(uint8_t type) const;

    // add an attribute, return false if attrib of same type already exists
    bool addAttrib(const BgpPathAttrib &attrib);

    // replace attribute list with attrs
    bool setAttribs(const std::vector<BgpPathAttrib> &attrs);

    // utility function to remove attribute by type
    bool dropAttrib(uint8_t type);

    // utility function to update attribute (will be append if not exist)
    bool updateAttribute(const BgpPathAttrib &attrib);

    // utility function to drop all non-transitive attribute
    bool dropNonTransitive();

    // utility function to set nexthop
    bool setNextHop(uint32_t nexthop);

    // utility function to prepend an ASN to AS_PATH (for 2b mode, AS4_PATH and
    // AS_TRANS will be used)
    bool prepend(uint32_t asn);

    // try to recover AS_TRANS in AS_PATH with infomation in AS4_PATH, convert
    // 2b AS_PATH to 4b AS_PATH, and remove AS4_PATH attribute.
    bool restoreAsPath();

    // convert current 4b AS_PATH to 2b AS_PATH and add AS4_PATH attribute.
    bool downgradeAsPath();

    // recover 4b aggregator from aggregator4, remove aggregator4
    bool restoreAggregator();

    // convert 4b aggregator to 2b aggregator, add aggregator4
    bool downgradeAggregator();

    // replace withdrawn with routes
    bool setWithdrawn(const std::vector<Route> &routes);

    // utility function to add a route to withdrawn list
    bool addWithdrawn(uint32_t prefix, uint8_t length);

    // utility function to add a route to withdrawn list
    bool addWithdrawn(const Route &route);

    // replace NLRI with routes
    bool setNlri(const std::vector<Route> &routes);

    // utility function to add a route to NLRI
    bool addNlri(uint32_t prefix, uint8_t length);

    // utility function to add a route to NLRI
    bool addNlri(const Route &route);

    ssize_t parse(const uint8_t *from, size_t msg_sz);
    ssize_t write(uint8_t *to, size_t buf_sz) const;

private:
    // utility function to forward attribute parse error to here
    void forwardParseError(const BgpPathAttrib &attrib);

    // utility function to check if attributes are valid (i.e. no dulipicated, 
    // no missing well-known)
    bool validateAttribs();

    bool use_4b_asn;
};

}
#endif // BGP_UPDATE_MSG_H_