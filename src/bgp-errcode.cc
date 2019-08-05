/**
 * @file bgp-errcode.cc
 * @author Nato Morichika <nat@nat.moe>
 * @brief BGP error codes.
 * @version 0.1
 * @date 2019-07-04
 * 
 * @copyright Copyright (c) 2019
 * 
 */
#include "bgp-errcode.h"

namespace libbgp {

/**
 * @brief Error strings for BGP error codes.
 * 
 */
const char *bgp_error_code_str[7] = {
    "Unspecific",
    "Message Header Error",
    "OPEN Message Error",
    "UPDATE Message Error",
    "Hold Timer Expired",
    "Finite State Machine Error",
    "Cease"
};

/**
 * @brief Error strings for BGP header error subcodes.
 * 
 */
const char* bgp_header_error_subcode_str[4] = {
    "Unspecific",
    "Connection Not Synchronized",
    "Bad Message Length",
    "Bad Message Type"
};

/**
 * @brief Error strings for BGP open message error subcodes.
 * 
 */
const char *bgp_open_error_subcode_str[8] = {
    "Unspecific",
    "Unsupported Version Number",
    "Bad Peer AS",
    "Bad Peer BGP ID",
    "Unsupported Optional Parameter",
    "Authentication Failure",
    "Unacceptable Hold Time",
    "Unsupported Capability"
};

/**
 * @brief Error strings for BGP update message error subcodes.
 * 
 */
const char *bgp_update_error_str[12] = {
    "Unspecific",
    "Malformed Attribute List",
    "Unrecognized Well-known Attribute",
    "Missing Well-known Attribute",
    "Attribute Flags Error",
    "Attribute Length Error",
    "Invalid ORIGIN Attribute",
    "AS Routing Loop",
    "Invalid NEXT_HOP Attribute",
    "Optional Attribute Error",
    "Invalid Network Field",
    "Malformed AS_PATH"
};

/**
 * @brief Error strings for BGP FSM error subcodes.
 * 
 */
const char *bgp_fsm_error_str[4] = {
    "Unspecific",
    "Receive Unexpected Message in OpenSent State",
    "Receive Unexpected Message in OpenConfirm State",
    "Receive Unexpected Message in Established State"
};

/**
 * @brief Error strings for BGP cease error subcodes.
 * 
 */
const char *bgp_cease_error_str[9] = {
    "Unspecific",
    "Maximum Number of Prefixes Reached",
    "Administrative Shutdown",
    "Peer De-configured",
    "Administrative Reset",
    "Connection Rejected",
    "Other Configuration Change",
    "Connection Collision Resolution",
    "Out of Resources"
};

}