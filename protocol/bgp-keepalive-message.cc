#include "bgp-keepalive-message.h"
#include "bgp-errcode.h"
#include "bgp-error.h"
#include <unistd.h>

namespace bgpfsm {

BgpKeepaliveMessage::BgpKeepaliveMessage() {
    type = KEEPALIVE;
}

ssize_t BgpKeepaliveMessage::parse(const uint8_t *from, size_t msg_sz) {
    if (msg_sz != 0) {
        size_t err_len = msg_sz + 19;
        setError(E_HEADER, E_LENGTH, (uint8_t *) &err_len, 1);
        return -1; // KEEPALIVE can't have message body
    }

    return 0;
}

ssize_t BgpKeepaliveMessage::write(uint8_t *to, size_t msg_sz) const {
    return 0;
}

ssize_t BgpKeepaliveMessage::doPrint(size_t indent, uint8_t **to, size_t *buf_sz) const {
    return _print(indent, to, buf_sz, "KeepaliveMessage { }\n");
}

}