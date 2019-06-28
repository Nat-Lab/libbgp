#ifndef BGP_OPEN_MSG_H_
#define BGP_OPEN_MSG_H_
#include <vector>
#include <unistd.h>
#include "bgp-message.h"
#include "bgp-capability.h"

namespace bgpfsm {

class BgpOpenMessage : public BgpMessage {
public:
    BgpOpenMessage();
    BgpOpenMessage(uint32_t my_asn, uint16_t hold_time, uint32_t bgp_id);
    BgpOpenMessage(uint32_t my_asn, uint16_t hold_time, const char* bgp_id);
    ~BgpOpenMessage();

    uint8_t version;
    uint32_t my_asn;
    uint16_t hold_time;

    // bgp-id is in host-byte
    uint32_t bgp_id;

    // also avaliable thru getCapabilities()
    bool use_4b_asn;

    ssize_t doPrint(size_t indent, uint8_t **to, size_t *buf_sz) const;
    ssize_t parse(const uint8_t *from, size_t msg_sz);
    ssize_t write(uint8_t *to, size_t buf_sz) const;

    // bgp-fsm only supports 4-bytes asn capability. getCapabilities() allows
    // you to get a full, read-only list of cpabilities.
    const std::vector<BgpCapability>& getCapabilities() const;
private:
    std::vector<BgpCapability> capabilities;
};

}
#endif // BGP_OPEN_MSG_H_