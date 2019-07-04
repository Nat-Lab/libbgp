#include "bgp-bad-message.h"
#include "bgp-errcode.h"
#include "bgp-error.h"
#include <unistd.h>
#include <assert.h>

namespace libbgp {

BgpBadMessage::BgpBadMessage(uint8_t type) {
    this->type = type;
}

ssize_t BgpBadMessage::parse(__attribute__((unused)) const uint8_t *from, __attribute__((unused)) size_t msg_sz) {
    _bgp_error("BgpBadMessage::parse: unknow message type %d\n", type);
    setError(E_HEADER, E_TYPE, &type, 1);
    return -1;
}

ssize_t BgpBadMessage::write(__attribute__((unused)) uint8_t *to, __attribute__((unused)) size_t msg_sz) const {
    _bgp_error("BgpBadMessage::write: you can't write a bad message\n");
    return -1;
}

ssize_t BgpBadMessage::doPrint(__attribute__((unused)) size_t indent, uint8_t **to, __attribute__((unused)) size_t *buf_sz) const {
    return _print(indent, to, buf_sz, "InvalidMessage { }\n");
}

}