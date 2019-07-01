#include "bgp-capability.h"
#include "bgp-errcode.h"
#include "bgp-error.h"
#include "value-op.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <arpa/inet.h>

namespace bgpfsm {

BgpCapability::BgpCapability() {
    length = 0;
}

ssize_t BgpCapability::parseHeader(const uint8_t *from, size_t msg_sz) {
    if (msg_sz < 2) {
        setError(E_OPEN, E_UNSPEC_OPEN, NULL, 0);
        _bgp_error("BgpCapability::parseHeader: unexpected end of capability.\n");
        return -1;
    }

    code = getValue<uint8_t>(&from);
    length = getValue<uint8_t>(&from);

    if (length + 2 > msg_sz) {
        setError(E_OPEN, E_UNSPEC_OPEN, NULL, 0);
        _bgp_error("BgpCapability::parseHeader: capability size exceed capabilities list.\n");
        return -1;
    }

    return 2;
}

BgpCapability4BytesAsn::BgpCapability4BytesAsn() {
    my_asn = 0;
    code = ASN_4B;
}

ssize_t BgpCapability4BytesAsn::doPrint(size_t indent, uint8_t **to, size_t *buf_sz) const {
    ssize_t written;
    written += _print(indent, to, buf_sz, "FourOctetAsnCapability {\n");
    indent++; {
        written += _print(indent, to, buf_sz, "Code { %d }\n", code);
        written += _print(indent, to, buf_sz, "MyAsn { %d }\n", my_asn);
    }; indent--;
    written += _print(indent, to, buf_sz, "}\n");

    return written;
}

ssize_t BgpCapability4BytesAsn::parse(const uint8_t *from, size_t msg_sz) {
    ssize_t hdr_len = parseHeader(from, msg_sz);

    assert(code == ASN_4B);

    if (hdr_len < 0) return hdr_len;
    if (length != 4) {
        setError(E_OPEN, E_UNSPEC_OPEN, NULL, 0);
        _bgp_error("BgpCapability4BytesAsn::parse: bad length field (saw %d, want 4).\n", length);
        return -1;
    }

    my_asn = ntohl(getValue<uint32_t>(&from));

    return hdr_len + 4;
}

ssize_t BgpCapability4BytesAsn::write(uint8_t *to, size_t buf_sz) const {
    if (buf_sz < 6) {
        _bgp_error("BgpCapability4BytesAsn::write: dest buffer too small.\n");
        return -1;
    }

    uint8_t *buffer = to;
    putValue<uint8_t>(&buffer, ASN_4B);
    putValue<uint8_t>(&buffer, 4);
    putValue<uint32_t>(&buffer, htonl(my_asn));

    return 6;
}

BgpCapabilityUnknow::BgpCapabilityUnknow() {
    value = NULL;
}

BgpCapabilityUnknow::~BgpCapabilityUnknow() {
    if (length > 0 && value != NULL) free(value);
}

ssize_t BgpCapabilityUnknow::doPrint(size_t indent, uint8_t **to, size_t *buf_sz) const {
    ssize_t written;
    written += _print(indent, to, buf_sz, "UnknowCapability {\n");
    indent++; {
        written += _print(indent, to, buf_sz, "Code { %d }\n", code);
    }; indent--;
    written += _print(indent, to, buf_sz, "}\n");

    return written;
}

ssize_t BgpCapabilityUnknow::parse(const uint8_t *from, size_t msg_sz) {
    ssize_t hdr_len = parseHeader(from, msg_sz);

    if (hdr_len < 0) return hdr_len;

    value = (uint8_t *) malloc(length);
    memcpy(value, from + hdr_len, length);

    return hdr_len + length;
}

ssize_t BgpCapabilityUnknow::write(uint8_t *to, size_t buf_sz) const {
    if (buf_sz < length + 2) {
        _bgp_error("BgpCapabilityUnknow::write: dest buffer too small.\n");
        return -1;
    }

    uint8_t *buffer = to;
    putValue<uint8_t>(&buffer, code);
    putValue<uint8_t>(&buffer, length);
    if (length == 0) return 2;
    if (value == NULL) {
        _bgp_error("BgpCapabilityUnknow: missing value pointer.\n");
        return -1;
    }
    memcpy(buffer, value, length);

    return length + 2;
}




}