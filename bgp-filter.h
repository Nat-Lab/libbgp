#ifndef BGP_FILTER_H_
#define BGP_FILTER_H_
#include <stdint.h>
#include <vector>

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
    BgpFilterRule();
    BgpFilterRule(BgpFilterType type, BgpFilterOP op, uint32_t prefix, uint32_t mask);

    BgpFilterType type;
    BgpFilterOP op;
    uint32_t prefix;
    uint32_t mask;

    BgpFilterOP apply(uint32_t prefix, uint32_t mask) const;
};

class BgpFilterRules {
    std::vector<BgpFilterRule> rules;

    void append(BgpFilterType type, BgpFilterOP op, uint32_t prefix, uint32_t mask);
    BgpFilterOP apply(uint32_t prefix, uint32_t mask) const;
};

}

#endif // BGP_FILTER_H_