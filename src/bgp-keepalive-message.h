/**
 * @file bgp-keepalive-message.h
 * @author Nato Morichika <nat@nat.moe>
 * @brief The BGP keepalive message.
 * @version 0.1
 * @date 2019-07-04
 * 
 * @copyright Copyright (c) 2019
 * 
 */
#ifndef BGP_KEEPALIVE_MSG_H_
#define BGP_KEEPALIVE_MSG_H_

#include "bgp-message.h"
#include "bgp-log-handler.h"
#include <stdint.h>

namespace libbgp {

/**
 * @brief The BgpKeepaliveMessage class.
 * 
 * This is deserializer/serializer for BGP keepalive message body. If you want 
 * to deserializer/serializer a full BGP message. Take a look at BgpPacket
 * class.
 */
class BgpKeepaliveMessage : public BgpMessage {
public:
    BgpKeepaliveMessage(BgpLogHandler *logger);

    ssize_t doPrint(size_t indent, uint8_t **to, size_t *buf_sz) const;
    ssize_t parse(const uint8_t *from, size_t msg_sz);
    ssize_t write(uint8_t *to, size_t buf_sz) const;
};

}

#endif // BGP_KEEPALIVE_MSG_H_