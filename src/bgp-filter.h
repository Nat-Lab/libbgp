/**
 * @file bgp-filter.h
 * @author Nato Morichika <nat@nat.moe>
 * @brief Route filtering engine.
 * @version 0.3
 * @date 2019-07-31
 * 
 * @copyright Copyright (c) 2019
 * 
 */
#ifndef BGP_FILTER_H_
#define BGP_FILTER_H_
#include <vector>
#include <memory>
#include "bgp-afi.h"
#include "prefix4.h"
#include "prefix6.h"
#include "bgp-path-attrib.h"

namespace libbgp {

/**
 * @brief Type of filter rule.
 * 
 */
enum BgpFilterRuleType {
    F_ROUTE, /*!< Match a IP prefix */
    F_AS_PATH, /*!< Match AS_PATH */
    F_COMMUNITY /*!< Match COMMUNITY */
};

/**
 * @brief Matching type of IP prefix rule.
 * 
 */
enum BgpFilterRuleRouteMatchType {
    M_EQ, /*!< Match a IP prefix equals to rule prefix */
    M_NE, /*!< Match a IP prefix does not equals to rule prefix */
    M_GT, /*!< Match a IP prefix that has this prefix as sub-prefix (greater then) */
    M_LT, /*!< Match a sub-prefix of this IP prefix (less then) */
    M_GE, /*!< Match a IP prefix equals to, or greater then the rule prefix */
    M_LE /*!< Match a IP prefix equals to, or less then the rule prefix */
};

/**
 * @brief Matching type of AS_PATH rule.
 * 
 */
enum BgpFilterRuleAsPathMatchType {
    M_HAS_ASN, /*!< Match routes that has the ASN in AS_PATH */
    M_NOT_HAS_ASN, /*!< Match routes that does not have the ASN in AS_PATH */
    M_FROM_ASN, /*!< Match routes that are originating from the ASN */
    M_NOT_FROM_ASN /*!< Match routes that are not originating from the ASN */
};

/**
 * @brief Matching type of COMMUNITY rule.
 * 
 */
enum BgpFilterRuleCommunityMatchType {
    M_HAS_COMMUNITY, /*!< Match routes has the given community */
    M_NOT_HAS_COMMUNITY /*!< Match routes does not have the given community */
};

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
 * @brief The base of BgpFilterRule.
 * 
 */
class BgpFilterRule {
public:
    uint8_t filter_type;
    uint8_t match_type;
    BgpFilterOP op;

    /**
     * @brief Apply the rule on a route.
     * 
     * @param prefix Route prefix.
     * @param attribs Path attribues.
     * @return BgpFilterOP Action to take.
     */
    virtual BgpFilterOP apply(const Prefix &prefix, const std::vector<std::shared_ptr<BgpPathAttrib>> &attribs) = 0;
    virtual ~BgpFilterRule();
};

/**
 * @brief The prefix filtering rule.
 * 
 * @tparam T type of route prefix.
 */
template <typename T>
class BgpFilterRuleRoute : public BgpFilterRule {
public:
    /**
     * @brief Construct a new BgpFilterRuleRoute object
     * 
     */
    BgpFilterRuleRoute() {
        filter_type = F_ROUTE;
    }

    /**
     * @brief The route prefix.
     * 
     */
    T prefix;

    BgpFilterOP apply(const Prefix &prefix, __attribute__((unused)) const std::vector<std::shared_ptr<BgpPathAttrib>> &attribs) {
        if (prefix.afi != this->prefix.afi) return NOP;
        const T &prefix_t = dynamic_cast<const T &>(prefix);
        switch (match_type) {
            case M_EQ: return prefix_t == this->prefix ? op : NOP;
            case M_NE: return prefix_t != this->prefix ? op : NOP;
            case M_GT: return prefix_t != this->prefix && prefix_t.includes(this->prefix) ? op : NOP;
            case M_LT: return prefix_t != this->prefix && this->prefix.includes(prefix_t) ? op : NOP;
            case M_GE: return prefix_t.includes(this->prefix) ? op : NOP;
            case M_LE: return this->prefix.includes(prefix_t) ? op : NOP;
        }

        return NOP;
    }

    /**
     * @brief Destroy the BgpFilterRuleRoute object
     * 
     */
    virtual ~BgpFilterRuleRoute() {}
};
#ifdef SWIG
%template(Prefix4RouteFilterRule) BgpFilterRuleRoute<Prefix4>;
%template(Prefix6RouteFilterRule) BgpFilterRuleRoute<Prefix6>;
#endif

/**
 * @brief The IPv4 route filtering rule.
 * 
 */
class BgpFilterRuleRoute4 : public BgpFilterRuleRoute<Prefix4> {
public:
    BgpFilterRuleRoute4(BgpFilterOP op, BgpFilterRuleRouteMatchType type, const Prefix4 &prefix);
};

/**
 * @brief The IPv6 route filtering rule.
 * 
 */
class BgpFilterRuleRoute6 : public BgpFilterRuleRoute<Prefix6> {
public:
    BgpFilterRuleRoute6(BgpFilterOP op, BgpFilterRuleRouteMatchType type, const Prefix6 &prefix);
};

/**
 * @brief The AS_PATH filtering rule
 * 
 */
class BgpFilterRuleAsPath : public BgpFilterRule {
public:
    BgpFilterRuleAsPath(BgpFilterOP op, BgpFilterRuleAsPathMatchType type, uint32_t asn);

    /**
     * @brief The ASN of this entry.
     * 
     */
    uint32_t asn;

    BgpFilterOP apply(const Prefix &prefix, const std::vector<std::shared_ptr<BgpPathAttrib>> &attribs);
};

/**
 * @brief BGP well-known communities
 * 
 */
enum BgpWellKnownCommunity {
    NO_EXPORT = 0xFFFFFF01,
    NO_ADVERTISE = 0xFFFFFF02,
    NO_EXPORT_SUBCONFED = 0xFFFFFF03
};

/**
 * @brief The COMMUNITY filtering rule 
 * 
 */
class BgpFilterRuleCommunity : public BgpFilterRule {
public:
    BgpFilterRuleCommunity(BgpFilterOP op, BgpFilterRuleCommunityMatchType type, uint16_t asn, uint16_t keyword);
    BgpFilterRuleCommunity(BgpFilterOP op, BgpFilterRuleCommunityMatchType type, uint32_t community);

    /**
     * @brief community for this entry.
     * 
     */
    uint32_t community;

    BgpFilterOP apply(const Prefix &prefix, const std::vector<std::shared_ptr<BgpPathAttrib>> &attribs);
};

/**
 * @brief The BGP filtering rules set.
 * 
 */
class BgpFilterRules {
public:

    BgpFilterRules();
    BgpFilterRules(BgpFilterOP default_op);

    /**
     * @brief Append a rule to the rule set.
     * 
     * @tparam T Type of this rule.
     * @param rule The rule to append.
     */
    template <typename T>
    void append(const BgpFilterRule &rule) {
        const T &rule_typed = dynamic_cast<const T&> (rule);
        rules.push_back(std::shared_ptr<BgpFilterRule>(new T(rule_typed)));
    }
#ifdef SWIG
%template(appendAsPathRule) append<BgpFilterRuleAsPath>;
%template(appendCommunityRule) append<BgpFilterRuleCommunity>;
%template(appendRoute4Rule) append<BgpFilterRuleRoute4>;
%template(appendRoute6Rule) append<BgpFilterRuleRoute6>;
#endif

    BgpFilterOP apply(const Prefix &prefix, const std::vector<std::shared_ptr<BgpPathAttrib>> &attribs);
private:
    std::vector<std::shared_ptr<BgpFilterRule>> rules;
    BgpFilterOP default_op;
};

}

#endif // BGP_FILTER_H_