/**
 * @file bgp-error.cc
 * @author Nato Morichika <nat@nat.moe>
 * @brief Legacy libbgp error handler.
 * @version 0.1
 * @date 2019-07-07
 * 
 * @copyright Copyright (c) 2019
 * 
 */
#include "bgp-error.h"
#include <string.h>

namespace libbgp {

char __bgp_error[255];
uint8_t __bgp_err_offset = 0;
bool __bgp_err_buf_init = false;

/**
 * @brief Log an error.
 * 
 * @deprecated use BgpLogHandler instead.
 * @param format_str printf format string.
 * @param ... print variables.
 */
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
    if ((size_t) sz > buf_left) throw "_bgp_error: error buffer full.";
    __bgp_err_offset += (uint8_t) sz;
}

/**
 * @brief Get error messages.
 * 
 * @return const char* Error message.
 */
const char *get_bgp_errors() {
    return __bgp_err_buf_init ? __bgp_error : "";
}

/**
 * @brief Clear all log messages.
 * 
 */
void clear_bgp_errors() {
    memset(__bgp_error, 0, 255);
    __bgp_err_offset = 0;
}

}