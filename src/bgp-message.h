/**
 * @file bgp-message.h
 * @author Nato Morichika <nat@nat.moe>
 * @brief The BGP Message base.
 * @version 0.1
 * @date 2019-07-04
 * 
 * @copyright Copyright (c) 2019
 * 
 */
#ifndef BGP_MSG_H_
#define BGP_MSG_H_
#include <stdint.h>
#include <unistd.h>
#include "serializable.h"
#include "bgp-log-handler.h"

namespace libbgp {

/**
 * @brief BGP Message types.
 * 
 */
enum BgpMessageType {
    OPEN = 1,
    UPDATE = 2,
    NOTIFICATION = 3,
    KEEPALIVE = 4
};

/**
 * @brief The BgpMessage base class.
 * 
 */
class BgpMessage : public Serializable {
public:
    /**
     * @brief Construct a new Bgp Message object
     * 
     * @param logger Pointer to logger object for error logging.
     */
    BgpMessage(BgpLogHandler *logger) : Serializable(logger) {}

    uint8_t type;

    virtual ~BgpMessage() {}
};

}
#endif // BGP_MSG_H_