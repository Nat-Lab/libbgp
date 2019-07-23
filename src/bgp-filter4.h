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
#include "route4.h"

namespace libbgp {

/**
 * @brief The filter operation.
 * 
 */
enum BgpFilterOP {
    NOP, /*!< No Operation (Prefix does not match) */
    ACCEPT, /*!< Accept */
    REJECT /*!< Reject */
};

/**
 * @brief The types of filter.
 * 
 */
enum BgpFilterType {
    STRICT, /*!< Only match if the target subnet is an exact match */
    LOOSE /*!< Match if the taeget subnet is a sub-prefix */
};

/**
 * @brief The BgpFilterRule class.
 * 
 * A BGP route filtering rule.
 * 
 */
class BgpFilterRule {
public:
    BgpFilterRule(BgpFilterType type, BgpFilterOP op, uint32_t prefix, uint8_t mask);
    BgpFilterRule(BgpFilterType type, BgpFilterOP op, const char *prefix, uint8_t mask);
    BgpFilterRule(BgpFilterType type, BgpFilterOP op, const Route4 &prefix);
    BgpFilterOP apply(uint32_t prefix, uint8_t mask) const;
    BgpFilterOP apply(const Route4 &prefix) const;

private:
    BgpFilterType type;
    BgpFilterOP op;
    Route4 prefix;
};

/**
 * @brief The BgpFilterRules class.
 * 
 * A list of BGP route filtering rules.
 * 
 */
class BgpFilterRules {
public:
    BgpFilterRules();
    BgpFilterRules(BgpFilterOP default_op);

    void append(const BgpFilterRule &rule);
    BgpFilterOP apply(uint32_t prefix, uint32_t mask) const;
    BgpFilterOP apply(const Route4 &prefix) const;

private:
    BgpFilterOP default_op;
    std::vector<BgpFilterRule> rules;
};

/**
 * @example route-filter.cc
 * Example of using ingress/egress route filtering feature of BgpFsm.
 * 
 */

}

#endif // BGP_FILTER4_H_