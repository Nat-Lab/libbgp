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

    BgpUpdateMessage();

    // construct with pre-defined attribs
    BgpUpdateMessage(const std::vector<BgpPathAttrib> &arrtibs);

    // construct with pre-defined attribs and NRLIs
    BgpUpdateMessage(const std::vector<BgpPathAttrib> &arrtibs, const std::vector<Route> &nlri);

    // construct with pre-defined withdrawn
    BgpUpdateMessage(const std::vector<Route> &withdraws);

    // get attribute by type
    BgpPathAttrib *getAttrib(BgpPathAttribType type);

    // get attribute by type, if attrib of that type does not exist, a new one
    // will be added.
    BgpPathAttrib &cGetAttrib(BgpPathAttribType type);

    // add an attribute, return false if attrib of same type already exists
    bool addAttrib(const BgpPathAttrib &attrib);

    // utility function to remove attribute by type
    void dropAttrib(BgpPathAttribType type);

    // utility function to drop all non-transitive attribute
    void dropNonTransitive();

    // utility function to set nexthop
    void setNextHop(uint32_t nexthop);

    // utility function to prepend a 4-bytes ASN to AS_PATH (for peer w/ 4b-asn
    // capability only (and 4b-asn enabled on local)), AS4_PATH attrib will be
    // removed if exist.
    void prepend(uint32_t asn);

    // utility function to prepend a 4-bytes ASN to AS_PATH (AS_TRANS will be
    // used if asn can't be repersent in 2b). This is for peer w/o 4b-asn
    // capability, but 4b-asn enabled locally. AS4_PATH will be create/replace
    // by AS_PATH, and 4-byte version of asn will be prepend to AS4_PATH.
    void prepend2b(uint32_t asn);

    // try to recover AS_TRANS in AS_PATH with infomation in AS4_PATH
    void restoreAsPath();

    // utility function to add a route to withdrawn list
    void addWithdrawn(uint32_t prefix, uint8_t length);

    // utility function to add a route to withdrawn list
    void addWithdrawn(const Route &route);

    // utility function to add a route to NLRI
    void addNlri(uint32_t prefix, uint8_t length);

    // utility function to add a route to NLRI
    void addNlri(const Route &route);

    ssize_t parse(const uint8_t *from, size_t msg_sz);
    ssize_t write(uint8_t *to, size_t buf_sz) const;

    uint8_t getErrorCode() const;
    uint8_t getErrorSubCode() const;
    const uint8_t* getError() const;
    size_t getErrorLength() const;
private:
    bool add_as4_path;
};

}
#endif // BGP_UPDATE_MSG_H_