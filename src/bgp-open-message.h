/**
 * @file bgp-open-message.h
 * @author Nato Morichika <nat@nat.moe>
 * @brief The BGP open message.
 * @version 0.1
 * @date 2019-07-04
 * 
 * @copyright Copyright (c) 2019
 * 
 */
#ifndef BGP_OPEN_MSG_H_
#define BGP_OPEN_MSG_H_
#include <vector>
#include <memory>
#include <unistd.h>
#include "bgp-message.h"
#include "bgp-capability.h"

namespace libbgp {

/**
 * @brief The BgpOpenMessage class.
 * 
 * This is deserializer/serializer for BGP open message body. If you want to 
 * deserializer/serializer a full BGP message. Take a look at BgpPacket class.
 */
class BgpOpenMessage : public BgpMessage {
public:
    BgpOpenMessage(BgpLogHandler *logger, bool use_4b_asn);
    BgpOpenMessage(BgpLogHandler *logger, bool use_4b_asn, uint16_t my_asn, uint16_t hold_time, uint32_t bgp_id);
    BgpOpenMessage(BgpLogHandler *logger, bool use_4b_asn, uint16_t my_asn, uint16_t hold_time, const char* bgp_id);
    ~BgpOpenMessage();

    uint8_t version;
    uint16_t my_asn;
    uint16_t hold_time;

    // bgp-id is in network-byte
    uint32_t bgp_id;

    // utility function for setting ASN. will add/edit BgpCapability if in 4b
    // mode
    bool setAsn(uint32_t my_asn);

    // utility function for getting ASN.
    uint32_t getAsn() const;

    // utility function for testing capability
    bool hasCapability(uint8_t code) const;

    bool addCapability(std::shared_ptr<BgpCapability> capability);

    ssize_t doPrint(size_t indent, uint8_t **to, size_t *buf_sz) const;

    ssize_t parse(const uint8_t *from, size_t msg_sz);

    ssize_t write(uint8_t *to, size_t buf_sz) const;

    // bgp-fsm only supports 4-bytes asn capability. getCapabilities() allows
    // you to get a full, read-only list of cpabilities.
    const std::vector<std::shared_ptr<BgpCapability>>& getCapabilities() const;
private:
    std::vector<std::shared_ptr<BgpCapability>> capabilities;
    bool use_4b_asn;
};

}
#endif // BGP_OPEN_MSG_H_