/**
 * @file prefix4.cc
 * @author Nato Morichika <nat@nat.moe>
 * @brief IPv4 Route/Prefix related utilities.
 * @version 0.1
 * @date 2019-07-21
 * 
 * @copyright Copyright (c) 2019
 * 
 */
#include "prefix4.h"
#include "value-op.h"
#include <arpa/inet.h>

namespace libbgp {

const uint32_t CIDR_MASK_MAP[33] = { 
    0x00000000, 0x00000080, 0x000000c0, 0x000000e0, 0x000000f0, 0x000000f8, 
    0x000000fc, 0x000000fe, 0x000000ff, 0x000080ff, 0x0000c0ff, 0x0000e0ff, 
    0x0000f0ff, 0x0000f8ff, 0x0000fcff, 0x0000feff, 0x0000ffff, 0x0080ffff, 
    0x00c0ffff, 0x00e0ffff, 0x00f0ffff, 0x00f8ffff, 0x00fcffff, 0x00feffff, 
    0x00ffffff, 0x80ffffff, 0xc0ffffff, 0xe0ffffff, 0xf0ffffff, 0xf8ffffff, 
    0xfcffffff, 0xfeffffff, 0xffffffff
};

/**
 * @brief Convert netmask in CIDR notation to network bytes integer.
 * 
 * @param cidr The netmask in CIDR notation.
 * @return uint32_t The netmask in network byte order.
 * @throws "bad_route_length" Netmask invalid.
 */
uint32_t cidr_to_mask(uint8_t cidr) {
    if (cidr > 32) throw "bad_route_length";
    return CIDR_MASK_MAP[cidr];
}

/**
 * @brief Construct a new Prefix4 object
 * 
 */
Prefix4::Prefix4() {
    afi = IPV4;
    prefix = length = 0;
}

/**
 * @brief Construct a new Prefix4 object
 * 
 * @param prefix Prefix in network bytes order.
 * @param length Netmask in CIDR notation.
 * @throws "bad_route_length" Netmask invalid.
 */
Prefix4::Prefix4(uint32_t prefix, uint8_t length) {
    if (length > 32) throw "bad_route_length";
    afi = IPV4;
    this->prefix = prefix;
    this->length = length;
}

/**
 * @brief Construct a new Prefix4:: Prefix4 object
 * 
 * @param prefix Prefix in dotted string notation.
 * @param length Netmask in CIDR notation.
 * @throws "bad_route_length" Netmask invalid.
 */
Prefix4::Prefix4(const char* prefix, uint8_t length) {
    if (length > 32) throw "bad_route_length";
    afi = IPV4;
    this->length = length;
    inet_pton(AF_INET, prefix, &(this->prefix));
}

/**
 * @brief Parse a IPv4 NLRI prefix from buffer.
 * 
 * @param buffer Buffer to parse from.
 * @param buf_sz Size of the buffer.
 * @return ssize_t Bytes read.
 * @retval -1 Failed to parse prefix.
 * @retval >=0 Bytes read.
 */
ssize_t Prefix4::parse(const uint8_t *buffer, size_t buf_sz) {
    if (buf_sz < 1) return -1;
    length = getValue<uint8_t>(&buffer);
    size_t prefix_buf_len = (length + 7) / 8;
    if (prefix_buf_len + 1 > buf_sz) return -1;
    prefix = 0;
    memcpy(&prefix, buffer, prefix_buf_len);
    return prefix_buf_len + 1;
}

/**
 * @brief Write a IPv4 prefix to NLRI buffer.
 * 
 * @param buffer Buffer to write to.
 * @param buf_sz Size of the buffer (max write size).
 * @return ssize_t Bytes written.
 * @retval -1 Failed to write.
 * @retval >=0 Bytes written.
 */
ssize_t Prefix4::write(uint8_t *buffer, size_t buf_sz) const {
    size_t prefix_buf_len = (length + 7) / 8;
    if (buf_sz < 1 + prefix_buf_len) return -1;
    putValue<uint8_t>(&buffer, length);
    memcpy(buffer, &prefix, prefix_buf_len);
    return prefix_buf_len + 1;
}

/**
 * @brief Test if an address is inside a prefix.
 * 
 * @param prefix The prefix in network bytes order.
 * @param length The netmask of prefix in CIDR notation.
 * @param address The address in network bytes order.
 * @return true The address is in the prefix.
 * @return false The address in not in the prefix.
 */
bool Prefix4::Includes (uint32_t prefix, uint8_t length, uint32_t address) {
    if (length > 32) return false;
    return (address & CIDR_MASK_MAP[length]) == prefix;
}

/**
 * @brief Test if a prefix is inside another prefix.
 * 
 * @param prefix_a The prefix in network bytes order.
 * @param length_a The netmask of prefix in CIDR notation.
 * @param prefix_b The prefix to test against.
 * @param length_b The netmask of prefix to test against in CIDR notation.
 * @return true prefix_b is in prefix_a.
 * @return false prefix_b is not in prefix_a.
 */
bool Prefix4::Includes (uint32_t prefix_a, uint8_t length_a, uint32_t prefix_b, uint8_t length_b) {
    if (length_a > 32 || length_b > 32) return false;
    if (length_b < length_a) return false;
    return (prefix_b & CIDR_MASK_MAP[length_a]) == prefix_a;
}

/**
 * @brief Test if an address is inside this prefix.
 * 
 * @param address The address in network bytes order.
 * @return true The address is in the prefix.
 * @return false The address in not in the prefix.
 */
bool Prefix4::includes (uint32_t address) const {
    return (address & CIDR_MASK_MAP[length]) == prefix;
}

/**
 * @brief Test if an address is inside this prefix.
 * 
 * @param address The address in dotted string notation.
 * @return true The address is in the prefix.
 * @return false The address in not in the prefix.
 */
bool Prefix4::includes (const char* address) const {
    uint32_t address_int = 0;
    if (inet_pton(AF_INET, address, &address_int) <= 0) return false;
    return includes (address_int);
}

/**
 * @brief Test if another prefix is inside this prefix.
 * 
 * @param prefix The prefix in network bytes order.
 * @param length The netmask of prefix in CIDR notation.
 * @return true The other prefix is in this prefix.
 * @return false The other prefix is not in this prefix.
 */
bool Prefix4::includes (uint32_t prefix, uint8_t length) const {
    if (length > 32) return false;
    if (length < this->length) return false;
    return (prefix & CIDR_MASK_MAP[this->length]) == this->prefix;
}

/**
 * @brief Test if another prefix is inside this prefix.
 * 
 * @param other The other prefix.
 * @return true The other prefix is in this prefix.
 * @return false The other prefix is not in this prefix.
 */
bool Prefix4::includes (const Prefix &other) const {
    if (other.afi != IPV4) return false;
    const Prefix4 &other_4 = dynamic_cast<const Prefix4 &>(other);
    return includes (other_4.prefix, other_4.length);
}

/**
 * @brief Test if another prefix is inside this prefix.
 * 
 * @param prefix The prefix in dotted string notation.
 * @param length The netmask of prefix in CIDR notation.
 * @return true The other prefix is in this prefix.
 * @return false The other prefix is not in this prefix.
 */
bool Prefix4::includes (const char* prefix, uint8_t length) const {
    uint32_t prefix_int = 0;
    if (inet_pton(AF_INET, prefix, &prefix_int) <= 0) return false;
    return includes(prefix_int, length);
}

/**
 * @brief Test if two routes are equals.
 * 
 * @param other The other route object.
 * @return true The routes are equal.
 * @return false The routes are different.
 */
bool Prefix4::operator== (const Prefix &other) const {
    if (other.afi != IPV4) return false;
    const Prefix4 &other_4 = dynamic_cast<const Prefix4 &>(other);
    return other_4.prefix == prefix && other_4.length == length;
}

bool Prefix4::operator> (const Prefix &other) const {
    if (other.afi != IPV4) return false;
    const Prefix4 &other_4 = dynamic_cast<const Prefix4 &>(other);
    return length < other_4.length;
}

bool Prefix4::operator< (const Prefix &other) const {
    if (other.afi != IPV4) return false;
    const Prefix4 &other_4 = dynamic_cast<const Prefix4 &>(other);
    return length > other_4.length;
}

bool Prefix4::operator>= (const Prefix &other) const {
    return !(*this < other);
}

bool Prefix4::operator<= (const Prefix &other) const {
    return !(*this > other);
}

bool Prefix4::operator!= (const Prefix &other) const {
    return !(*this == other);
}

/**
 * @brief Set Prefix4.
 * 
 * @param prefix The prefix in netowkr byte order.
 * @param length The netmask in CIDR notation.
 * @return true Prefix4 set.
 * @return false Failed to set route.
 */
bool Prefix4::set(uint32_t prefix, uint8_t length) {
    if (length > 32) return false;
    this->length = length;
    this->prefix = prefix;
    return true;
}

/**
 * @brief Set prefix.
 * 
 * @param prefix The prefix to set in network byte order.
 * @return true Prefix set.
 * @return false Failed to set prefix.
 */
bool Prefix4::setPrefix(uint32_t prefix) {
    this->prefix = prefix;
    return true;
}

/**
 * @brief Set netmask.
 * 
 * @param length The netmask in CIDR notation.
 * @return true Netmask set.
 * @return false Failed to set netmask.
 */
bool Prefix4::setLength(uint8_t length) {
    if (length > 32) return false;
    this->length = length;
    return true;
}

/**
 * @brief Get prefix.
 * 
 * @return uint32_t The prefix in network byte order.
 */
uint32_t Prefix4::getPrefix() const {
    return prefix;
}

/**
 * @brief Get netmask.
 * 
 * @return uint8_t The netmask in CIDR notation.
 */
uint8_t Prefix4::getLength() const {
    return length;
}

/**
 * @brief Get netmask.
 * 
 * @return uint32_t The netmask in network byte order.
 * @throws "bad_route_length" Netmask invalid.
 */
uint32_t Prefix4::getMask() const {
    if (length > 32) throw "bad_route_length";
    return CIDR_MASK_MAP[length];
}

}