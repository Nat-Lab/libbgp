/**
 * @file serializable.h
 * @author Nato Morichika <nat@nat.moe>
 * @brief The serializable base.
 * @version 0.1
 * @date 2019-07-04
 * 
 * @copyright Copyright (c) 2019
 * 
 */
#ifndef SERIALIZABLE_H_
#define SERIALIZABLE_H_
#include <stdint.h>
#include <unistd.h>
#include <stdarg.h>
#include "bgp-log-handler.h"

namespace libbgp {

class BgpLogHandler;

/**
 * @brief The serializable base class.
 * 
 */
class Serializable {
public:
    Serializable(BgpLogHandler *logger);
    ~Serializable();

    // print the Serializable object as human readable string.
    ssize_t print(uint8_t *to, size_t buf_sz) const;

    // print the Serializable object as human readable string, pre-indented.
    ssize_t print(size_t indent, uint8_t *to, size_t buf_sz) const;

    /**
     * @brief Deserialize the object from buffer.
     * 
     * @param from The pointer to buffer.
     * @param msg_sz The max read length of deserializer.
     * @return ssize_t Bytes read.
     * @retval -1 Deserialization failed. error may be written to stderr with 
     * log handler.
     * @retval >=0 Bytes read.
     */
    virtual ssize_t parse(const uint8_t *from, size_t msg_sz) = 0;

    /**
     * @brief Serialize the object and write to buffer.
     * 
     * @param to The pointer to buffer.
     * @param buf_sz The max write size of serializer.
     * @return ssize_t Btyes written.
     * @retval -1 Serialization failed. error may be written to stderr with log
     * handler.
     * @retval >=0 Bytes written.
     */
    virtual ssize_t write(uint8_t *to, size_t buf_sz) const = 0;

    virtual ssize_t length() const;

    bool hasError() const;

    // get error code
    uint8_t getErrorCode() const;

    // get error subcode
    uint8_t getErrorSubCode() const;

    // get error payload (data field of NOTIFICATION message)
    const uint8_t* getError() const;

    // get length of error payload
    size_t getErrorLength() const;

    // replace logger
    void setLogger(BgpLogHandler *logger);

protected:
    // print helper. print string with format to pointer to buffer. will change
    // buffer pointer and buf_left after write. return bytes written
    static ssize_t _print(size_t indent, uint8_t **to, size_t *buf_left, const char* format, ...);

    /**
     * @brief Print implementation.
     * 
     * @param indent indent level.
     * @param to The pointer to the pointer to the string buffer.
     * @param buf_sz The pointer to the counter of avaliable buffer space.
     * @return ssize_t ssize_t Bytes written.
     * @retval -1 Failed to print.
     * @retval >= 0 Bytes written.
     */
    virtual ssize_t doPrint(size_t indent, uint8_t **to, size_t *buf_sz) const = 0;

    // utility function to error related values
    void setError(uint8_t err, uint8_t suberr, const uint8_t *data, size_t data_len);

    // utility function to forward attribute parse error to here
    void forwardParseError(const Serializable &other);

    uint8_t err_code;
    uint8_t err_subcode;
    size_t err_len;
    uint8_t *err_data;
    BgpLogHandler *logger;
};

}

#endif // SERIALIZABLE_H_