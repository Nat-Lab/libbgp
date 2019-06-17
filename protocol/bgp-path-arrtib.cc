#include "bgp-path-attrib.h"
#include "bgp-error.h"
#include "bgp-errcode.h"
#include "value-op.h"
#include <stdlib.h>
#include <assert.h>

namespace bgpfsm {

int8_t BgpPathAttrib::getType(const uint8_t *from, size_t buffer_sz) {
    if (buffer_sz < 3) return -1;
    return *((uint8_t *) (from + 1));
}

BgpPathAttrib::BgpPathAttrib() {
    err_buf_len = 0;
    err_code = 0;
    err_subcode = 0;
    err_buf = NULL;
}

BgpPathAttrib::~BgpPathAttrib() {
    if (err_buf_len > 0) free(err_buf);
}

ssize_t BgpPathAttrib::parseHeader(const uint8_t *from, size_t buffer_sz) {
    // header has the length of 3 (flag, type, length). parseHeader only uses 2,
    // but since a packet of size < 3 can't be valid we will reject it here.
    if (buffer_sz < 3) {
        setError(E_UPDATE, E_UNSPEC_UPDATE, NULL, 0);
        _bgp_error("BgpPathAttrib::parseHeader: invalid attribute header size.\n");
        return -1;
    }
    const uint8_t *buffer = from;

    uint8_t flags = getValue<uint8_t>(&buffer);

    optional = (flags >> 7) & 0x1;
    transitive = (flags >> 6) & 0x1;
    partial = (flags >> 5) & 0x1;
    extened = (flags >> 4) & 0x1;
    type_code = getValue<uint8_t>(&buffer);
    value_len = getValue<uint8_t>(&buffer);

    if (value_len > buffer_sz - 3) {
        err_code = E_UPDATE;
        // This is kind of "invalid length", but we are not using E_ATTR_LEN.
        // E_ATTR_LEN: "Attribute Length that conflict with the expected length
        // (based on the attribute type code)." This is not based on type code,
        // but it is buffer overflow, so we set subcode to E_UNSPEC,
        setError(E_UPDATE, E_UNSPEC_UPDATE, NULL, 0);
        _bgp_error("BgpPathAttrib::parseHeader: value_length (%d) < buffer left (%d).\n", value_len, buffer_sz - 3);
        return -1;
    }

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
    putValue<uint8_t>(&buffer, type_code);
    return 2;
}

void BgpPathAttrib::setError(uint8_t err, uint8_t suberr, const uint8_t *data, size_t data_len) {
    err_code = err;
    err_subcode = suberr;

    // err_buf_len not 0, setError() when error already there?
    assert(err_buf_len == 0);

    if (data_len == 0) return;
    err_buf_len = data_len;
    err_buf = (uint8_t *) malloc(err_buf_len);
    memcpy(err_buf, data, data_len);
}

uint8_t BgpPathAttrib::getErrorCode() const {
    return err_code;
}

uint8_t BgpPathAttrib::getErrorSubCode() const {
    return err_subcode;
}

const uint8_t* BgpPathAttrib::getError() const {
    return err_buf;
}

size_t BgpPathAttrib::getErrorLength() const {
    return err_buf_len;
}

BgpPathAttribUnknow::BgpPathAttribUnknow() {}

BgpPathAttribUnknow::~BgpPathAttribUnknow() {
    if (value_len > 0) free(value_ptr);
}

ssize_t BgpPathAttribUnknow::parse(const uint8_t *from, size_t length) {
    if (parseHeader(from, length) != 3) return -1;

    const uint8_t *buffer = from + 2;

    // Well-Known, Mandatory = !optional, transitive
    // Well-Known, Discretionary = !optional, !transitive
    // Optional, Transitive = optional, transitive
    // Optional, Non-Transitive = optional, !transitive
    if (!optional && transitive) {
        // well-known mandatory, but not recognized
        setError(E_UPDATE, E_BAD_WELL_KNOWN, from, value_len + 3);
        _bgp_error("BgpPathAttribUnknow::parse: flag indicates well-known, mandatory but this attribute is unknown.\n");
        // set value_len = 0, so we won't free() nullptr when destruct.
        value_len = 0; 
        return -1;
    }

    value_ptr = (uint8_t *) malloc(value_len);
    memcpy(value_ptr, buffer, value_len);

    return length;
}

ssize_t BgpPathAttribUnknow::write(uint8_t *to, size_t buffer_sz) const {
    if (buffer_sz < value_len + 3) {
        _bgp_error("BgpPathAttribUnknow::write: destination buffer size too small.\n");
        return -1;
    }

    if (writeHeader(to, 2) != 2) return -1;

    uint8_t *buffer = to + 2;
    putValue<uint8_t>(&buffer, value_len);

    if (value_len > 0) memcpy(buffer, value_ptr, value_len);

    return value_len + 3;   
}

BgpPathAttribOrigin::BgpPathAttribOrigin() {}

ssize_t BgpPathAttribOrigin::parse(const uint8_t *from, size_t length) {
    if (parseHeader(from, length) != 3) return -1;

    const uint8_t *buffer = from + 2;

    if (value_len < 1) {
        _bgp_error("BgpPathAttribOrigin::parse: incomplete attrib.\n");
        setError(E_UPDATE, E_UNSPEC_UPDATE, NULL, 0);
        return -1;
    }

    if (value_len != 1) {
        _bgp_error("BgpPathAttribOrigin::parse: bad length, want 1, saw %d.\n", value_len);
        setError(E_UPDATE, E_ATTR_LEN, from, 4);
        return -1;
    }

    if (optional || !transitive) {
        _bgp_error("BgpPathAttribOrigin::parse: bad flag bits, must be !optional, transitive.\n");
        setError(E_UPDATE, E_ATTR_FLAG, from , 4);
        return -1;
    }

    origin = getValue<uint8_t>(&buffer);

    if (origin > 2) {
        setError(E_UPDATE, E_ORIGIN, from, 4);
        _bgp_error("BgpPathAttribOrigin::Bad Origin Value: %d.\n", origin);
        return -1;
    }

    return 4;
}

ssize_t BgpPathAttribOrigin::write(uint8_t *to, size_t buffer_sz) const {
    if (buffer_sz < 3) {
        _bgp_error("BgpPathAttribOrigin::write: destination buffer size too small.\n");
        return -1;
    }

    if (writeHeader(to, 2) != 2) return -1;
    uint8_t *buffer = to + 2;

    putValue<uint8_t>(&buffer, 1); // length = 1
    putValue<uint8_t>(&buffer, origin);
    return 4;

}

}