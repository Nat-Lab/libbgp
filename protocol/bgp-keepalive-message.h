#ifndef BGP_KEEPALIVE_MSG_H_
#define BGP_KEEPALIVE_MSG_H_

#include "bgp-message.h"
#include <stdint.h>

namespace bgpfsm {

class BgpKeepaliveMessage : public BgpMessage {
public:
    BgpKeepaliveMessage();

    ssize_t parse(const uint8_t *from, size_t msg_sz);
    ssize_t write(uint8_t *to, size_t buf_sz) const;

    const uint8_t* getError() const;
    ssize_t getErrorLength() const;
};

}

#endif // BGP_KEEPALIVE_MSG_H_