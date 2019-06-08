#include "bgp-keepalive-message.h"
#include "bgp-error.h"
#include <unistd.h>

namespace bgpfsm {

BgpKeepaliveMessage::BgpKeepaliveMessage() {}

ssize_t BgpKeepaliveMessage::parse(const uint8_t *from, size_t msg_sz) {
    if (msg_sz != 0) return -1; // KEEPALIVE can't have message body
    return 0;

}

ssize_t BgpKeepaliveMessage::write(uint8_t *to, size_t msg_sz) const {
    return 0;
}

uint8_t BgpKeepaliveMessage::getErrorCode() const { return 0; }
uint8_t BgpKeepaliveMessage::getErrorSubCode() const { return 0; }
const uint8_t* BgpKeepaliveMessage::getError() const { return NULL; }
size_t BgpKeepaliveMessage::getErrorLength() const { return 0; }

}