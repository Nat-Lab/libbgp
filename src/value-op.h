#ifndef VALUE_OP_H_
#define VALUE_OP_H_
#include <stdint.h>
#include <unistd.h>
#include <string.h>

namespace libbgp {

template <typename T> T getValue(const uint8_t **buffer);
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