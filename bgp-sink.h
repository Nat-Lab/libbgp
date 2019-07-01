#ifndef BGP_SINK_H_
#define BGP_SINK_H_
#include <mutex>
#include <stdint.h>
#include <unistd.h>
#include "protocol/bgp-packet.h"

namespace bgpfsm {

// a sink for BGP packets
class BgpSink {
public:
    // create a new sink
    BgpSink(bool use_4b_asn, size_t buffer_size);

    // feed stream of packets into sink
    ssize_t fill(const uint8_t *buffer, size_t len);

    // get a pointer to next packet from sink (might chane if fill())
    //BufferPtr pourPtr();

    // get a pointer to datas in sink (might chane if fill())
    //BufferPtr pourPtrAll();

    // get and remove a single BGP packet from sink (max size = 4096)
    //ssize_t pour(uint8_t *buffer, size_t len);

    // get a packet from sink and remove that packet from sink. if no packet
    // currently avaliable, 0 will be returned. if error, -1 will be returned.
    // otherwise, pkt is set to point to the new packet, and bytes drained is
    // returned. (> 0)
    int pour(BgpPacket **pkt);

    // get and remove all packets from sink (max size = sink buffer size)
    //ssize_t pourAll(uint8_t *buffer, size_t len);

    // get number of bytes currently in sink
    size_t getBytesInSink() const;

    // discard packets in sink
    void drain();

    ~BgpSink();

private:
    // settle the sink
    void settle();

    uint8_t *buffer;
    size_t buffer_size;
    size_t offset_start;
    size_t offset_end;
    bool use_4b_asn;
    std::mutex mutex;
};

}

#endif // BGP_SINK_H_