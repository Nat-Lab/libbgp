#ifndef BGP_PATH_ATTR_H
#define BGP_PATH_ATTR_H

#include <stdint.h>
#include <vector>

namespace bgpfsm {

typedef struct Route {
    uint8_t length;
    uint32_t prefix;
} Route;

enum BgpPathAttribType {
    ORIGIN,
    AS_PATH,
    NEXTHOP,
    MED,
    LOCAL_PREF,
    ATOMIC_AGGREGATE,
    AGGREATOR,
    AS4_PATH,
    AGGREGATOR4
};

class BgpPathAttrib {
public:
    BgpPathAttribType type;
};

class BgpPathAttribOrigin : public BgpPathAttrib {
public:
    uint8_t origin;
};

class BgpPathAttribAsPath : public BgpPathAttrib {
public:
    std::vector<uint16_t> as_path;
};

class BgpPathAttribNexthop : public BgpPathAttrib {
public:
    uint32_t next_hop;
};

class BgpPathAttribMed : public BgpPathAttrib {
public:
    uint32_t med;
};

class BgpPathAttribLocalPref : public BgpPathAttrib {
public:
    uint32_t local_pref;
};

class BgpPathAttribAtomicAggregate : public BgpPathAttrib {
public:
    bool atomic_aggregate;
};

class BgpPathAttribAggregatorAsn : public BgpPathAttrib {
public:
    uint16_t aggregator_asn;
};

class BgpPathAttribAggregator : public BgpPathAttrib {
public:
    uint32_t aggregator;
};

class BgpPathAttribAsPath : public BgpPathAttrib {
public:
    std::vector<uint16_t> as_path4;
};

class BgpPathAttribAggregatorAsn4 : public BgpPathAttrib {
public:
    uint16_t aggregator_asn4;
};

}
#endif // BGP_PATH_ATTR_H