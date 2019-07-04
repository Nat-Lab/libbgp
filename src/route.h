/**
 * @file route.h
 * @author Nato Morichika <nat@nat.moe>
 * @brief Route/Prefix related utilities.
 * @version 0.1
 * @date 2019-07-04
 * 
 * @copyright Copyright (c) 2019
 * 
 */
#ifndef BGP_ROUTE_H_
#define BGP_ROUTE_H_
#include <stdint.h>

namespace libbgp {

uint32_t cidr_to_mask(uint8_t cidr);

/**
 * @brief Route/Prefix related utilities.
 * 
 */
class Route {
public:
    Route(uint32_t prefix, uint8_t length);
    Route(const char* prefix, uint8_t length);

    // static utility functions for route include test
    static bool Includes (uint32_t prefix, uint8_t length, uint32_t address);
    static bool Includes (uint32_t prefix_a, uint8_t length_a, uint32_t prefix_b, uint8_t length_b);

    // test if address in prefix
    bool includes (uint32_t address) const;
    bool includes (const char* address) const;

    // test if route other is sub-prefix
    bool includes (const Route &other) const;
    bool includes (uint32_t prefix, uint8_t length) const;
    bool includes (const char* prefix, uint8_t length) const;

    // test if length & prefix equals to other
    bool operator== (const Route &other) const;

    // test if length smaller (prefix size bigger) then other. prefix must be
    // same to do this.
    bool operator> (const Route &other) const;

    // test if length bigger (prefix size smaller) then other. prefix must be
    // same to do this.
    bool operator< (const Route &other) const;

    bool operator>= (const Route &other) const;
    bool operator<= (const Route &other) const;
    bool operator!= (const Route &other) const;

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

#endif // BGP_ROUTE_H