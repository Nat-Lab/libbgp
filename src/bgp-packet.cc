/**
 * @file bgp-packet.cc
 * @author Nato Morichika <nat@nat.moe>
 * @brief Top level deserialization/serialization entry point for BGP messages.
 * @version 0.1
 * @date 2019-07-06
 * 
 * @copyright Copyright (c) 2019
 * 
 */
#include "bgp-packet.h"
#include "bgp.h"
#include "value-op.h"
#include <string.h>
#include <arpa/inet.h>

namespace libbgp {

/**
 * @brief Construct a new BgpPacket object for deserializing BGP message.
 * 
 * @param logger Pointer to logger object for error logging.
 * @param is_4b Enable four octets ASN support.
 */
BgpPacket::BgpPacket(BgpLogHandler *logger, bool is_4b) : Serializable(logger) {
    msg = NULL;
    m_msg = NULL;
    is_message_owner = true;
    this->is_4b = is_4b;
}

/**
 * @brief Construct a new BgpPacket object for serializing BGP message.
 * 
 * @param logger Pointer to logger object for error logging.
 * @param is_4b Enable four octets ASN support.
 * @param msg The message to be serialized
 */
BgpPacket::BgpPacket(BgpLogHandler *logger, bool is_4b, const BgpMessage *msg) : Serializable(logger) {
    this->msg = msg;
    m_msg = NULL;
    this->is_4b = is_4b;
    is_message_owner = false;
}

BgpPacket::~BgpPacket() {
    if (m_msg != NULL && is_message_owner) delete m_msg;
}

ssize_t BgpPacket::doPrint(size_t indent, uint8_t **to, size_t *buf_sz) const {
    ssize_t written = -1;
    
    if (is_message_owner) written = m_msg->print(indent, *to, *buf_sz);
    else written = msg->print(indent, *to, *buf_sz);
    if (written < 0) return 0;

    *to += written;
    *buf_sz += written;
    return written;
}

/**
 * @brief Deserialize a BGP message.
 * 
 * @param from Pointer to packet buffer.
 * @param msg_sz Size of packet.
 * @return ssize_t Bytes read.
 * @retval -1 Deserialization error. Error may be logged.
 * @retval >=0 Bytes read.
 * @throws "bad_parse" Internal deserialization error.
 * @throws "bad_type" The type of message/field member in buffer does not 
 * match the attribute type of container.
 * @throws "invalid_op" Invalid operation.
 */
ssize_t BgpPacket::parse(const uint8_t *from, size_t buf_sz) {
    if (buf_sz < 19 && buf_sz > 4096) {
        logger->log(ERROR, "BgpPacket::parse: got a packet with invalid size.\n");
        return -1;
    }

    if (!is_message_owner) {
        logger->log(FATAL, "BgpPacket::parse: can't parse: read-only packet.\n");
        throw "invalid_op";
    }

    if (is_message_owner && m_msg != NULL) {
        logger->log(FATAL, "BgpPacket::parse: can't parse: message pointer not NULL.\n");
        throw "invalid_op";
    }

    const uint8_t *buffer = from + 18;
    uint8_t msg_type = getValue<uint8_t>(&buffer);

    switch (msg_type) {
        case OPEN: m_msg = new BgpOpenMessage(logger, is_4b); break;
        case UPDATE: m_msg = new BgpUpdateMessage(logger, is_4b); break;
        case KEEPALIVE: m_msg = new BgpKeepaliveMessage(logger); break;
        case NOTIFICATION: m_msg = new BgpNotificationMessage(logger); break;
        default: m_msg = new BgpBadMessage(logger, msg_type); break;
    }

    size_t msg_sz = buf_sz - 19;
    ssize_t parsed_len = m_msg->parse(buffer, msg_sz);

    if (parsed_len < 0) {
        forwardParseError(*m_msg);
        return parsed_len;
    }

    if (msg_sz != (size_t) parsed_len) {
        logger->log(FATAL, "BgpPacket::parse: parsed message length invalid but no error reported.\n");
        throw "bad_parse";
    }

    return parsed_len + 19;
}

/**
 * @brief Serialize a BGP message.
 * 
 * @param from Pointer to packet buffer.
 * @param msg_sz Size of packet.
 * @return ssize_t Bytes written.
 * @retval -1 Serialization error. Error may be logged.
 * @retval >=0 Bytes written.
 * @throws "invalid_op" Invalid operation.
 */
ssize_t BgpPacket::write(uint8_t *to, size_t buf_sz) const {
    if (is_message_owner && m_msg == NULL) {
        logger->log(FATAL, "BgpPacket::write: can't write: message pointer NULL.\n");
        throw "invalid_op";
    }

    if (!is_message_owner && msg == NULL) {
        logger->log(FATAL, "BgpPacket::write: can't write: message pointer NULL.\n");
        throw "invalid_op";
    }

    if (buf_sz < 19) {
        logger->log(ERROR, "BgpPacket::write: dst buffer too small.\n");
        return -1;
    }

    uint8_t *buffer = to;
    memset(buffer, '\xff', 16);
    buffer += 16;

    // we do not know length yet, just keep a pointer to the field
    uint8_t *pkt_len_ptr = buffer;
    // and skip ahread.
    buffer += 2;

    putValue<uint8_t> (&buffer, is_message_owner ? m_msg->type : msg->type);
    ssize_t msg_len = -1;
    
    if (is_message_owner) msg_len = m_msg->write(buffer, buf_sz - 19);
    else msg_len = msg->write(buffer, buf_sz - 19);

    if (msg_len < 0) return msg_len;

    size_t pkt_len = msg_len + 19;
    putValue<uint16_t>(&pkt_len_ptr, htons(pkt_len));

    return pkt_len;
}

/**
 * @brief Get pointer to the contained message.
 * 
 * @return const BgpMessage* Pointer to the message.
 */
const BgpMessage *BgpPacket::getMessage() const {
    return is_message_owner ? m_msg : msg;
}

}