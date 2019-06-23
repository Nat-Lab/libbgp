#ifndef BGP_FSM_H_
#define BGP_FSM_H_
#define BGP_FSM_SINK_SIZE 8192
#define BGP_FSM_BUFFER_SIZE 4096

#include "clock.h"
#include "bgp-rib.h"
#include "bgp-config.h"
#include "bgp-sink.h"
#include "route-event-receiver.h"
#include <stdint.h>
#include <unistd.h>
#include <mutex>

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

    // tick the clock (check for time-based event e.g. hold timer)
    // return value:
    // -1: fatal_error, FSM now BROKEN, check errbuf.
    // 0: error: NOTIFY sent, FSM now IDLE, errbuf might has details. (likely 
    // hold timer exipred)
    // 1: success
    // 2: success, keepalive sent
    int tick();

    // soft reset: send Administrative Reset and go to idle
    // return value:
    // -1: fatal_error, FSM now BROKEN, check errbuf.
    // 0: success, NOTIFY sent, FSM not IDLE
    int resetSoft();

    // hard reset: go to idle
    void resetHard();

private:
    bool rib_local;
    bool clock_local;
    bool rev_bus_exist;

    bool handleRouteEvent(const RouteEvent &ev);
    bool handleRouteCollisionEvent(const RouteCollisionEvent &ev);
    bool handleRouteWithdrawEvent(const RouteWithdrawEvent &ev);
    bool handleRouteAddEvent(const RouteAddEvent &ev);

    int validateState(uint8_t type);
    int fsmEvalIdle(const BgpMessage *msg);
    int fsmEvalOpenSent(const BgpMessage *msg);
    int fsmEvalOpenConfirm(const BgpMessage *msg);
    int fsmEvalEstablished(const BgpMessage *msg);

    // resloveCollison: resloving collsion
    // return value:
    // -1: fatal_error, FSM now BROKEN, check errbuf.
    // 0: there is a collision and this FSM should be dispose, FSM is now
    // IDLE and should be disposed
    // 1: there is a collision and the other FSM should be dispose.
    int resloveCollision(uint32_t peer_bgp_id, bool is_new);

    // since we handle open recv event in both idle and opensent, make it a func
    int openRecv(const BgpOpenMessage *open);

    bool writeMessage(const BgpMessage &msg);

    // prepare update message for advertisement (prepend my_asn, remove 
    // non-trans attrs)
    void prepareUpdateMessage(BgpUpdateMessage &update);

    BgpSink in_sink;
    BgpState state;
    BgpConfig config;
    BgpRib *rib;
    Clock *clock;

    std::mutex out_buffer_mutex;
    std::mutex in_sink_mutex;

    // pointer to output buffer
    uint8_t *out_buffer;

    // peer's bgp id
    uint32_t peer_bgp_id;

    // negotiated hold_timer
    uint16_t hold_timer;

    // time last event sent
    uint64_t last_sent;

    // time last event received
    uint64_t last_recv;

    // true if both peer & local support 4B ASN
    bool use_4b_asn;

};

}

#endif // BGP_FSM_H_