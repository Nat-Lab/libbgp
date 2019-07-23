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
BgpFilterRule6::BgpFilterRule6(BgpFilterType type, BgpFilterOP op, const uint8_t prefix[16], uint8_t mask) : prefix(prefix, mask) {
    this->type = type;
    this->op = op;
}

/**
 * @brief Construct a new Bgp Filter Rule:: Bgp Filter Rule object
 * 
 * @param type Type of the rule
 * @param op Action to take
 * @param prefix Prefix to match in dotted string notation.
 * @param mask Netmask of the prefix in CIDR notation.
 */
BgpFilterRule6::BgpFilterRule6(BgpFilterType type, BgpFilterOP op, const char *prefix, uint8_t mask) : prefix(prefix, mask) {
    this->type = type;
    this->op = op;
}

/**
 * @brief Construct a new Bgp Filter Rule:: Bgp Filter Rule object
 * 
 * @param type Type of the rule
 * @param op Action to take
 * @param p Prefix to match.
 */
BgpFilterRule6::BgpFilterRule6(BgpFilterType type, BgpFilterOP op, const Route6 &p) : prefix(p) {
    this->type = type;
    this->op = op;
}

/**
 * @brief Apply the rule on a prefix.
 * 
 * @param prefix The prefix to run this rule on.
 * @return BgpFilterOP The operation to take.
 */
BgpFilterOP BgpFilterRule6::apply(const Route6 &prefix) const {
    if (type == STRICT && this->prefix == prefix) return op;
    if (type == LOOSE && this->prefix.includes(prefix)) return op;
    return NOP;
}

/**
 * @brief Apply the rule on a prefix.
 * 
 * @param prefix The prefix to run this rule.
 * @param mask Netmask of the prefix in CIDR notation.
 * @return BgpFilterOP The operation to take.
 */
BgpFilterOP BgpFilterRule6::apply(const uint8_t prefix[16], uint8_t mask) const {
    if (mask > 128) return NOP;
    Route6 prefix_obj(prefix, mask);
    return apply(prefix_obj);
}

/**
 * @brief Construct a new IPv6 BGP route filter rule list.
 * 
 * The default operation will be set to ACCEPT.
 * 
 */
BgpFilterRules6::BgpFilterRules6() {
    default_op = ACCEPT;
}

/**
 * @brief Construct a new Bgp Filter Rules:: Bgp Filter Rules object
 * 
 * @param default_op The default operation to take if non of the rules matched
 * the prefix.
 */
BgpFilterRules6::BgpFilterRules6(BgpFilterOP default_op) {
    this->default_op = default_op;
}

/**
 * @brief Add a rule to the rules set.
 * 
 * @param rule The rule to add.
 */
void BgpFilterRules6::append(const BgpFilterRule6 &rule) {
    rules.push_back(rule);
}

/**
 * @brief Apply rules set on a prefix.
 * 
 * @param prefix The prefix to run rules set on.
 * @return BgpFilterOP The operation to take.
 */
BgpFilterOP BgpFilterRules6::apply(const Route6 &prefix) const {
    BgpFilterOP op = default_op;
    for (const BgpFilterRule6 &rule : rules) {
        BgpFilterOP this_op = rule.apply(prefix);
        if (this_op != NOP) op = this_op;
    }
    return op;
}

/**
 * @brief Apply rules set on a prefix.
 * 
 * @param prefix The prefix to run rules set on.
 * @param mask Netmask of the prefix in CIDR notation.
 * @return BgpFilterOP The operation to take.
 */
BgpFilterOP BgpFilterRules6::apply(const uint8_t prefix[16], uint32_t mask) const {
    BgpFilterOP op = default_op;
    for (const BgpFilterRule6 &rule : rules) {
        BgpFilterOP this_op = rule.apply(prefix, mask);
        if (this_op != NOP) op = this_op;
    }
    return op;
}

}