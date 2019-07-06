/**
 * @file bgp-bad-message.h
 * @author Nato Morichika <nat@nat.moe>
 * @brief Container for invalid BGP message.
 * @version 0.1
 * @date 2019-07-05
 * 
 * @copyright Copyright (c) 2019
 * 
 */
#ifndef BGP_BAD_MSG_H_
#define BGP_BAD_MSG_H_

#include "bgp-message.h"
#include <stdint.h>

namespace libbgp {

/**
 * @brief The BgpBadMessage class.
 * 
 * Container class for invalid BGP message.
 */
class BgpBadMessage : public BgpMessage {
public:
    BgpBadMessage(BgpLogHandler *logger, uint8_t type);

    ssize_t doPrint(size_t indent, uint8_t **to, size_t *buf_sz) const;
    ssize_t parse(const uint8_t *from, size_t msg_sz);
    ssize_t write(uint8_t *to, size_t buf_sz) const;
};

}

#endif // BGP_BAD_MSG_H_