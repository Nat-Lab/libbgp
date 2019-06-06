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
    ACTIVE,
    OPEN_SENT,
    OPEN_CONFIRM,
    ESTABLISHED,
    FEED // TODO: FEED status: sending routes but blocked by out_sink
};

class BgpFsm {
public:
    BgpFsm(BgpConfig config);

    uint32_t getAsn() const;
    uint32_t getBgpId() const;
    uint32_t getPeerAsn() const;
    uint32_t getPeerBgpId() const;
    const BgpRib& getRib() const;

    BufferPtr start();
    BufferPtr stop();

    BufferPtr run(const BufferPtr &buffer);
    BufferPtr run(const uint8_t *buffer, const size_t buffer_size);

private:
    void handleRouteEvent (RouteEvent ev);

    BgpSink in_sink;

    BgpState state;
    BgpConfig config;
    
    uint32_t peer_bgp_id;
    
};

}

#endif // BGP_FSM_H_