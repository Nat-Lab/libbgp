/**
 * @file bgp-update-message.h
 * @author Nato Morichika <nat@nat.moe>
 * @brief The BGP update message
 * @version 0.1
 * @date 2019-07-04
 * 
 * @copyright Copyright (c) 2019
 * 
 */
#ifndef BGP_UPDATE_MSG_H_
#define BGP_UPDATE_MSG_H_
#include <vector>
#include <unistd.h>
#include <memory>
#include "prefix4.h"
#include "bgp-message.h"
#include "bgp-path-attrib.h"

namespace libbgp {

/**
 * @brief The BgpUpdateMessage class.
 * 
 * This is deserializer/serializer for BGP update message body. If you want to 
 * deserializer/serializer a full BGP message. Take a look at BgpPacket class.
 */
class BgpUpdateMessage : public BgpMessage {
public:
    std::vector<Prefix4> withdrawn_routes;
    std::vector<std::shared_ptr<BgpPathAttrib>> path_attribute;
    std::vector<Prefix4> nlri;

    BgpUpdateMessage(BgpLogHandler *logger, bool use_4b_asn);

    // get attribute by type, if attrib of that type does not exist, exception
    // will be thrown
    BgpPathAttrib &getAttrib(uint8_t type);

    // get const attribute by type, if attrib of that type does not exist, 
    // exception will be thrown
    const BgpPathAttrib &getAttrib(uint8_t type) const; 

    // return true if this type of attribute is in the message
    bool hasAttrib(uint8_t type) const;

    // copy and add an attribute, return false if attrib of same type already exists
    bool addAttrib(const BgpPathAttrib &attrib);

    // copy and replace attribute list with attrs
    bool setAttribs(const std::vector<std::shared_ptr<BgpPathAttrib>> &attrs);

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
    bool setWithdrawn4(const std::vector<Prefix4> &routes);

    // utility function to add a route to withdrawn list
    bool addWithdrawn4(uint32_t prefix, uint8_t length);

    // utility function to add a route to withdrawn list
    bool addWithdrawn4(const Prefix4 &route);

    // replace NLRI with routes
    bool setNlri4(const std::vector<Prefix4> &routes);

    // utility function to add a route to NLRI
    bool addNlri4(uint32_t prefix, uint8_t length);

    // utility function to add a route to NLRI
    bool addNlri4(const Prefix4 &route);

    // replace withdrawn with routes
    bool setWithdrawn6(const std::vector<Prefix6> &routes);

    // replace NLRI with routes
    bool setNlri6(const std::vector<Prefix6> &routes, const uint8_t nexthop_global[16], const uint8_t nexthop_linklocal[16]);

    ssize_t doPrint(size_t indent, uint8_t **to, size_t *buf_sz) const;
    ssize_t parse(const uint8_t *from, size_t msg_sz);
    ssize_t write(uint8_t *to, size_t buf_sz) const;

private:
    // utility function to check if attributes are valid (i.e. no dulipicated, 
    // no missing well-known)
    bool validateAttribs();

    bool use_4b_asn;
};

}
#endif // BGP_UPDATE_MSG_H_