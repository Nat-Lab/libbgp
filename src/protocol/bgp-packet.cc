#include "bgp-packet.h"
#include "bgp.h"
#include "bgp-error.h"
#include "value-op.h"
#include <string.h>
#include <arpa/inet.h>
#include <assert.h>

namespace bgpfsm {

BgpPacket::BgpPacket(bool is_4b) {
    msg = NULL;
    m_msg = NULL;
    is_message_owner = true;
    this->is_4b = is_4b;
}

BgpPacket::BgpPacket(bool is_4b, const BgpMessage *msg) {
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

ssize_t BgpPacket::parse(const uint8_t *from, size_t buf_sz) {
    assert(buf_sz >= 19 && buf_sz <= 4096);
    assert(m_msg == NULL && msg == NULL);
    assert(is_message_owner);

    const uint8_t *buffer = from + 18;
    uint8_t msg_type = getValue<uint8_t>(&buffer);

    switch (msg_type) {
        case OPEN: m_msg = new BgpOpenMessage(is_4b); break;
        case UPDATE: m_msg = new BgpUpdateMessage(is_4b); break;
        case KEEPALIVE: m_msg = new BgpKeepaliveMessage(); break;
        case NOTIFICATION: m_msg = new BgpNotificationMessage(); break;
        default: m_msg = new BgpBadMessage(msg_type); break;
    }

    size_t msg_sz = buf_sz - 19;
    ssize_t parsed_len = m_msg->parse(buffer, msg_sz);

    if (parsed_len < 0) {
        forwardParseError(*m_msg);
        return parsed_len;
    }

    assert(msg_sz == (size_t) parsed_len);
    return parsed_len + 19;
}

ssize_t BgpPacket::write(uint8_t *to, size_t buf_sz) const {
    if (is_message_owner) assert(m_msg != NULL);
    else assert(msg != NULL);

    if (buf_sz < 19) {
        _bgp_error("BgpPacket::write: dst buffer too small.\n");
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

const BgpMessage *BgpPacket::getMessage() const {
    return is_message_owner ? m_msg : msg;
}

}