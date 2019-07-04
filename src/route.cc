#include "route.h"
#include <assert.h>
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

uint32_t cidr_to_mask(uint8_t cidr) {
    assert(cidr <= 32);
    return CIDR_MASK_MAP[cidr];
}

Route::Route(uint32_t prefix, uint8_t length) {
    assert(length <= 32);
    this->prefix = prefix;
    this->length = length;
}

bool Route::Includes (uint32_t prefix, uint8_t length, uint32_t address) {
    if (length > 32) return false;
    return (address & CIDR_MASK_MAP[length]) == prefix;
}

bool Route::Includes (uint32_t prefix_a, uint8_t length_a, uint32_t prefix_b, uint8_t length_b) {
    if (length_a > 32 || length_b > 32) return false;
    if (length_b < length_a) return false;
    return (prefix_b & CIDR_MASK_MAP[length_a]) == prefix_a;
}

Route::Route(const char* prefix, uint8_t length) {
    assert(length <= 32);
    this->length = length;
    inet_pton(AF_INET, prefix, &(this->prefix));
}

bool Route::includes (uint32_t address) const {
    return (address & CIDR_MASK_MAP[length]) == prefix;
}

bool Route::includes (const char* address) const {
    uint32_t address_int = 0;
    if (inet_pton(AF_INET, address, &address_int) <= 0) return false;
    return includes (address_int);
}

bool Route::includes (uint32_t prefix, uint8_t length) const {
    if (length > 32) return false;
    if (length < this->length) return false;
    return (prefix & CIDR_MASK_MAP[this->length]) == this->prefix;
}

bool Route::includes (const Route &other) const {
    return includes (other.prefix, other.length);
}

bool Route::includes (const char* prefix, uint8_t length) const {
    uint32_t prefix_int = 0;
    if (inet_pton(AF_INET, prefix, &prefix_int) <= 0) return false;
    return includes(prefix_int, length);
}

bool Route::operator== (const Route &other) const {
    return other.prefix == prefix && other.length == length;
}

bool Route::operator> (const Route &other) const {
    assert(prefix == other.prefix);
    return length < other.length;
}

bool Route::operator< (const Route &other) const {
    assert(prefix == other.prefix);
    return length < other.length;
}

bool Route::operator>= (const Route &other) const {
    return !(*this < other);
}

bool Route::operator<= (const Route &other) const {
    return !(*this > other);
}

bool Route::operator!= (const Route &other) const {
    return !(*this == other);
}

bool Route::set(uint32_t prefix, uint8_t length) {
    if (length > 24) return false;
    this->length = length;
    this->prefix = prefix;
    return true;
}

bool Route::setPrefix(uint32_t prefix) {
    this->prefix = prefix;
    return true;
}

bool Route::setLength(uint8_t length) {
    if (length > 24) return false;
    this->length = length;
    return true;
}

uint32_t Route::getPrefix() const {
    return prefix;
}

uint8_t Route::getLength() const {
    return length;
}

uint32_t Route::getMask() const {
    assert(length <= 32);
    return CIDR_MASK_MAP[length];
}

}