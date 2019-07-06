/**
 * @file bgp-sink.cc
 * @author Nato Morichika <nat@nat.moe>
 * @brief The BGP sink.
 * @version 0.1
 * @date 2019-07-05
 * 
 * @copyright Copyright (c) 2019
 * 
 */
#include "bgp-sink.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <arpa/inet.h>

namespace libbgp {

/**
 * @brief Construct a new Bgp Sink:: Bgp Sink object
 * 
 * @param logger Pointer to logger object for error logging.
 * @param use_4b_asn Enable four octets ASN support.
 * @param buffer_size Size of the sink buffer.
 */
BgpSink::BgpSink(BgpLogHandler *logger, bool use_4b_asn, size_t buffer_size) {
    this->buffer_size = buffer_size;
    this->buffer = (uint8_t *) malloc(buffer_size);
    this->use_4b_asn = use_4b_asn;
    this->logger = logger;
    offset_start = offset_end = 0;
}

/**
 * @brief Destroy the Bgp Sink:: Bgp Sink object
 * 
 */
BgpSink::~BgpSink() {
    free(buffer);
}

/**
 * @brief Fill the sink with data.
 * 
 * @param buffer The pointer to data buffer.
 * @param len The length of data.
 * @return ssize_t Bytes consumed.
 * @retval -1 Failed to fill sink. error may be written to stderr with log
 * handler.
 * @retval >=0 Bytes consumed.
 */
ssize_t BgpSink::fill(const uint8_t *buffer, size_t len) {
    std::lock_guard<std::mutex> lock(mutex);
    assert(offset_end >= offset_start);
    if (len > buffer_size) {
        logger->stderr("BgpSink::fill: buffer length (%d) > sink size (%d).\n", len, buffer_size);
        return -1;
    }

    if (offset_end + len > buffer_size) {
        settle(); 
        if (offset_end + len > buffer_size) {
            logger->stderr("BgpSink::fill: not enough space left in sink (%d more needed).\n", buffer_size - (offset_end + len));
            return -1;
        }
    }

    memcpy(this->buffer + offset_end, buffer, len);
    offset_end += len;

    return len;
}

/**
 * @brief Pour BGP packet out from sink.
 * 
 * Get a packet from sink and remove that packet from sink.
 * 
 * @param pkt Pointer to BgpPacket pointer.
 * @return ssize_t Bytes poured.
 * @retval -2 Failed to pour packet. error may be written to stderr with log
 * handler.
 * @retval -1 Packet poured, but parse error occurred. error may be written to 
 * stderr with log handler, notification message data that needs to be sent to
 * peer may be avaliable.
 * @retval >=0 Bytes poured.
 */
ssize_t BgpSink::pour(BgpPacket **pkt) {
    std::lock_guard<std::mutex> lock(mutex);
    assert(offset_end >= offset_start);

    uint8_t *cur = this->buffer + offset_start;

    if (offset_end - offset_start < 19) return 0;
    if (memcmp(cur, "\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff", 16) != 0) {
        logger->stderr("BgpSink::pour: invalid BGP marker.\n");
        return -2;
    }

    uint16_t field_len = ntohs(*(uint16_t *) (cur + 16));

    if (field_len < 19 || field_len > 4096) {
        logger->stderr("BgpSink::pourPtr: invalid BGP packet length (%d).\n", field_len);
        return -2;
    }

    ssize_t bytes = getBytesInSink();
    if (field_len > bytes) return 0; // incomplete packet, wait for more.

    offset_start += field_len;

    BgpPacket *new_pkt = new BgpPacket(logger, use_4b_asn);
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

/**
 * @brief Drain the sink. (Remove all data from sink buffer)
 * 
 */
void BgpSink::drain() {
    std::lock_guard<std::mutex> lock(mutex);
    offset_end = offset_start = 0;
}

/**
 * @brief Get amount to data in sink.
 * 
 * @return size_t Data size in bytes.
 */
size_t BgpSink::getBytesInSink() const {
    return offset_end - offset_start;
}

}