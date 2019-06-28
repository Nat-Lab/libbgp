#ifndef BGP_KEEPALIVE_MSG_H_
#define BGP_KEEPALIVE_MSG_H_

#include "bgp-message.h"
#include <stdint.h>

namespace bgpfsm {

class BgpKeepaliveMessage : public BgpMessage {
public:
    BgpKeepaliveMessage();

    ssize_t print(size_t indent, uint8_t *to, size_t buf_sz);

    ssize_t parse(const uint8_t *from, size_t msg_sz);
    ssize_t write(uint8_t *to, size_t buf_sz) const;
};

}

#endif // BGP_KEEPALIVE_MSG_H_