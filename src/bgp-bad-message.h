#ifndef BGP_BAD_MSG_H_
#define BGP_BAD_MSG_H_

#include "bgp-message.h"
#include <stdint.h>

namespace libbgp {

class BgpBadMessage : public BgpMessage {
public:
    BgpBadMessage(BgpLogHandler *logger, uint8_t type);

    ssize_t doPrint(size_t indent, uint8_t **to, size_t *buf_sz) const;
    ssize_t parse(const uint8_t *from, size_t msg_sz);
    ssize_t write(uint8_t *to, size_t buf_sz) const;
};

}

#endif // BGP_BAD_MSG_H_