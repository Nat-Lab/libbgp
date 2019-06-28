#include "serializable.h"
#include <stdio.h>

namespace bgpfsm {

ssize_t Serializable::print(uint8_t *to, size_t buf_sz) const {
    return print(0, to, buf_sz);
}

ssize_t Serializable::_print(size_t indent, uint8_t **to, size_t *buf_left, const char* format, ...) {
    if (buf_left <= 0) return 0;
    va_list args;
    va_start(args, format);
    ssize_t sz = vsnprintf((char *) *to, *buf_left, format, args);
    va_end(args);

    if (sz > *buf_left) *buf_left = 0;
    else {
        *buf_left -= sz;
        *to += sz;
    }

    return sz;
}

}