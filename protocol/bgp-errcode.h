#ifndef BGP_ERRCODE_H_
#define BGP_ERRCODE_H_

namespace bgpfsm {

const char* bgp_error_code_str[] = {
    "Unspecific"
    "Message Header Error",
    "OPEN Message Error",
    "UPDATE Message Error",
    "Hold Timer Expired",
    "Finite State Machine Error",
    "Cease"
};

const char* bgp_header_error_subcode_str[] = {
    "Unspecific",
    "Connection Not Synchronized",
    "Bad Message Length",
    "Bad Message Type"
};

const char *bgp_open_error_subcode_str[] = {
    "Unspecific",
    "Unsupported Version Number",
    "Bad Peer AS",
    "Bad Peer BGP ID",
    "Unsupported Optional Parameter",
    "Authentication Failure",
    "Unacceptable Hold Time"
};

const char *bgp_update_error_str[] = {
    "Unspecific",
    "Malformed Attribute List",
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

const char *bgp_cease_error_str[] = {
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

enum BgpErrorCode {
    E_HEADER = 1, // Message Header Error
    E_OPEN = 2, // OPEN Message Error 
    E_UPDATE = 3, // UPDATE Message Error
    E_HOLD = 4, // Hold Timer Expired
    E_FSM = 5, // Finite State Machine Error
    E_CEASE = 6 // Cease
};

enum BgpHeaderErrorSubcode {
    E_SYNC = 1, // Connection Not Synchronized
    E_LENGTH = 2, // Bad Message Length
    E_TYPE = 3 // Bad Message Type
};

enum BgpOpenErrorSubcode {
    E_VERSION = 1, // Unsupported Version Number
    E_PEER_AS = 2, // Bad Peer AS
    E_BGP_ID = 3, // Bad Peer BGP ID
    E_OPT_PARAM = 4, // Unsupported Optional Parameter
    E_AUTH_FAILED = 5, // Authentication Failure (Deprecated)
    E_HOLD_TIME = 6 // Unacceptable Hold Time
};

enum BgpUpdateErrorSubcode {
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

enum BgpCeaseErrorSubcode {
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