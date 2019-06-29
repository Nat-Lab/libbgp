#include "serializable.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

namespace bgpfsm {

Serializable::Serializable() {
    err_code = 0;
    err_subcode = 0;
    err_data = NULL;
    err_len = 0;
}

Serializable::~Serializable() {
    if (err_len > 0 && err_data != NULL) free(err_data);
}

bool Serializable::hasError() const {
    return err_len != 0;
}

void Serializable::setError(uint8_t err, uint8_t suberr, const uint8_t *data, size_t data_len) {
    err_code = err;
    err_subcode = suberr;

    // err_buf_len not 0, setError() when error already there?
    assert(err_len == 0);

    if (data_len == 0) return;
    err_len = data_len;
    err_data = (uint8_t *) malloc(err_len);
    memcpy(err_data, data, data_len);
}

uint8_t Serializable::getErrorCode() const {
    return err_code;
}

uint8_t Serializable::getErrorSubCode() const {
    return err_subcode;
}

const uint8_t* Serializable::getError() const {
    return err_data;
}

size_t Serializable::getErrorLength() const {
    return err_len;
}

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