#ifndef BGP_NOTIFICATION_MSG_H_
#define BGP_NOTIFICATION_MSG_H_

#include <unistd.h>
#include "bgp-message.h"

namespace bgpfsm {

class BgpNotificationMessage : public BgpMessage {
public:
    BgpNotificationMessage();
    BgpNotificationMessage(uint8_t errcode, uint8_t subcode, const uint8_t *data, uint16_t data_len);
    ~BgpNotificationMessage();

    uint8_t errcode;
    uint8_t subcode;
    uint8_t *data;
    uint16_t data_len;

    ssize_t parse(const uint8_t *from, size_t msg_sz);
    ssize_t write(uint8_t *to, size_t buf_sz) const;

    // don't confuse by the function below, those getters were mean to be used
    // to get parsing error (e.g. Unsupported Optional Parameter in OPEN msg),
    // they are not getters for NotificationMessage members.

    uint8_t getErrorCode() const;
    uint8_t getErrorSubCode() const;
    const uint8_t* getError() const;
    size_t getErrorLength() const;

private:
    uint8_t err_data;
};

}

#endif // BGP_NOTIFICATION_MSG_H_