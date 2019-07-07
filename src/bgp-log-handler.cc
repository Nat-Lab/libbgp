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

const char* bgp_log_level_str[] = {
    "FATAL",
    "ERROR",
    "WARN ",
    "INFO ",
    "DEBUG"
};

BgpLogHandler::BgpLogHandler() {
    level = INFO;
}

/**
 * @brief Set the log level.
 * 
 * @param level Log level.
 */
void BgpLogHandler::setLogLevel(LogLevel level) {
    this->level = level;
}

/**
 * @brief Get the log level.
 * 
 * @return LogLevel log level.
 */
LogLevel BgpLogHandler::getLogLevel() const {
    return level;
}

/**
 * @brief Log a message. Consider using LIBBGP_LOG if logging the message needs
 * a lot of computing power. (e.g., print Serializable or converting IP to 
 * string.)
 * 
 * @param level Log level.
 * @param format_str printf format string.
 * @param ... printf variables.
 */
void BgpLogHandler::log(LogLevel level, const char* format_str, ...) {
    if (level > this->level) return;

    buf_mtx.lock();
    int pre_sz = snprintf(out_buffer, 4096, "[%s] ", bgp_log_level_str[level]);

    va_list args;
    va_start(args, format_str);
    vsnprintf(out_buffer + pre_sz, 4096 - pre_sz, format_str, args);
    va_end(args);
    logImpl(out_buffer);
    buf_mtx.unlock();
}

/**
 * @brief Log a message. Consider using LIBBGP_LOG if logging the message needs
 * a lot of computing power. (e.g., print Serializable or converting IP to 
 * string.)
 * 
 * @param level Log level.
 * @param serializable Serializable object to log.
 */
void BgpLogHandler::log(LogLevel level, const Serializable &serializable) {
    buf_mtx.lock();
    int pre_sz = snprintf(out_buffer, 4096, "[%s] ", bgp_log_level_str[level]);
    serializable.print((uint8_t *) (out_buffer + pre_sz), 4096 - pre_sz);
    logImpl(out_buffer);
    buf_mtx.unlock();
}

void BgpLogHandler::logImpl(const char* str) {
    fprintf(::stderr, "%s", str);
}

}