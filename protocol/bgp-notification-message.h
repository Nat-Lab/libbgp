#ifndef BGP_NOTIFICATION_MSG_H_
#define BGP_NOTIFICATION_MSG_H_

#include <unistd.h>
#include "bgp-message.h"

namespace bgpfsm {

class BgpNotificationMessage : public BgpMessage {
public:
    BgpNotificationMessage();
    /* TODO */

    ssize_t parse(const uint8_t *from, size_t msg_sz);
    ssize_t write(uint8_t *to, size_t buf_sz) const;

    const uint8_t* getError() const;
    ssize_t getErrorLength() const;
};

}

#endif // BGP_NOTIFICATION_MSG_H_