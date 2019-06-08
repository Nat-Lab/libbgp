#include "bgp-notification-message.h"
#include "value-op.h"
#include <stdlib.h>
#include <string.h>

namespace bgpfsm {

BgpNotificationMessage::BgpNotificationMessage() {}

BgpNotificationMessage::~BgpNotificationMessage() {
    if (data_len > 0) free(data);
}

ssize_t BgpNotificationMessage::parse(const uint8_t *from, size_t msg_sz) {
    if (msg_sz < 2) return -1;
    const uint8_t *buffer = from;

    errcode = getValue<uint8_t>(&buffer);
    subcode = getValue<uint8_t>(&buffer);
    
    if (msg_sz > 2) {
        data_len = msg_sz - 2;
        data = (uint8_t *) malloc(data_len);
        memcpy(data, buffer, data_len);
    }

    return msg_sz;
}

ssize_t BgpNotificationMessage::write(uint8_t *to, size_t buf_sz) const {
    if (buf_sz < data_len + 2) return -1;
    uint8_t *buffer = to;

    putValue<uint8_t>(&buffer, errcode);
    putValue<uint8_t>(&buffer, subcode);
    memcpy(buffer, data, data_len);

    return data_len + 2;
}

}