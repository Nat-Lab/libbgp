/**
 * @file bgp-log-handler.cc
 * @author Nato Morichika <nat@nat.moe>
 * @brief BGP log handler
 * @version 0.1
 * @date 2019-07-05
 * 
 * @copyright Copyright (c) 2019
 * 
 */
#include "bgp-log-handler.h"
#include <stdarg.h>
#include <stdio.h>

namespace libbgp {

/**
 * @brief Print message to stdout.
 * 
 * @param format_str printf format string.
 * @param ... variables.
 */
void BgpLogHandler::stdout(const char* format_str, ...) {
    va_list args;

    buf_mtx.lock();
    va_start(args, format_str);
    vsnprintf(out_buffer, 4096, format_str, args);
    va_end(args);
    stdoutImpl(out_buffer);
    buf_mtx.unlock();
}

/**
 * @brief Print message to stderr.
 * 
 * @param format_str printf format string.
 * @param ... variables.
 */
void BgpLogHandler::stderr(const char* format_str, ...) {
    va_list args;

    buf_mtx.lock();
    va_start(args, format_str);
    vsnprintf(out_buffer, 4096, format_str, args);
    va_end(args);
    stderrImpl(out_buffer);
    buf_mtx.unlock();
}

void BgpLogHandler::stdoutImpl(const char* str) {
    fprintf(::stdout, "%s", str);
}

void BgpLogHandler::stderrImpl(const char* str) {
    fprintf(::stderr, "%s", str);
}

}