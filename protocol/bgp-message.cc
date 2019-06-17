#include "bgp-message.h"
#include <stdlib.h>
#include <assert.h>
#include <string.h>

namespace bgpfsm {

BgpMessage::BgpMessage() {
    err_code = 0;
    err_subcode = 0;
    err_data = NULL;
    err_len = 0;
}

BgpMessage::~BgpMessage() {
    if (err_len > 0) free(err_data);
}

void BgpMessage::setError(uint8_t err, uint8_t suberr, const uint8_t *data, size_t data_len) {
    err_code = err;
    err_subcode = suberr;

    // err_buf_len not 0, setError() when error already there?
    assert(err_len == 0);

    if (data_len == 0) return;
    err_len = data_len;
    err_data = (uint8_t *) malloc(err_len);
    memcpy(err_data, data, data_len);
}

uint8_t BgpMessage::getErrorCode() const {
    return err_code;
}

uint8_t BgpMessage::getErrorSubCode() const {
    return err_subcode;
}

const uint8_t* BgpMessage::getError() const {
    return err_data;
}

size_t BgpMessage::getErrorLength() const {
    return err_len;
}

}