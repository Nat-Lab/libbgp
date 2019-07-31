/**
 * @file prefix4.h
 * @author Nato Morichika <nat@nat.moe>
 * @brief IPv4 Route/Prefix related utilities.
 * @version 0.2
 * @date 2019-07-21
 * 
 * @copyright Copyright (c) 2019
 * 
 */
#ifndef BGP_PREFIX4_H_
#define BGP_PREFIX4_H_
#include <stdint.h>
#include <unistd.h>
#include "prefix.h"

namespace libbgp {

uint32_t cidr_to_mask(uint8_t cidr);

/**
 * @brief IPv4 Route/Prefix related utilities.
 * 
 */
class Prefix4 : public Prefix {
public:
    Prefix4();
    Prefix4(uint32_t prefix, uint8_t length);
    Prefix4(const char* prefix, uint8_t length);

    ssize_t parse(const uint8_t *buffer, size_t buf_sz);
    ssize_t write(uint8_t *buffer, size_t buf_sz) const;

    // static utility functions for route include test
    static bool Includes (uint32_t prefix, uint8_t length, uint32_t address);
    static bool Includes (uint32_t prefix_a, uint8_t length_a, uint32_t prefix_b, uint8_t length_b);

    // test if address in prefix
    bool includes (uint32_t address) const;
    bool includes (const char* address) const;

    // test if route other is sub-prefix
    bool includes (const Prefix &other) const;
    bool includes (uint32_t prefix, uint8_t length) const;
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

    bool set(uint32_t prefix, uint8_t length);
    bool setPrefix(uint32_t prefix);
    bool setLength(uint8_t length);
    uint32_t getPrefix() const;
    uint8_t getLength() const;
    uint32_t getMask() const;

private:
    uint8_t length;
    uint32_t prefix;
};

}

#endif // BGP_PREFIX4_H_