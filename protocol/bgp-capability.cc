#include "bgp-capability.h"
#include <stdlib.h>
#include <string.h>

namespace bgpfsm {

BgpCapabilityUnknow::BgpCapabilityUnknow(const uint8_t *buffer, uint8_t len) {
    length = len;

    if (len > 0) {
        value = (uint8_t *) malloc(len);
        memcpy(value, buffer, len);
    }
}

BgpCapabilityUnknow::~BgpCapabilityUnknow() {
    if (length > 0) free(value);
}

}