#ifndef BGP_CAP_H_
#define BGP_CAP_H_
#include "serializable.h"
#include <stdint.h>

namespace bgpfsm {

enum BgpCapabilityCode {
    MP_BGP = 1,
    ROUTE_REFRESH = 2,
    ORF = 3,
    EXTENDED_NEXTHOP = 5,
    BGPSEC = 7,
    MULTIPLE_LABELS = 8,
    GRACEFUL_RESTART = 64,
    ASN_4B = 65,
    ADD_PATH = 69,
    ENHANCED_ROUTE_REFRESH = 70
};

class BgpCapability : public Serializable {
public:
    BgpCapability();

    uint8_t code;
    virtual ~BgpCapability() {}
protected:

    ssize_t parseHeader(const uint8_t *from, size_t msg_sz);

    // length field is only used in parse to pass length to lower-level parser.
    // length field is ignored when serializing.
    uint8_t length;
};

class BgpCapability4BytesAsn : public BgpCapability {
public:
    BgpCapability4BytesAsn();

    ssize_t doPrint(size_t indent, uint8_t **to, size_t *buf_sz) const;
    ssize_t parse(const uint8_t *from, size_t msg_sz);
    ssize_t write(uint8_t *to, size_t buf_sz) const;

    uint32_t my_asn;
};

class BgpCapabilityUnknow : public BgpCapability {
public:
    BgpCapabilityUnknow();
    ~BgpCapabilityUnknow();

    ssize_t doPrint(size_t indent, uint8_t **to, size_t *buf_sz) const;
    ssize_t parse(const uint8_t *from, size_t msg_sz);
    ssize_t write(uint8_t *to, size_t buf_sz) const;

private:
    uint8_t *value;
};

}

#endif // BGP_CAP_H_