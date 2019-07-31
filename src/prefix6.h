/**
 * @file prefix6.h
 * @author Nato Morichika <nat@nat.moe>
 * @brief IPv6 Route/Prefix related utilities.
 * @version 0.1
 * @date 2019-07-21
 * 
 * @copyright Copyright (c) 2019
 * 
 */
#ifndef BGP_PREFIX6_H_
#define BGP_PREFIX6_H_
#include <stdint.h>
#include <unistd.h>
#include "prefix.h"

namespace libbgp {

bool cidr_to_mask6(uint8_t src, uint8_t dst[16]);
bool mask_ipv6(const uint8_t prefix[16], uint8_t mask, uint8_t masked_addr[16]);
bool v6addr_is_zero(const uint8_t prefix[16]);

/**
 * @brief IPv6 Route/Prefix related utilities.
 * 
 */
class Prefix6 : public Prefix {
public:
    Prefix6();
    Prefix6(const uint8_t prefix[16], uint8_t length);
    Prefix6(const char* prefix, uint8_t length);

    ssize_t parse(const uint8_t *buffer, size_t buf_sz);
    ssize_t write(uint8_t *buffer, size_t buf_sz) const;

    // static utility functions for route include test
    static bool Includes (const uint8_t prefix[16], uint8_t length, const uint8_t address[16]);
    static bool Includes (const uint8_t prefix_a[16], uint8_t length_a, const uint8_t prefix_b[16], uint8_t length_b);

    // test if address in prefix
    bool includes (const uint8_t address[16]) const;
    bool includes (const char* address) const;

    // test if route other is sub-prefix
    bool includes (const Prefix &other) const;
    bool includes (const uint8_t prefix[16], uint8_t length) const;
    bool includes (const char* prefix, uint8_t length) const;

    // test if length & prefix equals to other
    bool operator== (const Prefix &other) const;

    // test if length smaller (prefix size bigger) then other. prefix must be
    // same to do this.
    bool operator> (const Prefix &other) const;

    // test if length bigger (prefix size smaller) then other. prefix must be
    // same to do this.
    bool operator< (const Prefix &other) const;

    bool operator>= (const Prefix &other) const;
    bool operator<= (const Prefix &other) const;
    bool operator!= (const Prefix &other) const;

    bool set(const uint8_t prefix[16], uint8_t length);
    bool setPrefix(const uint8_t prefix[16]);
    bool setLength(uint8_t length);
    void getPrefix(uint8_t prefix[16]) const;
    uint8_t getLength() const;
    void getMask(uint8_t mask[16]) const;

private:
    uint8_t length;
    uint8_t prefix[16];
};

}

#endif // BGP_PREFIX6_H_