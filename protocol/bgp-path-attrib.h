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
    uint8_t type;

    // get attribute type from buffer, return -1 if failed.
    static int8_t getType(const uint8_t *buffer, size_t buffer_sz);

    // parse attribute 
    virtual ssize_t parse(const uint8_t *buffer, size_t length) = 0;

    // write attribute
    virtual ssize_t write(uint8_t *buffer, size_t buffer_sz) const = 0;

    // get error code
    virtual uint8_t getErrorCode() const = 0;

    // get error subcode
    virtual uint8_t getErrorSubCode() const = 0;

    // get error payload (data field of NOTIFICATION message)
    virtual const uint8_t* getError() const = 0;

    // get length of error payload
    virtual size_t getErrorLength() const = 0;

    virtual ~BgpPathAttrib() {}

protected:
    // utility function to parse header (flags, types) from buffer
    ssize_t parseHeader(const uint8_t *buffer, size_t length);

    // utility function to write header (flags, types) to buffer
    ssize_t writeHeader(uint8_t *buffer, size_t buffer_sz) const;
};

class BgpPathAttribUnknow : public BgpPathAttrib {
public:
    BgpPathAttribUnknow();
    ~BgpPathAttribUnknow();
    uint8_t type_code;
    uint8_t* value_ptr;
    uint8_t value_len;

    ssize_t parse(const uint8_t *buffer, size_t length);
    ssize_t write(uint8_t *buffer, size_t buffer_sz) const;

    uint8_t getErrorCode() const;
    uint8_t getErrorSubCode() const;
    const uint8_t* getError() const;
    size_t getErrorLength() const;
};

enum BgpPathAttribOrigins {
    IGP = 0,
    EGP = 1,
    INCOMPLETE = 2
};

class BgpPathAttribOrigin : public BgpPathAttrib {
public:
    BgpPathAttribOrigin();
    uint8_t origin;

    ssize_t parse(const uint8_t *buffer, size_t length);
    ssize_t write(uint8_t *buffer, size_t buffer_sz) const;

    uint8_t getErrorCode() const;
    uint8_t getErrorSubCode() const;
    const uint8_t* getError() const;
    size_t getErrorLength() const;
};

class BgpPathAttribAsPath : public BgpPathAttrib {
public:
    BgpPathAttribAsPath();
    std::vector<uint32_t> as_path;

    // is_4b: 4b ASN in AS_PATH?
    bool is_4b;

    ssize_t parse(const uint8_t *buffer, size_t length);
    ssize_t write(uint8_t *buffer, size_t buffer_sz) const;

    uint8_t getErrorCode() const;
    uint8_t getErrorSubCode() const;
    const uint8_t* getError() const;
    size_t getErrorLength() const;
};

class BgpPathAttribNexthop : public BgpPathAttrib {
public:
    BgpPathAttribNexthop();
    uint32_t next_hop;

    ssize_t parse(const uint8_t *buffer, size_t length);
    ssize_t write(uint8_t *buffer, size_t buffer_sz) const;

    uint8_t getErrorCode() const;
    uint8_t getErrorSubCode() const;
    const uint8_t* getError() const;
    size_t getErrorLength() const;
};

class BgpPathAttribMed : public BgpPathAttrib {
public:
    BgpPathAttribMed();
    uint32_t med;

    ssize_t parse(const uint8_t *buffer, size_t length);
    ssize_t write(uint8_t *buffer, size_t buffer_sz) const;

    uint8_t getErrorCode() const;
    uint8_t getErrorSubCode() const;
    const uint8_t* getError() const;
    size_t getErrorLength() const;
};

class BgpPathAttribLocalPref : public BgpPathAttrib {
public:
    BgpPathAttribLocalPref();
    uint32_t local_pref;

    ssize_t parse(const uint8_t *buffer, size_t length);
    ssize_t write(uint8_t *buffer, size_t buffer_sz) const;

    uint8_t getErrorCode() const;
    uint8_t getErrorSubCode() const;
    const uint8_t* getError() const;
    size_t getErrorLength() const;
};

class BgpPathAttribAtomicAggregate : public BgpPathAttrib {
public:
    BgpPathAttribAtomicAggregate();
    bool atomic_aggregate;

    ssize_t parse(const uint8_t *buffer, size_t length);
    ssize_t write(uint8_t *buffer, size_t buffer_sz) const;

    uint8_t getErrorCode() const;
    uint8_t getErrorSubCode() const;
    const uint8_t* getError() const;
    size_t getErrorLength() const;
};

class BgpPathAttribAggregator : public BgpPathAttrib {
public:
    BgpPathAttribAggregator();
    uint32_t aggregator;
    uint32_t aggregator_asn;

    // is_4b: 4b asn in aggregator?
    bool is_4b;

    ssize_t parse(const uint8_t *buffer, size_t length);
    ssize_t write(uint8_t *buffer, size_t buffer_sz) const;

    uint8_t getErrorCode() const;
    uint8_t getErrorSubCode() const;
    const uint8_t* getError() const;
    size_t getErrorLength() const;
};

class BgpPathAttribAs4Path : public BgpPathAttrib {
public:
    BgpPathAttribAs4Path();
    std::vector<uint32_t> as4_path;

    ssize_t parse(const uint8_t *buffer, size_t length);
    ssize_t write(uint8_t *buffer, size_t buffer_sz) const;

    uint8_t getErrorCode() const;
    uint8_t getErrorSubCode() const;
    const uint8_t* getError() const;
    size_t getErrorLength() const;
};

class BgpPathAttribAs4Aggregator : public BgpPathAttrib {
public:
    BgpPathAttribAs4Aggregator();
    uint32_t aggregator;
    uint32_t aggregator_asn4;

    ssize_t parse(const uint8_t *buffer, size_t length);
    ssize_t write(uint8_t *buffer, size_t buffer_sz) const;

    uint8_t getErrorCode() const;
    uint8_t getErrorSubCode() const;
    const uint8_t* getError() const;
    size_t getErrorLength() const;
};

class BgpPathAttribCommunity : public BgpPathAttrib {
public:
    BgpPathAttribCommunity();
    uint32_t community;

    ssize_t parse(const uint8_t *buffer, size_t length);
    ssize_t write(uint8_t *buffer, size_t buffer_sz) const;

    uint8_t getErrorCode() const;
    uint8_t getErrorSubCode() const;
    const uint8_t* getError() const;
    size_t getErrorLength() const;
};

}
#endif // BGP_PATH_ATTR_H