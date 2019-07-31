/**
 * @file bgp-filter.h
 * @author Nato Morichika <nat@nat.moe>
 * @brief Route filtering engine common resources.
 * @version 0.2
 * @date 2019-07-21
 * 
 * @copyright Copyright (c) 2019
 * 
 */
#ifndef BGP_FILTER_H_
#define BGP_FILTER_H_

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
 * @brief The Base of BgpFilterRule class.
 * 
 * @tparam T type of prefix
 */
template <typename T> class BgpFilterRule {
public:

    /**
     * @brief Construct a new BGP route filter rule.
     * 
     * @param type Type of the rule
     * @param op Action to take
     * @param prefix Prefix to match.
     */
    BgpFilterRule(BgpFilterType type, BgpFilterOP op, const T &prefix) {
        this->type = type;
        this->op = op;
        this->prefix = prefix;
    }

    /**
     * @brief Apply the rule on a prefix.
     * 
     * @param prefix The prefix to run this rule on.
     * @return BgpFilterOP The operation to take.
     */
    BgpFilterOP apply(const T &prefix) const {
        if (type == STRICT && this->prefix == prefix) return op;
        if (type == LOOSE && this->prefix.includes(prefix)) return op;
        return NOP;
    }

private:
    BgpFilterType type;
    BgpFilterOP op;
    T prefix;
};

/**
 * @brief The BgpFilterRules base class.
 * 
 * @tparam T type of filtering rule.
 */

/**
 * @brief The BgpFilterRules base class.
 * 
 * @tparam TFilter type of filtering rule.
 * @tparam TPrefix type of prefix.
 */
template <typename TFilter, typename TPrefix> class BgpFilterRules {
public:
    /**
     * @brief Construct a new BGP route filter rule list.
     * 
     * The default operation will be set to ACCEPT.
     * 
     */
    BgpFilterRules() {
        default_op = ACCEPT;
    }

    /**
     * @brief Construct a new BGP route filter rule list.
     * 
     * @param default_op The default operation to take if non of the rules 
     * matched the prefix.
     */
    BgpFilterRules(BgpFilterOP default_op) {
        this->default_op = default_op;
    }

    /**
     * @brief Add a rule to the rules set.
     * 
     * @param rule The rule to add.
     */
    void append(const TFilter &rule) {
        rules.push_back(rule);
    }

    /**
     * @brief Apply rules set on a prefix.
     * 
     * @param prefix The prefix to run rules set on.
     * @return BgpFilterOP The operation to take.
     */
    BgpFilterOP apply(const TPrefix &prefix) const {
        if (rules.size() == 0) return default_op;
    
        auto rule = rules.end();

        do {
            rule--;
            BgpFilterOP this_op = rule->BgpFilterRule::apply(prefix);
            if (this_op != NOP) return this_op;
        } while (rule != rules.begin());

        return default_op;
    }

private:
    BgpFilterOP default_op;
    std::vector<TFilter> rules;
};

}

#endif // BGP_FILTER_H_