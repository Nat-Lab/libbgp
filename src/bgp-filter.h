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

}

#endif // BGP_FILTER_H_