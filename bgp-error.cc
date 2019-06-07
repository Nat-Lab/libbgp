#include "bgp-error.h"
#include <string.h>

namespace bgpfsm {

void _bgp_error (const char *format_str, ...) {
    if (!__bgp_err_buf_init) {
        __bgp_err_buf_init = true;
        memset(__bgp_error, 0, 255);
    }

    size_t buf_left = 255 - __bgp_err_offset;

    va_list args;
    va_start(args, format_str);
    int sz = vsnprintf(__bgp_error + __bgp_err_offset, buf_left, format_str, args);
    va_end(args);

    if (sz < 0) throw "_bgp_error: I/O error when writing error buffer.";
    if (sz > buf_left) throw "_bgp_error: error buffer full.";
    __bgp_err_offset += (uint8_t) sz;
}

const char *get_bgp_errors() {
    return __bgp_err_buf_init ? __bgp_error : "";
}

void clear_bgp_errors() {
    memset(__bgp_error, 0, 255);
    __bgp_err_offset = 0;
}

}