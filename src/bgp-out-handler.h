/**
 * @file bgp-out-handler.h
 * @author Nato Morichika <nat@nat.moe>
 * @brief The BGP FSM output handler.
 * @version 0.1
 * @date 2019-07-06
 * 
 * @copyright Copyright (c) 2019
 * 
 */
#ifndef BGP_OUT_HANDLEER_H_
#define BGP_OUT_HANDLEER_H_
#include <stdint.h>
#include <unistd.h>
namespace libbgp {

/**
 * @brief The BGP FSM output handler.
 * 
 * BgpOutHandler is used by BGP FSM to write BGP message to peer.
 * 
 */
class BgpOutHandler {
public:

    /**
     * @brief The output implementation.
     * 
     * @param buffer Pointer to the outgoing message buffer.
     * @param length Length of the message.
     * @return true The output was handled.
     * @return false The out was not handled.
     */
    virtual bool handleOut(const uint8_t *buffer, size_t length) = 0;

    /**
     * @brief State change notification. Will be call if FSM state changed.
     * 
     * @param old_state Old state.
     * @param new_state New state.
     */
    virtual void notifyStateChange(__attribute__((unused)) int old_state, __attribute__((unused)) int new_state) {}
    virtual ~BgpOutHandler() {}
};

/** 
 * @example route-event-bus.cc
 * Example of adding new routes to RIB while BGP FSM is running. Notify BGP FSM
 * to send updates to the peer with RouteEventBus. This example also shows how
 * you can implement your own BgpOutHandler and BgpLogHandler.
 * 
 * @example route-filter.cc
 * Example of using ingress/egress route filtering feature of BgpFsm. This 
 * example also shows how you can implement your own BgpOutHandler and 
 * BgpLogHandler.
 */

}

#endif // BGP_OUT_HANDLEER_H_