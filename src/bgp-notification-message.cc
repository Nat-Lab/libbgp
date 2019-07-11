/**
 * @file bgp-notification-message.cc
 * @author Nato Morichika <nat@nat.moe>
 * @brief The BGP notification message.
 * @version 0.1
 * @date 2019-07-04
 * 
 * @copyright Copyright (c) 2019
 * 
 */
#include "bgp-notification-message.h"
#include "bgp-errcode.h"
#include "value-op.h"
#include <stdlib.h>
#include <string.h>

namespace libbgp {

/**
 * @brief Construct a new Bgp Notification Message:: Bgp Notification Message object
 * 
 * @param loggger Pointer to logger object for error logging.
 */
BgpNotificationMessage::BgpNotificationMessage(__attribute__((unused)) BgpLogHandler *loggger) : BgpMessage(logger) {
    type = NOTIFICATION;
    err_data = 0;
    data_len = 0;
}

BgpNotificationMessage::~BgpNotificationMessage() {
    if (data_len > 0) free(data);
}

/**
 * @brief Construct a new Bgp Notification Message:: Bgp Notification Message object
 * 
 * @param errcode Error code
 * @param subcode Error subcode
 * @param data The error buffer pointer
 * @param data_len Length of error buffer.
 */
BgpNotificationMessage::BgpNotificationMessage(__attribute__((unused)) BgpLogHandler *loggger, uint8_t errcode, uint8_t subcode, const uint8_t *data, uint16_t data_len) : BgpMessage(logger) {
    type = NOTIFICATION;
    this->errcode = errcode;
    this->subcode = subcode;
    this->data_len = data_len;
    if (data_len > 0) {
        this->data = (uint8_t *) malloc(data_len);
        memcpy(this->data, data, data_len);
    }
}

ssize_t BgpNotificationMessage::parse(const uint8_t *from, size_t msg_sz) {
    if (msg_sz < 2) {
        setError(E_HEADER, E_LENGTH, NULL, 0);
        return -1;
    }

    const uint8_t *buffer = from;

    errcode = getValue<uint8_t>(&buffer);
    subcode = getValue<uint8_t>(&buffer);
    
    if (msg_sz > 2) {
        setError(E_HEADER, E_LENGTH, NULL, 0);
        return -1;
    }

    return msg_sz;
}

ssize_t BgpNotificationMessage::write(uint8_t *to, size_t buf_sz) const {
    if (buf_sz < (size_t) (data_len + 2)) return -1;
    uint8_t *buffer = to;

    putValue<uint8_t>(&buffer, errcode);
    putValue<uint8_t>(&buffer, subcode);
    memcpy(buffer, data, data_len);

    return data_len + 2;
}

ssize_t BgpNotificationMessage::doPrint(size_t indent, uint8_t **to, size_t *buf_sz) const {
    size_t written = 0;

    written += _print(indent, to, buf_sz, "NotificationMessage {\n");
    const char *err_msg = bgp_error_code_str[errcode];
    const char *err_sub_msg = bgp_error_code_str[0];
    switch (errcode) {
        case E_HEADER: err_sub_msg = bgp_header_error_subcode_str[subcode]; break;
        case E_OPEN: err_sub_msg = bgp_open_error_subcode_str[subcode]; break;
        case E_UPDATE: err_sub_msg = bgp_update_error_str[subcode]; break;
        case E_FSM: err_sub_msg = bgp_fsm_error_str[subcode]; break;
        case E_CEASE: err_sub_msg = bgp_cease_error_str[subcode]; break;
    }
    
    indent++; {
        written += _print(indent, to, buf_sz, "Error { %s }\n", err_msg);
        written += _print(indent, to, buf_sz, "SubError { %s }\n", err_sub_msg);
    }; indent--;

    written += _print(indent, to, buf_sz, "}\n");

    return written;
}

}