/**
 * @file bgp-notification-message.h
 * @author Nato Morichika <nat@nat.moe>
 * @brief The BGP notification message
 * @version 0.1
 * @date 2019-07-04
 * 
 * @copyright Copyright (c) 2019
 * 
 */
#ifndef BGP_NOTIFICATION_MSG_H_
#define BGP_NOTIFICATION_MSG_H_

#include <unistd.h>
#include "bgp-message.h"

namespace libbgp {

/**
 * @brief The BgpNotificationMessage object.
 * 
 * This is deserializer/serializer for BGP notification message body. If you 
 * want to deserializer/serializer a full BGP message. Take a look at BgpPacket
 * class.
 */
class BgpNotificationMessage : public BgpMessage {
public:
    BgpNotificationMessage(BgpLogHandler *logger);
    BgpNotificationMessage(BgpLogHandler *logger, uint8_t errcode, uint8_t subcode, const uint8_t *data, uint16_t data_len);
    ~BgpNotificationMessage();

    /**
     * @brief Notification message error code.
     * 
     */
    uint8_t errcode;

    /**
     * @brief Notification message error subcode.
     * 
     */
    uint8_t subcode;

    /**
     * @brief Notification message error data pointer.
     * 
     */
    uint8_t *data;

    /**
     * @brief Notification message error data length.
     * 
     */
    uint16_t data_len;

    ssize_t doPrint(size_t indent, uint8_t **to, size_t *buf_sz) const;
    ssize_t parse(const uint8_t *from, size_t msg_sz);
    ssize_t write(uint8_t *to, size_t buf_sz) const;
};

}

#endif // BGP_NOTIFICATION_MSG_H_