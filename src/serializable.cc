/**
 * @file serializable.cc
 * @author Nato Morichika <nat@nat.moe>
 * @brief The serializable base.
 * @version 0.1
 * @date 2019-07-04
 * 
 * @copyright Copyright (c) 2019
 * 
 */
#include "serializable.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace libbgp {

/**
 * @brief Construct a new Serializable:: Serializable object
 * 
 * @param logger Logger for serializer/deserializer errors.
 */
Serializable::Serializable(BgpLogHandler *logger) {
    err_code = 0;
    err_subcode = 0;
    err_data = NULL;
    err_len = 0;
    this->logger = logger;
}

/**
 * @brief Destroy the Serializable:: Serializable object
 * 
 */
Serializable::~Serializable() {
    if (err_len > 0 && err_data != NULL) free(err_data);
}

/**
 * @brief Check if error information available.
 * 
 * @return true information avaliable.
 * @return false information not avaliable.
 */
bool Serializable::hasError() const {
    return err_len != 0;
}

/**
 * @brief Set the error information.
 * 
 * Set the error information. The information is used by BgpFsm to send 
 * `NOTIFICATION` message to the peer.
 * 
 * @param err The error code.
 * @param suberr The error subcode.
 * @param data The error data buffer.
 * @param data_len The length of error data buffer.
 * @throws "err_exist" Error already set.
 */
void Serializable::setError(uint8_t err, uint8_t suberr, const uint8_t *data, size_t data_len) {
    err_code = err;
    err_subcode = suberr;

    if (err_len != 0) {
        logger->log(FATAL, "Serializable::setError: error already exists.\n");
        throw "err_exist";
    }

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

/**
 * @brief Forward error information from other Serializable object.
 * 
 * @param other The other Serializable object.
 */
void Serializable::forwardParseError(const Serializable &other) {
    setError(other.getErrorCode(), other.getErrorSubCode(), other.getError(), other.getErrorLength());
}

/**
 * @brief Print the Serializable object as human readable string.
 * 
 * @param to The pointer to the string buffer.
 * @param buf_sz The length of string buffer.
 * @return ssize_t Bytes written.
 * @retval -1 Failed to print.
 * @retval >= 0 Bytes written.
 */
ssize_t Serializable::print(uint8_t *to, size_t buf_sz) const {
    return doPrint(0, &to, &buf_sz);
}

/**
 * @brief Print the Serializable object as human readable string, with 
 * indentation.
 * 
 * @param indent indent level.
 * @param to The pointer to the string buffer.
 * @param buf_sz The length of string buffer.
 * @return ssize_t Bytes written.
 * @retval -1 Failed to print.
 * @retval >= 0 Bytes written.
 */
ssize_t Serializable::print(size_t indent, uint8_t *to, size_t buf_sz) const {
    return doPrint(indent, &to, &buf_sz);
}

/**
 * @brief Print helper.
 * 
 * Print helper prints string to buffer with printf format, increment the buffer
 * pointer and decrease the available buffer pointer.
 * 
 * @param indent indent level.
 * @param to The pointer to the pointer to the string buffer.
 * @param buf_left The pointer to the counter of avaliable buffer space.
 * @param format The printf format string.
 * @param ... 
 * @return ssize_t Bytes written.
 * @retval -1 Failed to print.
 * @retval >=0 Bytes written.
 */
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
        return written + indent * 4;
    }

    *buf_left -= sz;
    *to += sz;

    return sz + indent * 4;
}

/**
 * @brief Get size in bytes required to serialize the object.
 * 
 * @return ssize_t Size in btyes.
 * @retval -1 Failed to get size.
 * @retval >=0 Size in btyes.
 */
ssize_t Serializable::length() const {
    logger->log(WARN, "Serializable::length: using default implementation of Serializable::length() (write to dummy).\n");
    uint8_t buffer[4096];
    return write(buffer, 4096);
}

/**
 * @brief Replace logger for this object.
 * 
 * @param logger The new logger.
 */
void Serializable::setLogger(BgpLogHandler *logger) {
    this->logger = logger;
}

}