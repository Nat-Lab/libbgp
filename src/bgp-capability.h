/**
 * @file bgp-capability.h
 * @author Nato Morichika <nat@nat.moe>
 * @brief BGP Capabilities.
 * @version 0.1
 * @date 2019-07-04
 * 
 * @copyright Copyright (c) 2019
 * 
 */
#ifndef BGP_CAP_H_
#define BGP_CAP_H_
#include "bgp-afi.h"
#include "serializable.h"
#include <stdint.h>

namespace libbgp {

/**
 * @brief BGP capability codes
 * 
 */
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

/**
 * @brief The BgpCapability base class.
 * 
 */
class BgpCapability : public Serializable {
public:
    BgpCapability(BgpLogHandler *logger);

    uint8_t code;
    virtual ~BgpCapability() {}

    /**
     * @brief Deserialize a BGP open message capability.
     * 
     * 
     * @param from Pointer to message body buffer.
     * @param msg_sz Size of message.
     * @return ssize_t Bytes read.
     * @retval -1 Deserialization error. Error may be logged.
     * @retval >=0 Bytes read.
     * @throws "bad_parse" Internal deserialization error.
     * @throws "bad_type" The type of message/field member in buffer does not 
     * match the attribute type of container.
     */
    virtual ssize_t parse(const uint8_t *from, size_t msg_sz) = 0;

    /**
     * @brief Serialize a BGP open message capability.
     * 
     * @param to Pointer to destination buffer.
     * @param buf_sz Max write size.
     * @return ssize_t Bytes written.
     * @retval -1 Serialization error. Error may be logged.
     * @retval >=0 Bytes written.
     */
    virtual ssize_t write(uint8_t *to, size_t buf_sz) const = 0;
protected:

    ssize_t parseHeader(const uint8_t *from, size_t msg_sz);

    /**
     * @brief Length of the attribute. Used only when deserializer.
     * 
     * Length field is only used in deserialization for parseHeader() to pass
     * attribute length to the underlying deserializers. The length field is
     * ignored when serialize (except BgpCapabilityUnknow). 
     * 
     */
    uint8_t length;
};

/**
 * @brief The BgpCapability4BytesAsn class.
 * 
 */
class BgpCapability4BytesAsn : public BgpCapability {
public:
    BgpCapability4BytesAsn(BgpLogHandler *logger);

    ssize_t doPrint(size_t indent, uint8_t **to, size_t *buf_sz) const;
    ssize_t parse(const uint8_t *from, size_t msg_sz);
    ssize_t write(uint8_t *to, size_t buf_sz) const;

    /**
     * @brief The ASN.
     * 
     */
    uint32_t my_asn;
};

/**
 * @brief The BgpCapabilityMpBgp class.
 * 
 */
class BgpCapabilityMpBgp : public BgpCapability {
public:
    BgpCapabilityMpBgp(BgpLogHandler *logger);

    ssize_t doPrint(size_t indent, uint8_t **to, size_t *buf_sz) const;
    ssize_t parse(const uint8_t *from, size_t msg_sz);
    ssize_t write(uint8_t *to, size_t buf_sz) const;

    /**
     * @brief Address Family Identifier.
     * 
     */
    uint16_t afi;

    /**
     * @brief Subsequent Address Family Identifier
     * 
     */
    uint8_t safi;
};

/**
 * @brief The BgpCapabilityUnknow class.
 * 
 */
class BgpCapabilityUnknow : public BgpCapability {
public:
    BgpCapabilityUnknow(BgpLogHandler *logger);
    ~BgpCapabilityUnknow();

    ssize_t doPrint(size_t indent, uint8_t **to, size_t *buf_sz) const;
    ssize_t parse(const uint8_t *from, size_t msg_sz);
    ssize_t write(uint8_t *to, size_t buf_sz) const;

private:
    uint8_t *value;
};

}

#endif // BGP_CAP_H_