#ifndef BGP_ERRCODE_H_
#define BGP_ERRCODE_H_

namespace bgpfsm {

enum BgpErrorCode {
    E_MSG_HDR = 1, // Message Header Error
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

}

#endif // BGP_ERRCODE_H_