/**
 * @file bgp-bad-message.cc
 * @author Nato Morichika <nat@nat.moe>
 * @brief Container for invalid BGP message.
 * @version 0.1
 * @date 2019-07-05
 * 
 * @copyright Copyright (c) 2019
 * 
 */
#include "bgp-bad-message.h"
#include "bgp-errcode.h"
#include <unistd.h>

namespace libbgp {

/**
 * @brief Construct a new Bgp Bad Message:: Bgp Bad Message object
 * 
 * @param logger Pointer to logger object for error logging.
 * @param type Message type.
 */
BgpBadMessage::BgpBadMessage(BgpLogHandler *logger, uint8_t type) : BgpMessage(logger) {
    this->type = type;
}

ssize_t BgpBadMessage::parse(__attribute__((unused)) const uint8_t *from, __attribute__((unused)) size_t msg_sz) {
    logger->log(ERROR, "BgpBadMessage::parse: unknow message type %d\n", type);
    setError(E_HEADER, E_TYPE, &type, 1);
    return -1;
}

ssize_t BgpBadMessage::write(__attribute__((unused)) uint8_t *to, __attribute__((unused)) size_t msg_sz) const {
    logger->log(ERROR, "BgpBadMessage::write: you can't write a bad message\n");
    return -1;
}

ssize_t BgpBadMessage::doPrint(__attribute__((unused)) size_t indent, uint8_t **to, __attribute__((unused)) size_t *buf_sz) const {
    return _print(indent, to, buf_sz, "InvalidMessage { }\n");
}

}