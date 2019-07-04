#ifndef BGP_FILTER_H_
#define BGP_FILTER_H_
#include <stdint.h>
#include <vector>
#include "route.h"

namespace bgpfsm {

enum BgpFilterOP {
    NOP,
    ACCEPT,
    REJECT
};

enum BgpFilterType {
    STRICT,
    LOOSE
};

class BgpFilterRule {
public:
    BgpFilterRule(BgpFilterType type, BgpFilterOP op, uint32_t prefix, uint8_t mask);
    BgpFilterRule(BgpFilterType type, BgpFilterOP op, const char *prefix, uint8_t mask);
    BgpFilterRule(BgpFilterType type, BgpFilterOP op, const Route &prefix);
    BgpFilterOP apply(uint32_t prefix, uint8_t mask) const;
    BgpFilterOP apply(const Route &prefix) const;

private:
    BgpFilterType type;
    BgpFilterOP op;
    Route prefix;
};

class BgpFilterRules {
public:
    BgpFilterRules();
    BgpFilterRules(BgpFilterOP default_op);

    void append(const BgpFilterRule &rule);
    BgpFilterOP apply(uint32_t prefix, uint32_t mask) const;
    BgpFilterOP apply(const Route &prefix) const;

private:
    BgpFilterOP default_op;
    std::vector<BgpFilterRule> rules;
};

}

#endif // BGP_FILTER_H_