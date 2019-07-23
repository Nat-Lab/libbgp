/**
 * @file bgp-filter6.h
 * @author Nato Morichika <nat@nat.moe>
 * @brief The IPv6 Route filtering engine.
 * @version 0.2
 * @date 2019-07-21
 * 
 * @copyright Copyright (c) 2019
 * 
 */
#ifndef BGP_FILTER6_H_
#define BGP_FILTER6_H_
#include <stdint.h>
#include <vector>
#include "bgp-filter.h"
#include "prefix6.h"

namespace libbgp {

/**
 * @brief The BgpFilterRule6 class.
 * 
 * A BGP IPv6 route filtering rule.
 * 
 */
class BgpFilterRule6 {
public:
    BgpFilterRule6(BgpFilterType type, BgpFilterOP op, const uint8_t prefix[16], uint8_t mask);
    BgpFilterRule6(BgpFilterType type, BgpFilterOP op, const char *prefix, uint8_t mask);
    BgpFilterRule6(BgpFilterType type, BgpFilterOP op, const Prefix6 &prefix);
    BgpFilterOP apply(const uint8_t prefix[16], uint8_t mask) const;
    BgpFilterOP apply(const Prefix6 &prefix) const;

private:
    BgpFilterType type;
    BgpFilterOP op;
    Prefix6 prefix;
};

/**
 * @brief The BgpFilterRules6 class.
 * 
 * A list of BGP IPv6 route filtering rules.
 * 
 */
class BgpFilterRules6 {
public:
    BgpFilterRules6();
    BgpFilterRules6(BgpFilterOP default_op);

    void append(const BgpFilterRule6 &rule);
    BgpFilterOP apply(const uint8_t prefix[16], uint32_t mask) const;
    BgpFilterOP apply(const Prefix6 &prefix) const;

private:
    BgpFilterOP default_op;
    std::vector<BgpFilterRule6> rules;
};

}

#endif // BGP_FILTER6_H_