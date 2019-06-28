#ifndef SERIALIZABLE_H_
#define SERIALIZABLE_H_
#include <stdint.h>
#include <unistd.h>

namespace bgpfsm {

class Serializable {
public:
    // print the Serializable object as human readable string.
    virtual ssize_t print(uint8_t *to, size_t buf_sz) const = 0;

    // deserialize the Serializable object from buffer.
    virtual ssize_t parse(const uint8_t *from, size_t msg_sz) = 0;

    // serialize the Serializable object to buffer.
    virtual ssize_t write(uint8_t *to, size_t buf_sz) const = 0;
    virtual ~Serializable() {}
};

}

#endif // SERIALIZABLE_H_