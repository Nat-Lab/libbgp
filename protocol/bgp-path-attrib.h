#ifndef BGP_PATH_ATTR_H
#define BGP_PATH_ATTR_H

#include <stdint.h>
#include <unistd.h>
#include <vector>

namespace bgpfsm {

typedef struct Route {
    uint8_t length;
    uint32_t prefix;
} Route;

enum BgpPathAttribType {
    UNKNOW = -1,
    RESERVED = 0,
    ORIGIN = 1,
    AS_PATH = 2,
    NEXT_HOP = 3,
    MULTI_EXIT_DISC = 4,
    LOCAL_PREF = 5,
    ATOMIC_AGGREGATE = 6,
    AGGREATOR = 7,
    COMMUNITY = 8,
    AS4_PATH = 17,
    AS4_AGGREGATOR = 18
    // TODO: RFC4760
};

class BgpPathAttrib {
public:
    bool optional;
    bool transitive;
    bool partial;
    bool extened;
    BgpPathAttribType type;

    virtual ssize_t parse(const uint8_t *buffer, size_t length) = 0;
    virtual ssize_t write(uint8_t *buffer, size_t buffer_sz) const = 0;
    virtual ~BgpPathAttrib() {}
};

class BgpPathAttribUnknow : public BgpPathAttrib {
public:
    BgpPathAttribUnknow();
    ~BgpPathAttribUnknow();
    uint8_t type_code;
    uint8_t* value_ptr;
    size_t value_len;

    ssize_t parse(const uint8_t *buffer, size_t length);
    ssize_t write(uint8_t *buffer, size_t buffer_sz) const;
};

class BgpPathAttribOrigin : public BgpPathAttrib {
public:
    BgpPathAttribOrigin();
    uint8_t origin;

    ssize_t parse(const uint8_t *buffer, size_t length);
    ssize_t write(uint8_t *buffer, size_t buffer_sz) const;
};

class BgpPathAttribAsPath : public BgpPathAttrib {
public:
    BgpPathAttribAsPath();
    std::vector<uint16_t> as_path;

    ssize_t parse(const uint8_t *buffer, size_t length);
    ssize_t write(uint8_t *buffer, size_t buffer_sz) const;
};

class BgpPathAttribNexthop : public BgpPathAttrib {
public:
    BgpPathAttribNexthop();
    uint32_t next_hop;

    ssize_t parse(const uint8_t *buffer, size_t length);
    ssize_t write(uint8_t *buffer, size_t buffer_sz) const;
};

class BgpPathAttribMed : public BgpPathAttrib {
public:
    BgpPathAttribMed();
    uint32_t med;

    ssize_t parse(const uint8_t *buffer, size_t length);
    ssize_t write(uint8_t *buffer, size_t buffer_sz) const;
};

class BgpPathAttribLocalPref : public BgpPathAttrib {
public:
    BgpPathAttribLocalPref();
    uint32_t local_pref;

    ssize_t parse(const uint8_t *buffer, size_t length);
    ssize_t write(uint8_t *buffer, size_t buffer_sz) const;
};

class BgpPathAttribAtomicAggregate : public BgpPathAttrib {
public:
    BgpPathAttribAtomicAggregate();
    bool atomic_aggregate;

    ssize_t parse(const uint8_t *buffer, size_t length);
    ssize_t write(uint8_t *buffer, size_t buffer_sz) const;
};

class BgpPathAttribAggregator : public BgpPathAttrib {
public:
    BgpPathAttribAggregator();
    uint32_t aggregator;
    uint16_t aggregator_asn;

    ssize_t parse(const uint8_t *buffer, size_t length);
    ssize_t write(uint8_t *buffer, size_t buffer_sz) const;
};

class BgpPathAttribAs4Path : public BgpPathAttrib {
public:
    BgpPathAttribAs4Path();
    std::vector<uint32_t> as_path4;

    ssize_t parse(const uint8_t *buffer, size_t length);
    ssize_t write(uint8_t *buffer, size_t buffer_sz) const;
};

class BgpPathAttribAs4Aggregator : public BgpPathAttrib {
public:
    BgpPathAttribAs4Aggregator();
    uint32_t aggregator;
    uint32_t aggregator_asn4;

    ssize_t parse(const uint8_t *buffer, size_t length);
    ssize_t write(uint8_t *buffer, size_t buffer_sz) const;
};

class BgpPathAttribCommunity : public BgpPathAttrib {
public:
    BgpPathAttribCommunity();
    uint32_t community;

    ssize_t parse(const uint8_t *buffer, size_t length);
    ssize_t write(uint8_t *buffer, size_t buffer_sz) const;
};

}
#endif // BGP_PATH_ATTR_H