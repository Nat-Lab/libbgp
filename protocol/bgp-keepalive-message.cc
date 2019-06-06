#include "bgp-keepalive-message.h"
#include "bgp-error.h"
#include <unistd.h>

namespace bgpfsm {

BgpKeepaliveMessage::BgpKeepaliveMessage() {}

ssize_t BgpKeepaliveMessage::parse (const uint8_t *from, size_t msg_sz) {

}

ssize_t BgpKeepaliveMessage::write(uint8_t *to, size_t msg_sz) const {
    
}

}