#include "bgp-path-attrib.h"
#include "bgp-error.h"
#include "value-op.h"
#include <stdlib.h>

namespace bgpfsm {

int8_t BgpPathAttrib::getType(const uint8_t *from, size_t buffer_sz) {
    if (buffer_sz < 3) return -1;
    return *((uint8_t *) (from + 1));
}

ssize_t BgpPathAttrib::parseHeader(const uint8_t *from, size_t buffer_sz) {
    if (buffer_sz < 2) {
        _bgp_error("BgpPathAttrib::parseHeader: invalid attribute header size.\n");
        return -1;
    }
    const uint8_t *buffer = from;

    uint8_t flags = getValue<uint8_t>(&buffer);

    optional = (flags >> 7) & 0x1;
    transitive = (flags >> 6) & 0x1;
    partial = (flags >> 5) & 0x1;
    extened = (flags >> 4) & 0x1;
    type = getValue<uint8_t>(&buffer);

    return 2;
}

ssize_t BgpPathAttrib::writeHeader(uint8_t *to, size_t buffer_sz) const {
    if (buffer_sz < 2) {
        _bgp_error("BgpPathAttrib::writeHeader: invalid attribute header size.\n");
        return -1;
    }
    
    uint8_t *buffer = to;
    uint8_t flags = (optional << 7) | (transitive << 6)| (partial << 5) | (extened << 4);
    putValue<uint8_t>(&buffer, flags);
    putValue<uint8_t>(&buffer, type);
    return 2;
}

BgpPathAttribUnknow::BgpPathAttribUnknow() {}

BgpPathAttribUnknow::~BgpPathAttribUnknow() {
    if (value_len > 0) free(value_ptr);
}

ssize_t BgpPathAttribUnknow::parse(const uint8_t *from, size_t length) {
    if (parseHeader(from, 2) != 2) return -1;

    const uint8_t *buffer = from + 2;

    value_len = getValue<uint8_t>(&buffer);
    if (value_len != length - 3) {
        _bgp_error("BgpPathAttribUnknow::parse: value_length (%d) != buffer left (%d).\n", value_len, length - 3);
        return -1;
    }

    value_ptr = (uint8_t *) malloc(value_len);
    memcpy(value_ptr, buffer, value_len);

    return length;
}

ssize_t BgpPathAttribUnknow::write(uint8_t *to, size_t buffer_sz) const {
    if (buffer_sz < 3) {
        _bgp_error("BgpPathAttribUnknow::write: destination buffer size too small.\n");
        return -1;
    }

    if (writeHeader(to, 2) != 2) return -1;

    uint8_t *buffer = to + 2;
    putValue<uint8_t>(&buffer, value_len);

    if (value_len > 0) memcpy(buffer, value_ptr, value_len);

    return value_len + 2;   
}

}