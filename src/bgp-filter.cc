#include "bgp-filter.h"

namespace bgpfsm {

BgpFilterRule::BgpFilterRule(BgpFilterType type, BgpFilterOP op, uint32_t prefix, uint8_t mask) : prefix(prefix, mask) {
    this->type = type;
    this->op = op;
}

BgpFilterRule::BgpFilterRule(BgpFilterType type, BgpFilterOP op, const char *prefix, uint8_t mask) : prefix(prefix, mask) {
    this->type = type;
    this->op = op;
}

BgpFilterRule::BgpFilterRule(BgpFilterType type, BgpFilterOP op, const Route &p) : prefix(p) {
    this->type = type;
    this->op = op;
}

BgpFilterOP BgpFilterRule::apply(const Route &prefix) const {
    if (type == STRICT && this->prefix == prefix) return op;
    if (type == LOOSE && this->prefix.includes(prefix)) return op;
    return NOP;
}

BgpFilterOP BgpFilterRule::apply(uint32_t prefix, uint8_t mask) const {
    if (mask > 32) return NOP;
    if (type == STRICT && this->prefix.getPrefix() == prefix && this->prefix.getLength() == mask) return op;
    if (type == LOOSE && this->prefix.includes(prefix, mask)) return op;
    return NOP;
}

BgpFilterRules::BgpFilterRules() {
    default_op = ACCEPT;
}

BgpFilterRules::BgpFilterRules(BgpFilterOP default_op) {
    this->default_op = default_op;
}

void BgpFilterRules::append(const BgpFilterRule &rule) {
    rules.push_back(rule);
}

BgpFilterOP BgpFilterRules::apply(const Route &prefix) const {
    BgpFilterOP op = default_op;
    for (const BgpFilterRule &rule : rules) {
        BgpFilterOP this_op = rule.apply(prefix);
        if (this_op != NOP) op = this_op;
    }
    return op;
}

BgpFilterOP BgpFilterRules::apply(uint32_t prefix, uint32_t mask) const {
    BgpFilterOP op = default_op;
    for (const BgpFilterRule &rule : rules) {
        BgpFilterOP this_op = rule.apply(prefix, mask);
        if (this_op != NOP) op = this_op;
    }
    return op;
}

}