/**
 * @file bgp-config.h
 * @author Nato Morichika <nat@nat.moe>
 * @brief The BGP FSM configuration object.
 * @version 0.1
 * @date 2019-07-04
 * 
 * @copyright Copyright (c) 2019
 * 
 */
#ifndef BGP_CONFIG_H_
#define BGP_CONFIG_H_
#include <stdint.h>
#include "clock.h"
#include "bgp-rib4.h"
#include "bgp-filter4.h"
#include "bgp-out-handler.h"
#include "bgp-log-handler.h"
#include "route-event-bus.h"

namespace libbgp {

/**
 * @brief The BGP FSM configuration object.
 * 
 */
typedef struct BgpConfig {
    /**
     * @brief Ingress route filters.
     * 
     * Ingress route filters are applied on the routes received from the peer. 
     */
    BgpFilterRules in_filters;

    /**
     * @brief Egress route filters.
     * 
     * Egress route filters are applied when sending routes to the peer. 
     */
    BgpFilterRules out_filters;

    /**
     * @brief The output handler.
     * 
     * The output handler is invoked whenever BGP FSM needs to write data.
     */
    BgpOutHandler *out_handler;

    /**
     * @brief The log handler.
     * 
     * The log handler is invoked whenever BGP FSM needs to log information. If 
     * you set this to null, FSM will output to the stderr with log level INFO.
     */
    BgpLogHandler *log_handler;

    /**
     * @brief Pointer to the IPv4 Routing Information Base object.
     * 
     * BGP FSM will use this RIB object to store routing information. If you 
     * would like to share RIB across different BGP FSMs, or pre-fill the RIB 
     * with some routes, you can create the RIB object yourself and pass it as 
     * configuration parameter here. If you set this to NULL, a new RIB will be 
     * created by BGP FSM. You can get it by calling `BgpFsm::getRib`.
     */
    BgpRib4 *rib;

    // pointer to event bus, route add/withdraw events will be sent to other
    // FSM thru event bus, won't use if NULL

    /**
     * @brief Pointer to the route event bus.
     * 
     * The route event bus is used to share information and communicate with 
     * other BGP FSMs. For example, route add/withdrawn events are sent to other
     * FSMs with route event bus. Collision resolution also depends on the route
     * event bus. You will need to create a route event bus object and pass it 
     * in as the configuration parameter for every FSMs. You may set route event
     * bus to NULL if you are only using one FSM.
     */
    RouteEventBus *rev_bus;

    /**
     * @brief Disable collision detection.
     * 
     * Set this parameter to true will disable collision detection.
     */
    bool no_collision_detection;

    /**
     * @brief Enable four octets ASN support (RFC 6793)
     * 
     * Set this parameter to true will eable four octets ASN support.
     */
    bool use_4b_asn;

    /**
     * @brief Local ASN.
     * 
     */
    uint32_t asn;

    /**
     * @brief Peer ASN. Set to 0 will make BGP FSM accept peer with any ASN.
     * 
     */
    uint32_t peer_asn;

    /**
     * @brief Local BGP ID in network byte order.
     * 
     */
    uint32_t router_id;

    /**
     * @brief The prefix of the peering LAN in network-byte order.
     * 
     * Peering LAN information is used to check the validity of the nexthop 
     * attribute of the received routes. Routes received from the peer with a 
     * nexthop outside the peering LAN will be ignored.
     */
    uint32_t peering_lan_prefix;

    /**
     * @brief The netmask of the peering LAN in CIDR notation.
     * 
     * Peering LAN information is used to check the validity of the nexthop 
     * attribute of the received routes. Routes received from the peer with a 
     * nexthop outside the peering LAN will be ignored.
     */
    uint8_t peering_lan_length;

    /**
     * @brief Disable ingress nexthop validation.
     * 
     * If true, BGP FSM will accept route with any nexthop, regardless of the 
     * peering LAN.
     */
    bool no_nexthop_check;

    /**
     * @brief The default nexthop to use.
     * 
     * Default nexthop is used when sending routes to the peer. The nexthop 
     * value will remain unchanged if it is inside peering LAN. Default nexthop 
     * is used only when the nexthop attribute of an egress route is not in 
     * peering LAN. 
     */
    uint32_t nexthop;

    /**
     * @brief Forced default nexthop.
     * 
     * If this is set to true, the `nexthop` configuration parameter will always
     * be used as nexthop, regardless of the peering LAN.
     */
    bool forced_default_nexthop;

    /**
     * @brief The hold timer.
     * 
     */
    uint16_t hold_timer;

    /**
     * @brief The clock to use for time-based events.
     * 
     * Time-based events like hold timer expired needs to refer to the clock. 
     * Sometime we may not want to use the system clock. For example, if BgpFsm
     * is used inside a simulator like ns3, we will need to use the simulated
     * clock instead of the real-time clock. Set this to NULL if you want to use
     * a real-time clock.
     */
    Clock *clock;
} BgpConfig;

/**
 * @example peer-and-print.cc
 * A simple BGP speaker listen on TCP 0.0.0.0:179, wait for a peer, and print 
 * all BGP messages sent/received with BgpFsm. 
 * 
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

#endif // BGP_CONFIG_H_ 