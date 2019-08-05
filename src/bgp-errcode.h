/**
 * @file bgp-errcode.h
 * @author Nato Morichika <nat@nat.moe>
 * @brief BGP error codes.
 * @version 0.1
 * @date 2019-07-04
 * 
 * @copyright Copyright (c) 2019
 * 
 */
#ifndef BGP_ERRCODE_H_
#define BGP_ERRCODE_H_

namespace libbgp {

extern const char *bgp_error_code_str[7];
extern const char* bgp_header_error_subcode_str[4];
extern const char *bgp_open_error_subcode_str[8];
extern const char *bgp_update_error_str[12];
extern const char *bgp_fsm_error_str[4];
extern const char *bgp_cease_error_str[9];

/**
 * @brief BGP Error codes
 * 
 */
enum BgpErrorCode {
    E_UNSPEC = 0,
    E_HEADER = 1, // Message Header Error
    E_OPEN = 2, // OPEN Message Error 
    E_UPDATE = 3, // UPDATE Message Error
    E_HOLD = 4, // Hold Timer Expired
    E_FSM = 5, // Finite State Machine Error
    E_CEASE = 6 // Cease
};

/**
 * @brief BGP header error subcodes.
 * 
 */
enum BgpHeaderErrorSubcode {
    E_UNSPEC_HEADER = 0,
    E_SYNC = 1, // Connection Not Synchronized
    E_LENGTH = 2, // Bad Message Length
    E_TYPE = 3 // Bad Message Type
};

/**
 * @brief BGP open message error subcodes.
 * 
 */
enum BgpOpenErrorSubcode {
    E_UNSPEC_OPEN = 0,
    E_VERSION = 1, // Unsupported Version Number
    E_PEER_AS = 2, // Bad Peer AS
    E_BGP_ID = 3, // Bad Peer BGP ID
    E_OPT_PARAM = 4, // Unsupported Optional Parameter
    E_AUTH_FAILED = 5, // Authentication Failure (Deprecated)
    E_HOLD_TIME = 6, // Unacceptable Hold Time
    E_CAPABILITY = 7 // Unsupported Capability
};

/**
 * @brief BGP update message error subcodes.
 * 
 */
enum BgpUpdateErrorSubcode {
    E_UNSPEC_UPDATE = 0,
    E_ATTR_LIST = 1, // Malformed Attribute List
    E_BAD_WELL_KNOWN = 2, // Unrecognized Well-known Attribute
    E_MISS_WELL_KNOWN = 3, // Missing Well-known Attribute
    E_ATTR_FLAG = 4, // Attribute Flags Error
    E_ATTR_LEN = 5, // Attribute Length Error
    E_ORIGIN = 6, // Invalid ORIGIN Attribute
    E_AS_LOOP = 7, // AS Routing Loop (Deprecated)
    E_NEXT_HOP = 8, // Invalid NEXT_HOP Attribute
    E_OPT_ATTR = 9, // Optional Attribute Error
    E_NETFIELD = 10, // Invalid Network Field
    E_AS_PATH = 11 // Malformed AS_PATH
};

/**
 * @brief BGP FSm error subcodes.
 * 
 */
enum BgpFsmErrorSubcode {
    E_UNSPEC_FSM = 0,
    E_OPEN_SENT = 1, // Receive Unexpected Message in OpenSent State
    E_OPEN_CONFIRM = 2, // Receive Unexpected Message in OpenConfirm State
    E_ESTABLISHED = 3 // Receive Unexpected Message in Established State
};

/**
 * @brief BGP cease error subcodes.
 * 
 */
enum BgpCeaseErrorSubcode {
    E_UNSPEC_CEASE = 0,
    E_MAX_PREFIX = 1, // Maximum Number of Prefixes Reached
    E_SHUTDOWN = 2, // Administrative Shutdown
    E_DECONF = 3, // Peer De-configured
    E_RESET = 4, // Administrative Reset
    E_REJECT = 5, // Connection Rejected
    E_CONFGCHANGE = 6, // Other Configuration Change
    E_COLLISION = 7, // Connection Collision Resolution
    E_RESOURCES = 8 // Out of Resources
};

}

#endif // BGP_ERRCODE_H_