#ifndef BGP_MSG_H_
#define BGP_MSG_H_
#include <stdint.h>

namespace bgpfsm {

enum BgpMessageType {
    OPEN = 1,
    UPDATE = 2,
    KEEPALIVE = 3,
    NOTIFICATION = 4
};

class BgpMessage {
public:
    BgpMessageType type;

    // prase BGP message from buffer
    virtual ssize_t parse(const uint8_t *from, size_t msg_sz) = 0;

    // write BGP message to buffer
    virtual ssize_t write(uint8_t *to, size_t buf_sz) const = 0;

    // get error code
    virtual uint8_t getErrorCode() const = 0;

    // get error subcode
    virtual uint8_t getErrorSubCode() const = 0;

    // get error payload (data field of NOTIFICATION message)
    virtual const uint8_t* getError() const = 0;

    // get length of error payload
    virtual size_t getErrorLength() const = 0;

    virtual ~BgpMessage() {}
};

}
#endif // BGP_MSG_H_