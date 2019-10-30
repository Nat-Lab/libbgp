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
#include "bgp-rib6.h"
#include "bgp-filter.h"
#include "bgp-out-handler.h"
#include "bgp-log-handler.h"
#include "route-event-bus.h"

namespace libbgp {

/**
 * @brief The BGP FSM configuration object.
 * 
 */
typedef struct BgpConfig {
    BgpConfig() {
        rib4 = NULL;
        rib6 = NULL;
        rev_bus = NULL;
        mp_bgp_ipv4 = mp_bgp_ipv6 = false;
        no_collision_detection = false;
        use_4b_asn = true;
        peer_asn = 0;
        forced_default_nexthop4 = no_nexthop_check4 = false;
        clock = NULL;
        hold_timer = 120;
        forced_default_nexthop6 = no_nexthop_check6 = false;
        allow_local_as = 0;
        weight = 0;
        no_autotick = false;
        ibgp_alter_nexthop = false;
    }

    /**
     * @brief IPv4 Ingress route filters.
     * 
     * Ingress route filters are applied on the routes received from the peer. 
     * (default: accept any)
     */
    BgpFilterRules in_filters4;

    /**
     * @brief IPv4 Egress route filters.
     * 
     * Egress route filters are applied when sending routes to the peer. 
     * (default: accept any)
     */
    BgpFilterRules out_filters4;

    /**
     * @brief IPv6 Ingress route filters.
     * 
     * Ingress route filters are applied on the routes received from the peer. 
     * (default: accept any)
     */
    BgpFilterRules in_filters6;

    /**
     * @brief IPv6 Ingress route filters.
     * 
     * Egress route filters are applied when sending routes to the peer. 
     * (default: accept any)
     */
    BgpFilterRules out_filters6;

    /**
     * @brief The output handler.
     * 
     * The output handler is invoked whenever BGP FSM needs to write data.
     * (required, no default value)
     */
    BgpOutHandler *out_handler;

    /**
     * @brief The log handler.
     * 
     * The log handler is invoked whenever BGP FSM needs to log information. If 
     * you set this to null, FSM will output to the stderr with log level INFO.
     * (required, no default value)
     */
    BgpLogHandler *log_handler;

    /**
     * @brief Pointer to the IPv4 Routing Information Base object.
     * 
     * BGP FSM will use this RIB object to store routing information. If you 
     * would like to share RIB across different BGP FSMs, or pre-fill the RIB 
     * with some routes, you can create the RIB object yourself and pass it as 
     * configuration parameter here. If you set this to NULL, a new RIB will be 
     * created by BGP FSM. You can get it by calling `BgpFsm::getRib4`.
     * (default: NULL)
     */
    BgpRib4 *rib4;

    /**
     * @brief Pointer to the IPv6 Routing Information Base object.
     * 
     * BGP FSM will use this RIB object to store routing information. If you 
     * would like to share RIB across different BGP FSMs, or pre-fill the RIB 
     * with some routes, you can create the RIB object yourself and pass it as 
     * configuration parameter here. If you set this to NULL, a new RIB will be 
     * created by BGP FSM. You can get it by calling `BgpFsm::getRib6`.
     * (default: NULL)
     */
    BgpRib6 *rib6;

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
     * (default: NULL)
     */
    RouteEventBus *rev_bus;

    /**
     * @brief Disable collision detection.
     * 
     * Set this parameter to true will disable collision detection.
     * (default: false)
     */
    bool no_collision_detection;

    /**
     * @brief Enable four octets ASN support (RFC 6793)
     * 
     * Set this parameter to true will eable four octets ASN support.
     * (default: true)
     */
    bool use_4b_asn;

    /**
     * @brief Enable MP-BGP IPv4 support.
     * 
     * Set this parameter to true will enable IPv4 support with MP-BGP. Note
     * that even without MP-BGP IPv4, IPv4 routing information will still be
     * exchanged with normal BGP session. This should only be set when 
     * mp_bgp_ipv6 is set and you want to carry ipv4 routing infromation on the
     * same session.
     * (default: false)
     */
    bool mp_bgp_ipv4;

    /**
     * @brief Enable MP-BGP IPv6 support.
     * 
     * Set this parameter to true will enable IPv6 support with MP-BGP. Setting
     * this to true will disable IPv4 unless mp_bgp_ipv4 is also set to true.
     * (default: false)
     */
    bool mp_bgp_ipv6;

    /**
     * @brief Local ASN.
     * (required, no default value)
     */
    uint32_t asn;

    /**
     * @brief Peer ASN. Set to 0 will make BGP FSM accept peer with any ASN.
     * (default: 0)
     */
    uint32_t peer_asn;

    /**
     * @brief Local BGP ID in network byte order.
     * (required, no default value)
     */
    uint32_t router_id;

    /**
     * @brief The prefix of the IPv4 peering LAN.
     * 
     * Peering LAN information is used to check the validity of the nexthop 
     * attribute of the received routes. Routes received from the peer with a 
     * nexthop outside the peering LAN will be ignored. When sending routes to
     * peer, if nexthop attribute in RIB is not in peering LAN, default nexthop
     * will be used.
     * (required, no default value)
     */
    Prefix4 peering_lan4;

    /**
     * @brief The prefix of the IPv6 peering LAN.
     * 
     * Peering LAN information is used to check the validity of the nexthop 
     * attribute of the received routes. Routes received from the peer with a 
     * nexthop outside the peering LAN will be ignored. When sending routes to
     * peer, if nexthop attribute in RIB is not in peering LAN, default nexthop
     * will be used.
     * (required, no default value)
     */
    Prefix6 peering_lan6;

    /**
     * @brief Disable IPv4 ingress nexthop validation.
     * 
     * If true, BGP FSM will accept route with any nexthop, regardless of the 
     * peering LAN.
     * (default: false)
     */
    bool no_nexthop_check4;

    /**
     * @brief The default IPv4 nexthop to use.
     * 
     * Default nexthop is used when sending routes to the peer. The nexthop 
     * value will remain unchanged if it is inside peering LAN. Default nexthop 
     * is used only when the nexthop attribute of an egress route is not in 
     * peering LAN. 
     * (required, no default value)
     */
    uint32_t default_nexthop4;

    /**
     * @brief Forced IPv4 default nexthop.
     * 
     * If this is set to true, the `default_nexthop4` configuration parameter 
     * will always be used as nexthop, regardless of the peering LAN. (for IBGP,
     * this will only work if ibgp_alter_nexthop is set to true.)
     * (default: false)
     */
    bool forced_default_nexthop4;

    /**
     * @brief Disable IPv6 ingress nexthop validation.
     * 
     * If true, BGP FSM will accept route with any nexthop, regardless of the 
     * peering LAN.
     * (default: false)
     */
    bool no_nexthop_check6;

    /**
     * @brief The default global IPv6 nexthop to use.
     * 
     * Default nexthop is used when sending routes to the peer. The nexthop 
     * value will remain unchanged if it is inside peering LAN. Default nexthop 
     * is used only when the nexthop attribute of an egress route is not in 
     * peering LAN. 
     *  (required, no default value)
     */
    uint8_t default_nexthop6_global[16];

    /**
     * @brief The default link-local IPv6 nexthop to use.
     * 
     * The link local nexhop. You may set this to all-zero if you don't want
     * to send a link-local nexthop.
     * 
     * Default nexthop is used when sending routes to the peer. The nexthop 
     * value will remain unchanged if it is inside peering LAN. Default nexthop 
     * is used only when the nexthop attribute of an egress route is not in 
     * peering LAN. 
     *  (required, no default value)
     */
    uint8_t default_nexthop6_linklocal[16];

    /**
     * @brief Forced IPv6 default nexthop.
     * 
     * If this is set to true, the `default_nexthop6` configuration parameter 
     * will always be used as nexthop, regardless of the peering LAN. (for IBGP,
     * this will only work if ibgp_alter_nexthop is set to true.)
     * (default: false)
     */
    bool forced_default_nexthop6;

    /**
     * @brief The hold timer.
     * (default: 120)
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
     * (default: NULL)
     */
    Clock *clock;

    /**
     * @brief Allow numbers of local asn in as_path.
     * (default: 0)
     */
    int8_t allow_local_as;

    /**
     * @brief Weight of this BGP session.
     * 
     * Routes of session with higher weight will be prefered when makeing
     * routing decision. (RIB lookup)
     * 
     * (default: 0)
     */
    int32_t weight;

    /**
     * @brief Disable auto tick on message reception.
     * 
     * Set to true will disable auto FSM ticking when message received.
     * 
     * (default: false)
     */
    bool no_autotick;

    /**
     * @brief Do alter_nexthop for IBGP sessions.
     * 
     * If true, libbgp will alter IBGP nexthop attribute the same way as EBGP.
     * 
     * (default: false)
     */
    bool ibgp_alter_nexthop;
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