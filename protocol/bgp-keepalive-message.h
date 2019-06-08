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

    uint8_t getErrorCode() const;
    uint8_t getErrorSubCode() const;
    const uint8_t* getError() const;
    size_t getErrorLength() const;

private:
    uint8_t err_data;
};

}

#endif // BGP_KEEPALIVE_MSG_H_