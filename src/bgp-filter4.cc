/**
 * @file bgp-filter4.cc
 * @author Nato Morichika <nat@nat.moe>
 * @brief The IPv4 Route filtering engine.
 * @version 0.2
 * @date 2019-07-21
 * 
 * @copyright Copyright (c) 2019
 * 
 */
#include "bgp-filter4.h"

namespace libbgp {

/**
 * @brief Construct a new IPv4 BGP route filter rule
 * 
 * @param type Type of the rule
 * @param op Action to take
 * @param prefix Prefix to match in network byte order.
 * @param mask Netmask of the prefix in CIDR notation.
 */
BgpFilterRule4::BgpFilterRule4(BgpFilterType type, BgpFilterOP op, uint32_t prefix, uint8_t mask) : prefix(prefix, mask) {
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
BgpFilterRule4::BgpFilterRule4(BgpFilterType type, BgpFilterOP op, const char *prefix, uint8_t mask) : prefix(prefix, mask) {
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
BgpFilterRule4::BgpFilterRule4(BgpFilterType type, BgpFilterOP op, const Prefix4 &p) : prefix(p) {
    this->type = type;
    this->op = op;
}

/**
 * @brief Apply the rule on a prefix.
 * 
 * @param prefix The prefix to run this rule on.
 * @return BgpFilterOP The operation to take.
 */
BgpFilterOP BgpFilterRule4::apply(const Prefix4 &prefix) const {
    if (type == STRICT && this->prefix == prefix) return op;
    if (type == LOOSE && this->prefix.includes(prefix)) return op;
    return NOP;
}

/**
 * @brief Apply the rule on a prefix.
 * 
 * @param prefix The prefix to run this rule on in network byte order.
 * @param mask Netmask of the prefix in CIDR notation.
 * @return BgpFilterOP The operation to take.
 */
BgpFilterOP BgpFilterRule4::apply(uint32_t prefix, uint8_t mask) const {
    if (mask > 32) return NOP;
    if (type == STRICT && this->prefix.getPrefix() == prefix && this->prefix.getLength() == mask) return op;
    if (type == LOOSE && this->prefix.includes(prefix, mask)) return op;
    return NOP;
}

/**
 * @brief Construct a new IPv4 BGP route filter rule list.
 * 
 * The default operation will be set to ACCEPT.
 * 
 */
BgpFilterRules4::BgpFilterRules4() {
    default_op = ACCEPT;
}

/**
 * @brief Construct a new Bgp Filter Rules:: Bgp Filter Rules object
 * 
 * @param default_op The default operation to take if non of the rules matched
 * the prefix.
 */
BgpFilterRules4::BgpFilterRules4(BgpFilterOP default_op) {
    this->default_op = default_op;
}

/**
 * @brief Add a rule to the rules set.
 * 
 * @param rule The rule to add.
 */
void BgpFilterRules4::append(const BgpFilterRule4 &rule) {
    rules.push_back(rule);
}

/**
 * @brief Apply rules set on a prefix.
 * 
 * @param prefix The prefix to run rules set on.
 * @return BgpFilterOP The operation to take.
 */
BgpFilterOP BgpFilterRules4::apply(const Prefix4 &prefix) const {
    if (rules.size() == 0) return default_op;
    
    std::vector<BgpFilterRule4>::const_iterator rule = rules.end();

    do {
        rule--;
        BgpFilterOP this_op = rule->apply(prefix);
        if (this_op != NOP) return this_op;
    } while (rule != rules.begin());

    return default_op;
}

/**
 * @brief Apply rules set on a prefix.
 * 
 * @param prefix The prefix to run rules set on in network byte order.
 * @param mask Netmask of the prefix in CIDR notation.
 * @return BgpFilterOP The operation to take.
 */
BgpFilterOP BgpFilterRules4::apply(uint32_t prefix, uint32_t mask) const {
    if (rules.size() == 0) return default_op;
    
    std::vector<BgpFilterRule4>::const_iterator rule = rules.end();

    do {
        rule--;
        BgpFilterOP this_op = rule->apply(prefix, mask);
        if (this_op != NOP) return this_op;
    } while (rule != rules.begin());

    return default_op;
}

}