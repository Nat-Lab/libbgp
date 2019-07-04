#include "bgp-errcode.h"

namespace libbgp {

const char *bgp_error_code_str[] = {
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
    "Unacceptable Hold Time",
    "Unsupported Capability"
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

const char *bgp_fsm_error_str[] = {
    "Unspecific",
    "Receive Unexpected Message in OpenSent State",
    "Receive Unexpected Message in OpenConfirm State",
    "Receive Unexpected Message in Established State"
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

}