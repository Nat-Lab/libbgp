#include "bgp-open-message.h"
#include "bgp-errcode.h"
#include "bgp-error.h"
#include "value-op.h"
#include <stdlib.h>
#include <assert.h>
#include <arpa/inet.h>

namespace bgpfsm {

BgpOpenMessage::BgpOpenMessage() {
    this->version = 4;
    err_len = 0;
}

BgpOpenMessage::~BgpOpenMessage() {
    if (err_len > 0) free(err_data);
}

BgpOpenMessage::BgpOpenMessage(uint32_t my_asn, uint16_t hold_time, uint32_t bgp_id) : BgpOpenMessage() {
    this->my_asn = my_asn;
    this->hold_time = hold_time;
    this->bgp_id = bgp_id;
}

ssize_t BgpOpenMessage::parse(const uint8_t *from, size_t msg_sz) {
    if (msg_sz < 10) {
        err_data = (uint8_t *) malloc(sizeof(uint8_t));
        *err_data = msg_sz + 19;
        err_len = sizeof(uint8_t);
        err_code = E_HEADER;
        err_subcode = E_LENGTH;
        _bgp_error("BgpOpenMessage::parse: invalid open message size: %d.\n", msg_sz);
        return -1;
    }

    const uint8_t *buffer = from;

    version = getValue<uint8_t> (&buffer);
    my_asn = ntohs(getValue<uint16_t> (&buffer));
    hold_time = ntohs(getValue<uint16_t> (&buffer));
    bgp_id = ntohl(getValue<uint32_t> (&buffer));

    // TODO: bgp-id verify

    uint8_t opt_params_len = getValue<uint8_t> (&buffer);

    // size of rest of message != length of opt_param or invalid length
    if (opt_params_len != msg_sz - 10 || opt_params_len < 2) {
        err_code = E_OPEN;
        err_subcode = BgpOpenErrorSubcode::E_UNSPEC;
        if (opt_params_len + 10 != msg_sz) 
            _bgp_error("BgpOpenMessage::parse: size of rest of message (%d) != length of opt_param (%d).\n", msg_sz - 10, opt_params_len);
        if (opt_params_len < 2)
            _bgp_error("BgpOpenMessage::parse: opt params size < 2: %d.\n", opt_params_len);
        return -1;
    }

    uint8_t parsed_opt_params_len = 0;

    while (parsed_opt_params_len < opt_params_len) {
        uint8_t param_type = getValue<uint8_t> (&buffer);
        uint8_t param_length = getValue<uint8_t> (&buffer);

        // opt param size exceed opt_params_len
        if (parsed_opt_params_len + param_length > opt_params_len) {
            err_code = E_OPEN;
            err_subcode = BgpOpenErrorSubcode::E_UNSPEC;
            _bgp_error("BgpOpenMessage::parse: opt param size exceed opt_params_len.\n");
            return -1;
        }

        // not capability?
        if (param_type != 2) {
            err_code = E_OPEN;
            err_subcode = E_OPT_PARAM;
            _bgp_error("BgpOpenMessage::parse: unknow opt param type: %d.\n", param_type);
            return -1;
        }

        // invalid capability field?
        if (param_length < 2) {
            err_code = E_OPEN;
            err_subcode = BgpOpenErrorSubcode::E_UNSPEC;
            _bgp_error("BgpOpenMessage::parse: invalid capability opt param length: %d.\n", param_length);
            return -1;
        }

        uint8_t parsed_opt_param_len = 0;

        while (parsed_opt_param_len < param_length) {
            uint8_t capa_code = getValue<uint8_t> (&buffer);
            uint8_t capa_len = getValue<uint8_t> (&buffer);

            // capa_code & capa_len
            parsed_opt_param_len += 2;

            // capa value len exceed opt param
            if (parsed_opt_param_len + capa_len > param_length) {
                err_code = E_OPEN;
                err_subcode = BgpOpenErrorSubcode::E_UNSPEC;
                _bgp_error("BgpOpenMessage::parse: capability length exceed param length.\n");
                return -1;
            }

            if (capa_code == ASN_4B) {
                use_4b_asn = true;

                if (capa_len != 4) {
                    err_code = E_OPEN;
                    err_subcode = BgpOpenErrorSubcode::E_UNSPEC;
                    _bgp_error("BgpOpenMessage::parse: capability length invalid (want 4, saw %d).\n", capa_len);
                    return -1;
                }

                uint32_t my_4b_asn = ntohl(getValue<uint32_t> (&buffer));

                // value for 4b-asn field
                parsed_opt_param_len += 4;

                // peer has 4b-asn but my_asn is not AS_TRANS
                if (my_4b_asn > 65535 && my_asn != 23456) {
                    err_code = E_OPEN;
                    err_subcode = BgpOpenErrorSubcode::E_UNSPEC;
                    _bgp_error("BgpOpenMessage::parse: peer has 4b-asn but my_asn is not AS_TRANS (%d).\n", my_asn);
                    return -1;
                }

                my_asn = my_4b_asn;

                BgpCapability4BytesAsn cap = BgpCapability4BytesAsn ();
                cap.code = ASN_4B;
                cap.length = 4;
                cap.my_asn = my_4b_asn;
                capabilities.push_back(cap);

                continue;
            } else {
                BgpCapabilityUnknow cap (buffer, capa_len);
                cap.code = capa_code;

                // move buffer pointer
                buffer += capa_len;

                // count this capa
                parsed_opt_param_len += capa_len;

                continue;
            }
        }

        assert(parsed_opt_param_len == param_length);

        parsed_opt_params_len += parsed_opt_param_len;
    }

    assert(parsed_opt_params_len == opt_params_len);

    return parsed_opt_params_len + 10;
}


}