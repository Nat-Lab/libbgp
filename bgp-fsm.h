#ifndef BGP_FSM_H_
#define BGP_FSM_H_
#define BGP_FSM_SINK_SIZE 8192
#define BGP_FSM_BUFFER_SIZE 4096

#include "bgp-rib.h"
#include "bgp-config.h"
#include "bgp-sink.h"
#include "route-event-receiver.h"
#include <stdint.h>
#include <unistd.h>

namespace bgpfsm {

enum BgpState {
    IDLE,
    OPEN_SENT,
    OPEN_CONFIRM,
    ESTABLISHED,
    BROKEN
};

class BgpFsm : public RouteEventReceiver {
public:
    BgpFsm(const BgpConfig &config);
    ~BgpFsm();

    uint32_t getAsn() const;
    uint32_t getBgpId() const;
    uint32_t getPeerAsn() const;
    uint32_t getPeerBgpId() const;
    const BgpRib& getRib() const;
    BgpState getState() const;

    // start BGP (Idle -> Open Sent)
    // return value: 0: error, check errbuf, 1: success
    int start();

    // stop BGP (Any -> Idle)
    // return value: 0: error, check errbuf, 1: success
    int stop();

    // run FSM on buffer
    // return value: 
    // -1: fatal_error, FSM now BROKEN, check errbuf.
    // 0: error: NOTIFY sent, FSM now IDLE, errbuf might has details.
    // 1: success
    // 2: FSM reseted (NOTIFY received / FSM error), errbuf might has details.
    // 3: success, incomplete packet in sink
    int run(const uint8_t *buffer, const size_t buffer_size);

    // soft reset: send Administrative Reset and go to idle
    void resetSoft();

    // hard reset: go to idle
    void resetHard();

private:
    bool rib_local;
    bool rev_bus_exist;

    bool handleRouteEvent (const RouteEvent ev);

    int fsmEvalIdle(const BgpMessage *msg);
    int fsmEvalOpenSent(const BgpMessage *msg);
    int fsmEvalOpenConfirm(const BgpMessage *msg);
    int fsmEvalEstablished(const BgpMessage *msg);

    bool writeMessage(const BgpMessage &msg);

    BgpSink in_sink;
    BgpState state;
    BgpConfig config;
    BgpRib *rib;

    uint8_t *out_buffer;
    
    uint32_t peer_bgp_id;
};

}

#endif // BGP_FSM_H_