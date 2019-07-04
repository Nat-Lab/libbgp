#ifndef SERIALIZABLE_H_
#define SERIALIZABLE_H_
#include <stdint.h>
#include <unistd.h>
#include <stdarg.h>

namespace libbgp {

class Serializable {
public:
    Serializable();
    ~Serializable();

    // print the Serializable object as human readable string.
    ssize_t print(uint8_t *to, size_t buf_sz) const;

    // print the Serializable object as human readable string, pre-indented.
    ssize_t print(size_t indent, uint8_t *to, size_t buf_sz) const;

    // deserialize the Serializable object from buffer.
    virtual ssize_t parse(const uint8_t *from, size_t msg_sz) = 0;

    // serialize the Serializable object to buffer.
    virtual ssize_t write(uint8_t *to, size_t buf_sz) const = 0;

    bool hasError() const;

    // get error code
    uint8_t getErrorCode() const;

    // get error subcode
    uint8_t getErrorSubCode() const;

    // get error payload (data field of NOTIFICATION message)
    const uint8_t* getError() const;

    // get length of error payload
    size_t getErrorLength() const;

protected:
    // print helper. print string with format to pointer to buffer. will change
    // buffer pointer and buf_left after write. return bytes written
    static ssize_t _print(size_t indent, uint8_t **to, size_t *buf_left, const char* format, ...);

    // print the object, indent it for indent times.
    virtual ssize_t doPrint(size_t indent, uint8_t **to, size_t *buf_sz) const = 0;

    // utility function to error related values
    void setError(uint8_t err, uint8_t suberr, const uint8_t *data, size_t data_len);

    // utility function to forward attribute parse error to here
    void forwardParseError(const Serializable &other);

    uint8_t err_code;
    uint8_t err_subcode;
    size_t err_len;
    uint8_t *err_data;
};

}

#endif // SERIALIZABLE_H_