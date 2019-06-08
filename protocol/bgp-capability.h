#ifndef BGP_CAP_H_
#define BGP_CAP_H_
#include <stdint.h>

namespace bgpfsm {

enum BgpCapabilityCode {
    RESERVED = 0,
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

class BgpCapability {
public:
    BgpCapabilityCode code;
    uint8_t length;
    virtual ~BgpCapability() {}
};

class Bgp4BytesAsnCapability : public BgpCapability {
public:
    uint32_t my_asn;
};

class BgpUnknowCapability : public BgpCapability {
public:
    uint8_t *value;
};

}

#endif // BGP_CAP_H_