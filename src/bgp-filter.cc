/**
 * @file bgp-filter.cc
 * @author Nato Morichika <nat@nat.moe>
 * @brief The BGP route filtering engine.
 * @version 0.1
 * @date 2019-07-31
 * 
 * @copyright Copyright (c) 2019
 * 
 */
#include <arpa/inet.h>
#include "bgp-filter.h"
#include "value-op.h"

namespace libbgp {

BgpFilterRule::~BgpFilterRule() {}

/**
 * @brief Construct a new IPv4 route filtering object
 * 
 * @param op Action to take if the prefix matched.
 * @param type Type of matching.
 * @param prefix Prefix to match.
 */
BgpFilterRuleRoute4::BgpFilterRuleRoute4(BgpFilterOP op, BgpFilterRuleRouteMatchType type, const Prefix4 &prefix) {
    this->op = op;
    this->match_type = type;
    this->prefix = prefix;
}

/**
 * @brief Construct a new IPv6 route filtering object
 * 
 * @param op Action to take if the prefix matched.
 * @param type Type of matching.
 * @param prefix Prefix to match.
 */
BgpFilterRuleRoute6::BgpFilterRuleRoute6(BgpFilterOP op, BgpFilterRuleRouteMatchType type, const Prefix6 &prefix) {
    this->op = op;
    this->match_type = type;
    this->prefix = prefix;
}

/**
 * @brief Construct a new AS_PATH filtering object
 * 
 * @param op Action to take if the asn matched.
 * @param type Type of matching.
 * @param asn ASN to match.
 */
BgpFilterRuleAsPath::BgpFilterRuleAsPath(BgpFilterOP op, BgpFilterRuleAsPathMatchType type, uint32_t asn) {
    this->op = op;
    this->match_type = type;
    this->asn = asn;
}

BgpFilterOP BgpFilterRuleAsPath::apply(__attribute__((unused)) const Prefix &prefix, const std::vector<std::shared_ptr<BgpPathAttrib>> &attribs) {
    for (const std::shared_ptr<BgpPathAttrib> &attr : attribs) {
        if (attr->type_code != AS_PATH) continue;
        const BgpPathAttribAsPath &as_path = dynamic_cast<const BgpPathAttribAsPath &>(*attr);

        for (const BgpAsPathSegment &as_seg : as_path.as_paths) {
            if (as_seg.type != AS_SEQUENCE) continue;

            if (match_type == M_FROM_ASN && asn == as_seg.value.back()) return op;
            if (match_type == M_NOT_FROM_ASN && asn != as_seg.value.back()) return op;

            for (uint32_t asn : as_seg.value) {
                if (asn == this->asn && match_type == M_HAS_ASN) return op;
            }

        }

        if (match_type == M_NOT_HAS_ASN) return op; 
    }

    return NOP;
}

/**
 * @brief Construct a new COMMUNITY filtering object
 * 
 * @param op Action to take if community matched.
 * @param type Type of matching.
 * @param asn ASN part of the community. 'AAAA' in (AAAA:BBBB)
 * @param keyword keyword part of the community. 'BBBB' in (AAAA:BBBB)
 */
BgpFilterRuleCommunity::BgpFilterRuleCommunity(BgpFilterOP op, BgpFilterRuleCommunityMatchType type, uint16_t asn, uint16_t keyword) {
    this->op = op;
    this->match_type = type;
    uint8_t *community_ptr = (uint8_t *) &community;
    putValue<uint16_t>(&community_ptr, htons(asn));
    putValue<uint16_t>(&community_ptr, htons(keyword));
}

/**
 * @brief  Construct a new COMMUNITY filtering object
 * 
 * @param op Action to take if community matched.
 * @param type Type of matching.
 * @param community Raw community value in host-byte order.
 */
BgpFilterRuleCommunity::BgpFilterRuleCommunity(BgpFilterOP op, BgpFilterRuleCommunityMatchType type, uint32_t community) {
    this->op = op;
    this->match_type = type;
    this->community = htonl(community);
}

BgpFilterOP BgpFilterRuleCommunity::apply(__attribute__((unused)) const Prefix &prefix, const std::vector<std::shared_ptr<BgpPathAttrib>> &attribs) {
    for (const std::shared_ptr<BgpPathAttrib> &attr : attribs) {
        if (attr->type_code != COMMUNITY) continue;
        const BgpPathAttribCommunity &community = dynamic_cast<const BgpPathAttribCommunity &>(*attr);

        for (uint32_t community_val : community.communites) {
            if (match_type == M_HAS_COMMUNITY && community_val == this->community) return op;
        }

        if (match_type == M_NOT_HAS_COMMUNITY) return op;
    }

    return NOP;
}

/**
 * @brief Construct a new BgpFilterRules rules set.
 * 
 * Default action is ACCEPT.
 * 
 */
BgpFilterRules::BgpFilterRules() {
    default_op = ACCEPT;
}

/**
 * @brief Construct a new BgpFilterRules rules set.
 * 
 * @param default_op Default action.
 */
BgpFilterRules::BgpFilterRules(BgpFilterOP default_op) {
    this->default_op = default_op;
}

/**
 * @brief Apply the rules set on a route.
 * 
 * @param prefix Route prefix.
 * @param attribs Path attribues.
 * @return BgpFilterOP Action to take.
 */
BgpFilterOP BgpFilterRules::apply(const Prefix &prefix, const std::vector<std::shared_ptr<BgpPathAttrib>> &attribs) {
    if (rules.size() == 0) return default_op;
    
    auto rule = rules.end();

    do {
        rule--;
        BgpFilterOP this_op = (*rule)->apply(prefix, attribs);
        if (this_op != NOP) return this_op;
    } while (rule != rules.begin());

    return default_op;
}

}