/**
 * @file bgp-filter.h
 * @author Nato Morichika <nat@nat.moe>
 * @brief The Route filtering engine.
 * @version 0.1
 * @date 2019-07-06
 * 
 * @copyright Copyright (c) 2019
 * 
 */
#ifndef BGP_FILTER_H_
#define BGP_FILTER_H_
#include <stdint.h>
#include <vector>
#include "route.h"

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
    BgpFilterRule(BgpFilterType type, BgpFilterOP op, const Route &prefix);
    BgpFilterOP apply(uint32_t prefix, uint8_t mask) const;
    BgpFilterOP apply(const Route &prefix) const;

private:
    BgpFilterType type;
    BgpFilterOP op;
    Route prefix;
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
    BgpFilterOP apply(const Route &prefix) const;

private:
    BgpFilterOP default_op;
    std::vector<BgpFilterRule> rules;
};

}

#endif // BGP_FILTER_H_