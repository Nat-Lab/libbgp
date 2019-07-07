/**
 * @file bgp-packet.h
 * @author Nato Morichika <nat@nat.moe>
 * @brief Top level deserialization/serialization entry point for BGP messages.
 * @version 0.1
 * @date 2019-07-06
 * 
 * @copyright Copyright (c) 2019
 * 
 */
#ifndef BGP_PACKET_H_
#define BGP_PACKET_H_
#include "serializable.h"
#include "bgp-message.h"

namespace libbgp {

/**
 * @brief The BgpPacket class.
 * 
 * BgpPacket class is the top level deserialization/serialization entry point 
 * for BGP messages. This is what you should use if you wish to 
 * serialize/deserialize a BGP message.
 * 
 */
class BgpPacket : public Serializable {
public:
    BgpPacket(BgpLogHandler *logger, bool is_4b, const BgpMessage *msg);
    BgpPacket(BgpLogHandler *logger, bool is_4b);
    virtual ~BgpPacket();
    ssize_t doPrint(size_t indent, uint8_t **to, size_t *buf_sz) const;

    // notice: BgpPacket::parse assume the buffer contains one valid bgp message
    // (i.e.) poured from bgp-sink. in other words, BgpPacket::parse assume
    // the length field in message header is valid and bgp marker is correct.
    ssize_t parse(const uint8_t *from, size_t buf_sz);
    ssize_t write(uint8_t *to, size_t buf_sz) const;
    const BgpMessage *getMessage() const;
private:
    BgpMessage *m_msg;
    const BgpMessage *msg;

    // is the BgpMessage owned by us? (i.e. created by parse())
    // we will need to delete the object if this is the case.
    bool is_message_owner;
    
    bool is_4b;
};

/**
 * @example deserialize-and-serialize.cc
 * 
 * Deserializing and serializing BGP message with BgpPacket.
 * 
 */

}
#endif // BGP_PACKET_H_