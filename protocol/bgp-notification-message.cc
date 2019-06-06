#include "bgp-notification-message.h"

namespace bgpfsm {

BgpNotificationMessage::BgpNotificationMessage () {}

ssize_t BgpNotificationMessage::parse(const uint8_t *from, size_t msg_sz) {

}

ssize_t BgpNotificationMessage::write(uint8_t *to, size_t msg_sz) const {
    
}

}