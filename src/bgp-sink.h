/**
 * @file bgp-sink.h
 * @author Nato Morichika <nat@nat.moe>
 * @brief The BGP sink.
 * @version 0.1
 * @date 2019-07-05
 * 
 * @copyright Copyright (c) 2019
 * 
 */
#ifndef BGP_SINK_H_
#define BGP_SINK_H_
#include <mutex>
#include <stdint.h>
#include <unistd.h>
#include "bgp-packet.h"
#include "bgp-log-handler.h"

namespace libbgp {

/**
 * @brief The BgpSink class.
 * 
 * BGP sink is a packet buffering utility for BGP. It consumes binary buffer to
 * fill the sink (buffer) and allows users to get full BGP packet from the sink
 * (buffer). This is useful since BGP uses TCP, and TCP streams the data. (so we
 * might not get a full BGP packet in buffer every time)
 */
class BgpSink {
public:
    // create a new sink
    BgpSink(bool use_4b_asn);

    // feed stream of packets into sink
    ssize_t fill(const uint8_t *buffer, size_t len);

    // get a pointer to next packet from sink (might chane if fill())
    //BufferPtr pourPtr();

    // get a pointer to datas in sink (might chane if fill())
    //BufferPtr pourPtrAll();

    // get and remove a single BGP packet from sink (max size = 4096)
    //ssize_t pour(uint8_t *buffer, size_t len);

    // get a packet from sink and remove that packet from sink. if no packet
    // currently avaliable, 0 will be returned. if error, -2 will be returned, 
    // if error happend during parse but the packet object has already been
    // created, -1 will be returned.
    // otherwise, pkt is set to point to the new packet, and bytes drained is
    // returned. (> 0)
    ssize_t pour(BgpPacket **pkt);

    // get and remove all packets from sink (max size = sink buffer size)
    //ssize_t pourAll(uint8_t *buffer, size_t len);

    // get number of bytes currently in sink
    size_t getBytesInSink() const;

    // discard packets in sink
    void drain();

    void setLogger(BgpLogHandler *logger);

    ~BgpSink();

private:
    // settle the sink
    void settle();

    // exapnd the sink
    void expand();

    uint8_t *buffer;
    size_t buffer_size;
    size_t offset_start;
    size_t offset_end;
    bool use_4b_asn;
    std::recursive_mutex mutex;
    BgpLogHandler *logger;
};

}

#endif // BGP_SINK_H_