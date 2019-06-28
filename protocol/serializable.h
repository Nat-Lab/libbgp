#ifndef SERIALIZABLE_H_
#define SERIALIZABLE_H_
#include <stdint.h>
#include <unistd.h>
#include <stdarg.h>

namespace bgpfsm {

class Serializable {
public:
    // print the Serializable object as human readable string.
    ssize_t print(uint8_t *to, size_t buf_sz) const;

    // print the object, indent it for indent times.
    virtual ssize_t print(size_t indent, uint8_t *to, size_t buf_sz) const = 0;

    // deserialize the Serializable object from buffer.
    virtual ssize_t parse(const uint8_t *from, size_t msg_sz) = 0;

    // serialize the Serializable object to buffer.
    virtual ssize_t write(uint8_t *to, size_t buf_sz) const = 0;
    virtual ~Serializable() {}

protected:
    // print helper. print string with format to pointer to buffer. will change
    // buffer pointer and buf_left after write. return bytes written
    static ssize_t _print(size_t indent, uint8_t **to, size_t *buf_left, const char* format, ...);
};

}

#endif // SERIALIZABLE_H_