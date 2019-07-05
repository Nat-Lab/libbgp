#include "bgp-sink.h"
#include "bgp-error.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <arpa/inet.h>

namespace libbgp {

BgpSink::BgpSink(bool use_4b_asn, size_t buffer_size) {
    this->buffer_size = buffer_size;
    this->buffer = (uint8_t *) malloc(buffer_size);
    this->use_4b_asn = use_4b_asn;
    offset_start = offset_end = 0;
}

BgpSink::~BgpSink() {
    free(buffer);
}

ssize_t BgpSink::fill(const uint8_t *buffer, size_t len) {
    std::lock_guard<std::mutex> lock(mutex);
    assert(offset_end >= offset_start);
    if (len > buffer_size) {
        _bgp_error("BgpSink::fill: buffer length (%d) > sink size (%d).\n", len, buffer_size);
        return -1;
    }

    if (offset_end + len > buffer_size) {
        settle(); 
        if (offset_end + len > buffer_size) {
            _bgp_error("BgpSink::fill: not enough space left in sink (%d more needed).\n", buffer_size - (offset_end + len));
            return -1;
        }
    }

    memcpy(this->buffer + offset_end, buffer, len);
    offset_end += len;

    return len;
}

int BgpSink::pour(BgpPacket **pkt) {
    std::lock_guard<std::mutex> lock(mutex);
    assert(offset_end >= offset_start);

    uint8_t *cur = this->buffer + offset_start;

    if (offset_end - offset_start < 19) return 0;
    if (memcmp(cur, "\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff", 16) != 0) {
        _bgp_error("BgpSink::pour: invalid BGP marker.\n");
        return -2;
    }

    uint16_t field_len = ntohs(*(uint16_t *) (cur + 16));

    if (field_len < 19 || field_len > 4096) {
        _bgp_error("BgpSink::pourPtr: invalid BGP packet length (%d).\n", field_len);
        return -2;
    }

    ssize_t bytes = getBytesInSink();
    if (field_len > bytes) return 0; // incomplete packet, wait for more.

    offset_start += field_len;

    BgpPacket *new_pkt = new BgpPacket(use_4b_asn);
    ssize_t par_ret = new_pkt->parse(cur, field_len);

    *pkt = new_pkt;

    if (par_ret < 0) return -1;

    assert(par_ret == field_len);
    return par_ret;
}

void BgpSink::settle() {
    if (offset_start > 0) {
        if (offset_start == offset_end) offset_start = offset_end = 0;
        else memmove(buffer, buffer + offset_start, offset_end - offset_start);
    }
}

void BgpSink::drain() {
    std::lock_guard<std::mutex> lock(mutex);
    offset_end = offset_start = 0;
}

size_t BgpSink::getBytesInSink() const {
    return offset_end - offset_start;
}

}