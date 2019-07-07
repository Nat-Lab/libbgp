/**
 * @file bgp-capability.cc
 * @author Nato Morichika <nat@nat.moe>
 * @brief BGP Capabilities.
 * @version 0.1
 * @date 2019-07-04
 * 
 * @copyright Copyright (c) 2019
 * 
 */
#include "bgp-capability.h"
#include "bgp-errcode.h"
#include "value-op.h"
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>

namespace libbgp {

/**
 * @brief Construct a new Bgp Capability:: Bgp Capability object
 * 
 * @param logger Pointer to logger object for error logging.
 */
BgpCapability::BgpCapability(BgpLogHandler *logger) : Serializable(logger) {
    length = 0;
}

/**
 * @brief Parse the capability header (code, length).
 * 
 * @param from Pointer to buffer.
 * @param msg_sz Max read size.
 * @return ssize_t Bytes read.
 * @retval -1 Failed to deserialize header. errbuf may have details. (see 
 * bgp-error.h)
 * @retval >=0 Bytes read.
 */
ssize_t BgpCapability::parseHeader(const uint8_t *from, size_t msg_sz) {
    if (msg_sz < 2) {
        setError(E_OPEN, E_UNSPEC_OPEN, NULL, 0);
        logger->log(ERROR, "BgpCapability::parseHeader: unexpected end of capability.\n");
        return -1;
    }

    code = getValue<uint8_t>(&from);
    length = getValue<uint8_t>(&from);

    if ((size_t) (length + 2) > msg_sz) {
        setError(E_OPEN, E_UNSPEC_OPEN, NULL, 0);
        logger->log(ERROR, "BgpCapability::parseHeader: capability size exceed capabilities list.\n");
        return -1;
    }

    return 2;
}

/**
 * @brief Construct a new Bgp Capability 4 Bytes Asn:: Bgp Capability 4 Bytes Asn object
 * 
 * @param logger Logger.
 */
BgpCapability4BytesAsn::BgpCapability4BytesAsn(BgpLogHandler *logger) : BgpCapability(logger) {
    my_asn = 0;
    code = ASN_4B;
}

ssize_t BgpCapability4BytesAsn::doPrint(size_t indent, uint8_t **to, size_t *buf_sz) const {
    ssize_t written = 0;
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

    if (code != ASN_4B) {
        logger->log(FATAL, "BgpCapability4BytesAsn::parse: typecode mismatch with object type.\n");
        throw "bad_type";
    }

    if (hdr_len < 0) return hdr_len;
    if (length != 4) {
        setError(E_OPEN, E_UNSPEC_OPEN, NULL, 0);
        logger->log(ERROR, "BgpCapability4BytesAsn::parse: bad length field (saw %d, want 4).\n", length);
        return -1;
    }

    const uint8_t *buffer = from + hdr_len;
    my_asn = ntohl(getValue<uint32_t>(&buffer));

    return hdr_len + 4;
}

ssize_t BgpCapability4BytesAsn::write(uint8_t *to, size_t buf_sz) const {
    if (buf_sz < 6) {
        logger->log(ERROR, "BgpCapability4BytesAsn::write: dest buffer too small.\n");
        return -1;
    }

    uint8_t *buffer = to;
    putValue<uint8_t>(&buffer, ASN_4B);
    putValue<uint8_t>(&buffer, 4);
    putValue<uint32_t>(&buffer, htonl(my_asn));

    return 6;
}

/**
 * @brief Construct a new Bgp Capability Unknow:: Bgp Capability Unknow object
 * 
 * @param logger Pointer to logger object for error logging.
 */
BgpCapabilityUnknow::BgpCapabilityUnknow(BgpLogHandler *logger) : BgpCapability(logger) {
    value = NULL;
}

/**
 * @brief Destroy the Bgp Capability Unknow:: Bgp Capability Unknow object
 * 
 */
BgpCapabilityUnknow::~BgpCapabilityUnknow() {
    if (length > 0 && value != NULL) free(value);
}

ssize_t BgpCapabilityUnknow::doPrint(size_t indent, uint8_t **to, size_t *buf_sz) const {
    ssize_t written = 0;
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
    if (length == 0) return hdr_len;

    value = (uint8_t *) malloc(length);
    memcpy(value, from + hdr_len, length);

    return hdr_len + length;
}

ssize_t BgpCapabilityUnknow::write(uint8_t *to, size_t buf_sz) const {
    if (buf_sz < (size_t) (length + 2)) {
        logger->log(ERROR, "BgpCapabilityUnknow::write: dest buffer too small.\n");
        return -1;
    }

    uint8_t *buffer = to;
    putValue<uint8_t>(&buffer, code);
    putValue<uint8_t>(&buffer, length);
    if (length == 0) return 2;
    if (value == NULL) {
        logger->log(ERROR, "BgpCapabilityUnknow: missing value pointer.\n");
        return -1;
    }
    memcpy(buffer, value, length);

    return length + 2;
}

}