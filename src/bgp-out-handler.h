#ifndef BGP_OUT_HANDLEER_H_
#define BGP_OUT_HANDLEER_H_
#include <stdint.h>
#include <unistd.h>
namespace libbgp {

class BgpOutHandler {
public:
    virtual bool handleOut(const uint8_t *buffer, size_t length) = 0;
    virtual ~BgpOutHandler() {}
};

}

#endif // BGP_OUT_HANDLEER_H_