#ifndef BGP_UPDATE_MSG_H_
#define BGP_UPDATE_MSG_H_
#include <vector>
#include <unistd.h>
#include "bgp-message.h"
#include "bgp-path-attrib.h"

namespace bgpfsm {

class BgpUpdateMessage : public BgpMessage {
public:
    BgpUpdateMessage();

    std::vector<Route> withdrawn_routes;
    std::vector<BgpPathAttrib> path_attribute;
    std::vector<Route> nlri;

    BgpPathAttrib *getAttrib(BgpPathAttribType type);
    void addAttrib(const BgpPathAttrib &attrib);

    void addWithdrawn(uint32_t prefix, uint8_t length);
    void addNlri(uint32_t prefix, uint8_t length);

    ssize_t parse(const uint8_t *from, size_t msg_sz);
    ssize_t write(uint8_t *to, size_t buf_sz) const;

    uint8_t getErrorCode() const;
    uint8_t getErrorSubCode() const;
    const uint8_t* getError() const;
    size_t getErrorLength() const;
};

}
#endif // BGP_UPDATE_MSG_H_