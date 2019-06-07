#ifndef BGP_OPEN_MSG_H_
#define BGP_OPEN_MSG_H_
#include <vector>
#include <unistd.h>
#include "bgp-message.h"

namespace bgpfsm {

class BgpOpenMessage : public BgpMessage {
public:
    BgpOpenMessage();
    BgpOpenMessage(uint32_t my_asn, uint16_t hold_time, uint32_t bgp_id);

    uint8_t version;
    uint32_t my_asn;
    uint16_t hold_time;
    uint32_t bgp_id;

    bool use_4b_asn;

    ssize_t parse(const uint8_t *from, size_t msg_sz);
    ssize_t write(uint8_t *to, size_t buf_sz) const;

    uint8_t getErrorCode() const;
    uint8_t getErrorSubCode() const;
    const uint8_t* getError() const;
    size_t getErrorLength() const;
};

}
#endif // BGP_OPEN_MSG_H_