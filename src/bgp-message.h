/**
 * @file bgp-message.h
 * @author Nato Morichika <nat@nat.moe>
 * @brief The BGP Message base.
 * @version 0.1
 * @date 2019-07-04
 * 
 * @copyright Copyright (c) 2019
 * 
 */
#ifndef BGP_MSG_H_
#define BGP_MSG_H_
#include <stdint.h>
#include <unistd.h>
#include "serializable.h"
#include "bgp-log-handler.h"

namespace libbgp {

/**
 * @brief BGP Message types.
 * 
 */
enum BgpMessageType {
    OPEN = 1,
    UPDATE = 2,
    NOTIFICATION = 3,
    KEEPALIVE = 4
};

/**
 * @brief The BgpMessage base class.
 * 
 */
class BgpMessage : public Serializable {
public:
    /**
     * @brief Construct a new Bgp Message object
     * 
     * @param logger Pointer to logger object for error logging.
     */
    BgpMessage(BgpLogHandler *logger) : Serializable(logger) {}

    /**
     * @brief Deserialize a BGP message *body*.
     * 
     * BgpMessage deserializer only deserialize message body. (i.e. message 
     * without BGP marker and message length field) To deserialize a BGP packet,
     * use BgpPacket.
     * 
     * @param from Pointer to message body buffer.
     * @param msg_sz Size of message.
     * @return ssize_t Bytes read.
     * @retval -1 Deserialization error. Error may be logged.
     * @retval >=0 Bytes read.
     * @throws "bad_parse" Internal deserialization error.
     * @throws "bad_type" The type of message/field member in buffer does not 
     * match the attribute type of container.
     */
    virtual ssize_t parse(const uint8_t *from, size_t msg_sz) = 0;

    /**
     * @brief Serialize a BGP message *body*.
     * 
     * BgpMessage serializer only serialize message body. (i.e. message without
     * BGP marker and message length field) To serialize a BGP packet, use
     * BgpPacket.
     * 
     * @param to Pointer to destination buffer.
     * @param buf_sz Max write size.
     * @return ssize_t Bytes written.
     * @retval -1 Serialization error. Error may be logged.
     * @retval >=0 Bytes written.
     */
    virtual ssize_t write(uint8_t *to, size_t buf_sz) const = 0;

    uint8_t type;

    virtual ~BgpMessage() {}
};

}
#endif // BGP_MSG_H_