#ifndef BGP_FSM_H_
#define BGP_FSM_H_
#define BGP_FSM_BUFFER_SIZE_IN 8192

#include "bgp-rib.h"
#include "bgp-config.h"
#include "bgp-sink.h"
#include "route-event-bus.h"
#include <stdint.h>
#include <unistd.h>

namespace bgpfsm {

class RouteEventBus;

enum BgpState {
    IDLE,
    OPEN_SENT,
    OPEN_CONFIRM,
    ESTABLISHED
};

class BgpFsm {
public:
    BgpFsm(BgpConfig config);

    uint32_t getAsn() const;
    uint32_t getBgpId() const;
    uint32_t getPeerAsn() const;
    uint32_t getPeerBgpId() const;
    const BgpRib& getRib() const;

    // start BGP (Idle -> Open Sent)
    // return value: false/0: error, check errbuf, true/1: success
    int start();

    // stop BGP (Any -> Idle)
    // return value: false/0: error, check errbuf, true/1: success
    int stop();

    // run FSM on buffer
    // return value: fatal_error/-1, check errbuf, true/1: success, 
    // pending/0: in_sink non empty after run, error/2: FSM reseted
    // (NOTIFICATION received / FSM error)
    int run(const uint8_t *buffer, const size_t buffer_size);

private:
    void handleRouteEvent (RouteEvent ev);

    int fsmEvalIdle(const uint8_t *buffer, const size_t buffer_size);
    int fsmEvalOpenSent(const uint8_t *buffer, const size_t buffer_size);
    int fsmEvalOpenConfirm(const uint8_t *buffer, const size_t buffer_size);
    int fsmEvalEstablished(const uint8_t *buffer, const size_t buffer_size);

    BgpSink in_sink;

    BgpState state;
    BgpConfig config;
    
    uint32_t peer_bgp_id;
};

}

#endif // BGP_FSM_H_