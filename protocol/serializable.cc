#include "serializable.h"
#include <stdio.h>

namespace bgpfsm {

ssize_t Serializable::print(uint8_t *to, size_t buf_sz) const {
    return doPrint(0, &to, &buf_sz);
}

ssize_t Serializable::_print(size_t indent, uint8_t **to, size_t *buf_left, const char* format, ...) {
    if (*buf_left <= indent * 4) return 0;
    for (size_t i = 0; i < indent; i++) {
        sprintf((char *) *to, "    ");
        *to += 4;
        *buf_left -= 4;
    }
    va_list args;
    va_start(args, format);
    ssize_t sz = vsnprintf((char *) *to, *buf_left, format, args);
    va_end(args);

    if (sz < 0) return sz;
    if ((size_t) sz > *buf_left) {
        size_t written = *buf_left;
        *buf_left = 0;
        return written;
    }

    *buf_left -= sz;
    *to += sz;

    return sz;
}

}