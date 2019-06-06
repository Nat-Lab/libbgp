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

    virtual ssize_t parse(const uint8_t *from, size_t msg_sz) = 0;
    virtual ssize_t write(uint8_t *to, size_t msg_sz) const = 0;

    virtual ~BgpMessage() {}
};

}
#endif // BGP_MSG_H_