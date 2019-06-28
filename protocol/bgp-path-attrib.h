#ifndef BGP_PATH_ATTR_H
#define BGP_PATH_ATTR_H

#include "serializable.h"
#include <stdint.h>
#include <unistd.h>
#include <assert.h>
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

class BgpPathAttrib : public Serializable {
public:
    bool optional;
    bool transitive;
    bool partial;
    bool extended;
    uint8_t type_code;

    BgpPathAttrib();
    BgpPathAttrib(const uint8_t *value, uint16_t val_len);

    // get attribute type from buffer, return -1 if failed.
    static int8_t GetTypeFromBuffer(const uint8_t *buffer, size_t buffer_sz);

    // print the attribute
    ssize_t print(size_t indent, uint8_t *to, size_t buf_sz) const;

    // parse attribute 
    ssize_t parse(const uint8_t *buffer, size_t length);

    // write attribute
    ssize_t write(uint8_t *buffer, size_t buffer_sz) const;

    // clone the attribute
    virtual BgpPathAttrib* clone() const;

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

    // utility function to print flags
    ssize_t printFlags(size_t indent, uint8_t **to, size_t *buf_sz) const;

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

private:
    uint8_t* value_ptr;
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
    
    BgpPathAttrib* clone() const;
    ssize_t parse(const uint8_t *buffer, size_t length);
    ssize_t write(uint8_t *buffer, size_t buffer_sz) const;
    ssize_t print(size_t indent, uint8_t *to, size_t buf_sz);
};

enum BgpAsPathSegmentType {
    AS_SET = 1,
    AS_SEQUENCE = 2
};

class BgpAsPathSegment {
public:
    BgpAsPathSegment(bool is_4b, uint8_t type);
    bool is_4b;
    uint8_t type;
    size_t getCount() const;
    bool prepend(uint32_t asn);
    std::vector<uint32_t> value;
};

class BgpPathAttribAsPath : public BgpPathAttrib {
public:
    // is_4b: 4b ASN in AS_PATH?
    BgpPathAttribAsPath(bool is_4b);

    BgpPathAttrib* clone() const;
    std::vector<BgpAsPathSegment> as_paths;
    bool is_4b;

    // prepend: utility function to prepend an ASN to path
    bool prepend(uint32_t asn);

    ssize_t parse(const uint8_t *buffer, size_t length);
    ssize_t write(uint8_t *buffer, size_t buffer_sz) const;
    ssize_t print(size_t indent, uint8_t *to, size_t buf_sz);
private:
    // addSeg: add a new AS_SEQUENCE with one ASN to AS_PATH 
    void addSeg(uint32_t asn);
};

class BgpPathAttribNexthop : public BgpPathAttrib {
public:
    BgpPathAttribNexthop();
    uint32_t next_hop;

    BgpPathAttrib* clone() const;
    ssize_t parse(const uint8_t *buffer, size_t length);
    ssize_t write(uint8_t *buffer, size_t buffer_sz) const;
    ssize_t print(size_t indent, uint8_t *to, size_t buf_sz);
};

class BgpPathAttribMed : public BgpPathAttrib {
public:
    BgpPathAttribMed();
    uint32_t med;

    BgpPathAttrib* clone() const;
    ssize_t parse(const uint8_t *buffer, size_t length);
    ssize_t write(uint8_t *buffer, size_t buffer_sz) const;
    ssize_t print(size_t indent, uint8_t *to, size_t buf_sz);
};

class BgpPathAttribLocalPref : public BgpPathAttrib {
public:
    BgpPathAttribLocalPref();
    uint32_t local_pref;

    BgpPathAttrib* clone() const;
    ssize_t parse(const uint8_t *buffer, size_t length);
    ssize_t write(uint8_t *buffer, size_t buffer_sz) const;
    ssize_t print(size_t indent, uint8_t *to, size_t buf_sz);
};

class BgpPathAttribAtomicAggregate : public BgpPathAttrib {
public:
    BgpPathAttribAtomicAggregate();

    BgpPathAttrib* clone() const;
    ssize_t parse(const uint8_t *buffer, size_t length);
    ssize_t write(uint8_t *buffer, size_t buffer_sz) const;
    ssize_t print(size_t indent, uint8_t *to, size_t buf_sz);
};

class BgpPathAttribAggregator : public BgpPathAttrib {
public:
    // is_4b: 4b asn in aggregator?
    BgpPathAttribAggregator(bool is_4b);
    uint32_t aggregator;
    uint32_t aggregator_asn;

    bool is_4b;

    BgpPathAttrib* clone() const;
    ssize_t parse(const uint8_t *buffer, size_t length);
    ssize_t write(uint8_t *buffer, size_t buffer_sz) const;
    ssize_t print(size_t indent, uint8_t *to, size_t buf_sz);
};

class BgpPathAttribAs4Path : public BgpPathAttrib {
public:
    BgpPathAttribAs4Path();
    std::vector<BgpAsPathSegment> as4_paths;

    bool prepend(uint32_t asn);

    BgpPathAttrib* clone() const;
    ssize_t parse(const uint8_t *buffer, size_t length);
    ssize_t write(uint8_t *buffer, size_t buffer_sz) const;
    ssize_t print(size_t indent, uint8_t *to, size_t buf_sz);
private:
    void addSeg(uint32_t asn);
};

class BgpPathAttribAs4Aggregator : public BgpPathAttrib {
public:
    BgpPathAttribAs4Aggregator();
    uint32_t aggregator;
    uint32_t aggregator_asn4;

    BgpPathAttrib* clone() const;
    ssize_t parse(const uint8_t *buffer, size_t length);
    ssize_t write(uint8_t *buffer, size_t buffer_sz) const;
    ssize_t print(size_t indent, uint8_t *to, size_t buf_sz);
};

class BgpPathAttribCommunity : public BgpPathAttrib {
public:
    BgpPathAttribCommunity();
    uint32_t community;

    BgpPathAttrib* clone() const;
    ssize_t parse(const uint8_t *buffer, size_t length);
    ssize_t write(uint8_t *buffer, size_t buffer_sz) const;
    ssize_t print(size_t indent, uint8_t *to, size_t buf_sz);
};

}
#endif // BGP_PATH_ATTR_H