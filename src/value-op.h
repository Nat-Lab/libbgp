/**
 * @file value-op.h
 * @author Nato Morichika <nat@nat.moe>
 * @brief Buffer operation helpers.
 * @version 0.1
 * @date 2019-07-04
 * 
 * @copyright Copyright (c) 2019
 * 
 */
#ifndef VALUE_OP_H_
#define VALUE_OP_H_
#include <stdint.h>
#include <unistd.h>
#include <string.h>

namespace libbgp {

/**
 * @brief Get value from buffer.
 * 
 * Read value from buffer pointer and move buffer pointer.
 * 
 * @tparam T Type of value.
 * @param buffer Pointer to buffer.
 * @return T The value.
 */
template <typename T> T getValue(const uint8_t **buffer);

/**
 * @brief Put value to buffer.
 * 
 * Write the value to buffer pointer and move the buffer pointer.
 * 
 * @tparam T Type of value.
 * @param buffer Pointer to pointer to buffer.
 * @param value Value to write.
 * @return size_t Bytes written.
 */
template <typename T> size_t putValue(uint8_t **buffer, T value);

template <typename T> T getValue(const uint8_t **buffer) {
    auto *buf = *buffer;
    size_t sz = sizeof(T);
    T var;
    memcpy(&var, buf, sz);
    *buffer = buf + sz;
    return var;
}

template <typename T> size_t putValue(uint8_t **buffer, T value) {
    uint8_t *buf = *buffer;
    size_t sz = sizeof(T);
    memcpy(buf, &value, sz);
    *buffer = buf + sz;
    return sz;
}

}
#endif // VALUE_OP_H_