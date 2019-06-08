#include "bgp-keepalive-message.h"
#include "bgp-errcode.h"
#include "bgp-error.h"
#include <unistd.h>

namespace bgpfsm {

BgpKeepaliveMessage::BgpKeepaliveMessage() {
    type = KEEPALIVE;
    err_data = 0;
}

ssize_t BgpKeepaliveMessage::parse(const uint8_t *from, size_t msg_sz) {
    if (msg_sz != 0) {
        err_data = msg_sz + 19;
        return -1; // KEEPALIVE can't have message body
    }

    return 0;
}

ssize_t BgpKeepaliveMessage::write(uint8_t *to, size_t msg_sz) const {
    return 0;
}

uint8_t BgpKeepaliveMessage::getErrorCode() const {
    if (err_data > 0) return E_HEADER;
    return 0;
}

uint8_t BgpKeepaliveMessage::getErrorSubCode() const { 
    if (err_data > 0) return E_LENGTH;
    return 0;
}

const uint8_t* BgpKeepaliveMessage::getError() const { 
    if (err_data > 0) return &err_data;
    return NULL;
}

size_t BgpKeepaliveMessage::getErrorLength() const { 
    if (err_data > 0) return 1;
    return 0;
}

}