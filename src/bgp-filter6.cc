/**
 * @file bgp-filter6.cc
 * @author Nato Morichika <nat@nat.moe>
 * @brief The IPv6 Route filtering engine.
 * @version 0.2
 * @date 2019-07-21
 * 
 * @copyright Copyright (c) 2019
 * 
 */
#include "bgp-filter6.h"

namespace libbgp {

/**
 * @brief Construct a new IPv6 BGP route filter rule
 * 
 * @param type Type of the rule
 * @param op Action to take
 * @param prefix Prefix to match in network byte order.
 * @param mask Netmask of the prefix in CIDR notation.
 */
BgpFilterRule6::BgpFilterRule6(BgpFilterType type, BgpFilterOP op, const uint8_t prefix[16], uint8_t mask) : 
    BgpFilterRule(type, op, Prefix6(prefix, mask)) {}

/**
 * @brief Construct a new Bgp Filter Rule:: Bgp Filter Rule object
 * 
 * @param type Type of the rule
 * @param op Action to take
 * @param prefix Prefix to match in dotted string notation.
 * @param mask Netmask of the prefix in CIDR notation.
 */
BgpFilterRule6::BgpFilterRule6(BgpFilterType type, BgpFilterOP op, const char *prefix, uint8_t mask) : 
    BgpFilterRule(type, op, Prefix6(prefix, mask)) {}

/**
 * @brief Construct a new Bgp Filter Rule:: Bgp Filter Rule object
 * 
 * @param type Type of the rule
 * @param op Action to take
 * @param p Prefix to match.
 */
BgpFilterRule6::BgpFilterRule6(BgpFilterType type, BgpFilterOP op, const Prefix6 &p) : 
    BgpFilterRule(type, op, Prefix6(p)) {}

/**
 * @brief Apply the rule on a prefix.
 * 
 * @param prefix The prefix to run this rule.
 * @param mask Netmask of the prefix in CIDR notation.
 * @return BgpFilterOP The operation to take.
 */
BgpFilterOP BgpFilterRule6::apply(const uint8_t prefix[16], uint8_t mask) const {
    if (mask > 128) return NOP;
    return BgpFilterRule::apply(Prefix6 (prefix, mask));
}

/**
 * @brief Construct a new IPv6 BGP route filter rule list.
 * 
 * The default operation will be set to ACCEPT.
 * 
 */
BgpFilterRules6::BgpFilterRules6() {}

/**
 * @brief Construct a new Bgp Filter Rules:: Bgp Filter Rules object
 * 
 * @param default_op The default operation to take if non of the rules matched
 * the prefix.
 */
BgpFilterRules6::BgpFilterRules6(BgpFilterOP default_op) : BgpFilterRules(default_op) {}

/**
 * @brief Apply rules set on a prefix.
 * 
 * @param prefix The prefix to run rules set on.
 * @param mask Netmask of the prefix in CIDR notation.
 * @return BgpFilterOP The operation to take.
 */
BgpFilterOP BgpFilterRules6::apply(const uint8_t prefix[16], uint32_t mask) const {
    return BgpFilterRules::apply(Prefix6 (prefix, mask));
}

}