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
    uint8_t type_code;

    BgpPathAttrib();

    // get attribute type from buffer, return -1 if failed.
    static int8_t getType(const uint8_t *buffer, size_t buffer_sz);

    // parse attribute 
    virtual ssize_t parse(const uint8_t *buffer, size_t length) = 0;

    // write attribute
    virtual ssize_t write(uint8_t *buffer, size_t buffer_sz) const = 0;

    // get error code
    virtual uint8_t getErrorCode() const;

    // get error subcode
    virtual uint8_t getErrorSubCode() const;

    // get error payload (data field of NOTIFICATION message)
    virtual const uint8_t* getError() const;

    // get length of error payload
    virtual size_t getErrorLength() const;

    virtual ~BgpPathAttrib();

protected:
    // utility function to parse header (flags, typecode, length) from buffer
    ssize_t parseHeader(const uint8_t *buffer, size_t length);

    // utility function to write header (flags, type_code) to buffer. notice
    // that length is not write to buffer by this function.
    ssize_t writeHeader(uint8_t *buffer, size_t buffer_sz) const;

    // utility function to error related values
    void setError(uint8_t err, uint8_t suberr, const uint8_t *data, size_t data_len);

    // the length of attribute value, use only when parse. when write(), this
    // value is ignored.
    uint16_t value_len;

    uint8_t err_code;
    uint8_t err_subcode;
    size_t err_buf_len;
    uint8_t *err_buf;
};

class BgpPathAttribUnknow : public BgpPathAttrib {
public:
    BgpPathAttribUnknow();
    ~BgpPathAttribUnknow();
    uint8_t* value_ptr;

    ssize_t parse(const uint8_t *buffer, size_t length);
    ssize_t write(uint8_t *buffer, size_t buffer_sz) const;
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
};

enum BgpAsPathSegmentType {
    AS_SET = 1,
    AS_SEQUENCE = 2
};

class BgpAsPathSegment {
public:
    bool is_4b;
    uint8_t type;
    virtual size_t getCount() const = 0;
    virtual bool prepend(uint32_t asn) = 0;
    virtual ~BgpAsPathSegment() {}
};

class BgpAsPathSegment2b : public BgpAsPathSegment {
public:
    size_t getCount() const;
    bool prepend(uint32_t asn);
    BgpAsPathSegment2b(uint8_t type);
    std::vector<uint16_t> value;
};

class BgpAsPathSegment4b : public BgpAsPathSegment {
public:
    size_t getCount() const;
    bool prepend(uint32_t asn);
    BgpAsPathSegment4b(uint8_t type);
    std::vector<uint32_t> value;
};

class BgpPathAttribAsPath : public BgpPathAttrib {
public:
    // is_4b: 4b ASN in AS_PATH?
    BgpPathAttribAsPath(bool is_4b);

    std::vector<BgpAsPathSegment> as_paths;
    bool is_4b;

    // prepend: utility function to prepend an ASN to path
    bool prepend(uint32_t asn);

    ssize_t parse(const uint8_t *buffer, size_t length);
    ssize_t write(uint8_t *buffer, size_t buffer_sz) const;
private:
    // addSeg: add a new AS_SEQUENCE with one ASN to AS_PATH 
    void addSeg(uint32_t asn);
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
    // is_4b: 4b asn in aggregator?
    BgpPathAttribAggregator(bool is_4b);
    uint32_t aggregator;
    uint32_t aggregator_asn;

    bool is_4b;

    ssize_t parse(const uint8_t *buffer, size_t length);
    ssize_t write(uint8_t *buffer, size_t buffer_sz) const;
};

class BgpPathAttribAs4Path : public BgpPathAttrib {
public:
    BgpPathAttribAs4Path();
    std::vector<BgpAsPathSegment4b> as4_paths;

    void prepend(uint32_t asn);

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