/**
 * @file prefix.h
 * @author Nato Morichika <nat@nat.moe>
 * @brief Route/Prefix related utilities.
 * @version 0.1
 * @date 2019-07-31
 * 
 * @copyright Copyright (c) 2019
 * 
 */
#ifndef BGP_PREFIX_H_
#define BGP_PREFIX_H_
#include <stdint.h>
#include <unistd.h>
#include "bgp-afi.h"

namespace libbgp {

uint32_t cidr_to_mask(uint8_t cidr);

/**
 * @brief Route/Prefix related utilities.
 * 
 */
class Prefix {
public:
    Afi afi;

    /**
     * @brief Parse a NLRI prefix from buffer.
     * 
     * @param buffer Buffer to parse from.
     * @param buf_sz Size of the buffer.
     * @return ssize_t Bytes read.
     * @retval -1 Failed to parse prefix.
     * @retval >=0 Bytes read.
     */
    virtual ssize_t parse(const uint8_t *buffer, size_t buf_sz) = 0;

    /**
     * @brief Write a prefix to NLRI buffer.
     * 
     * @param buffer Buffer to write to.
     * @param buf_sz Size of the buffer (max write size).
     * @return ssize_t Bytes written.
     * @retval -1 Failed to write.
     * @retval >=0 Bytes written.
     */
    virtual ssize_t write(uint8_t *buffer, size_t buf_sz) const = 0;

    /**
     * @brief Test if another prefix is inside this prefix.
     * 
     * @param other The other prefix.
     * @return true The other prefix is in this prefix.
     * @return false The other prefix is not in this prefix.
     */
    virtual bool includes (const Prefix &other) const = 0;

    // test if length & prefix equals to other
    virtual bool operator== (const Prefix &other) const = 0;

    // test if length smaller (prefix size bigger) then other. prefix must be
    // same to do this.
    virtual bool operator> (const Prefix &other) const = 0;

    // test if length bigger (prefix size smaller) then other. prefix must be
    // same to do this.
    virtual bool operator< (const Prefix &other) const = 0;

    virtual bool operator>= (const Prefix &other) const = 0;
    virtual bool operator<= (const Prefix &other) const = 0;
    virtual bool operator!= (const Prefix &other) const = 0;

    virtual ~Prefix() {}
};

}

#endif // BGP_PREFIX_H_