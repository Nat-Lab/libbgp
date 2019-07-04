#ifndef BGP_MSG_H_
#define BGP_MSG_H_
#include <stdint.h>
#include <unistd.h>
#include "serializable.h"

namespace libbgp {

enum BgpMessageType {
    OPEN = 1,
    UPDATE = 2,
    NOTIFICATION = 3,
    KEEPALIVE = 4
};

class BgpMessage : public Serializable {
public:
    uint8_t type;

    virtual ~BgpMessage() {}
};

}
#endif // BGP_MSG_H_