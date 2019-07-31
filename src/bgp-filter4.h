/**
 * @file bgp-filter4.h
 * @author Nato Morichika <nat@nat.moe>
 * @brief The IPv4 Route filtering engine.
 * @version 0.2
 * @date 2019-07-21
 * 
 * @copyright Copyright (c) 2019
 * 
 */
#ifndef BGP_FILTER4_H_
#define BGP_FILTER4_H_
#include <stdint.h>
#include <vector>
#include "bgp-filter.h"
#include "prefix4.h"

namespace libbgp {

/**
 * @brief The BgpFilterRule4 class.
 * 
 * A BGP IPv4 route filtering rule.
 * 
 */
class BgpFilterRule4 : public BgpFilterRule<Prefix4> {
public:
    BgpFilterRule4(BgpFilterType type, BgpFilterOP op, uint32_t prefix, uint8_t mask);
    BgpFilterRule4(BgpFilterType type, BgpFilterOP op, const char *prefix, uint8_t mask);
    BgpFilterRule4(BgpFilterType type, BgpFilterOP op, const Prefix4 &prefix);
    BgpFilterOP apply(uint32_t prefix, uint8_t mask) const;
};

/**
 * @brief The BgpFilterRules4 class.
 * 
 * A list of BGP IPv4 route filtering rules.
 * 
 */
class BgpFilterRules4 : public BgpFilterRules<BgpFilterRule4, Prefix4> {
public:
    BgpFilterRules4();
    BgpFilterRules4(BgpFilterOP default_op);
    BgpFilterOP apply(uint32_t prefix, uint32_t mask) const;
};

/**
 * @example route-filter.cc
 * Example of using ingress/egress route filtering feature of BgpFsm.
 * 
 */

}

#endif // BGP_FILTER4_H_