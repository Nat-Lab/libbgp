#include "bgp-sink.h"
#include "bgp-error.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <arpa/inet.h>

namespace bgpfsm {

BgpSink::BgpSink(size_t buffer_size) {
    this->buffer_size = buffer_size;
    this->buffer = (uint8_t *) malloc(buffer_size);
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

BufferPtr BgpSink::pourPtr() {
    std::lock_guard<std::mutex> lock(mutex);
    assert(offset_end >= offset_start);

    uint8_t *cur = this->buffer + offset_start;

    if (offset_end - offset_start < 19) return BufferPtr(NULL, 0);
    if (memcmp(cur, "\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff", 16) != 0) {
        _bgp_error("BgpSink::pourPtr: invalid BGP marker.\n");
        return BufferPtr(NULL, -1);
    }

    uint16_t field_len = ntohs(*(uint16_t *) (cur + 16));

    if (field_len < 19 || field_len > 4096) {
        _bgp_error("BgpSink::pourPtr: invalid BGP packet length (%d).\n", field_len);
        return BufferPtr(NULL, -2);
    }

    ssize_t bytes = getBytesInSink();
    if (field_len > bytes) return BufferPtr(NULL, 0); // incomplete packet, wait for more.

    offset_start += field_len;

    return BufferPtr(cur, field_len);
}

BufferPtr BgpSink::pourPtrAll() {
    std::lock_guard<std::mutex> lock(mutex);
    assert(offset_end >= offset_start);
    ssize_t sz = getBytesInSink();
    uint8_t *cur = buffer + offset_start;
    offset_start = offset_end = 0;
    return BufferPtr(cur, sz);
}

ssize_t BgpSink::pour(uint8_t *buffer, size_t len) {
    std::lock_guard<std::mutex> lock(mutex);
    BufferPtr bp = pourPtr();
    
    if (bp.buffer_size < 0) return -1;
    if (bp.buffer_size == 0) return 0;

    if ((size_t) bp.buffer_size > len) {
        _bgp_error("BgpSink::pour: not enough space in output buffer (%d more needed).", bp.buffer_size - len);
        offset_start -= bp.buffer_size; // un-pour
        return -1;
    }

    memcpy(buffer, bp.buffer, bp.buffer_size);

    return bp.buffer_size;
}

ssize_t BgpSink::pourAll(uint8_t *buffer, size_t len) {
    std::lock_guard<std::mutex> lock(mutex);
    assert(offset_end >= offset_start);
    size_t bytes = getBytesInSink();

    if (bytes == 0) return 0;
    if (len < bytes) {
        _bgp_error("BgpSink::pourAll: not enough space in output buffer (%d more needed).", bytes - len);
        return -1;
    }

    memcpy(buffer, this->buffer + offset_start, bytes);
    offset_end = offset_start = 0;
    return bytes;
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