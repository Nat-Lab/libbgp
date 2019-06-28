#include "bgp-open-message.h"
#include "bgp-errcode.h"
#include "bgp-error.h"
#include "value-op.h"
#include <stdlib.h>
#include <assert.h>
#include <arpa/inet.h>

namespace bgpfsm {

BgpOpenMessage::BgpOpenMessage() {
    this->type = OPEN;
    this->version = 4;
}

BgpOpenMessage::~BgpOpenMessage() { }

BgpOpenMessage::BgpOpenMessage(uint32_t my_asn, uint16_t hold_time, uint32_t bgp_id) : BgpOpenMessage() {
    this->my_asn = my_asn;
    this->hold_time = hold_time;
    this->bgp_id = bgp_id;
}

BgpOpenMessage::BgpOpenMessage(uint32_t my_asn, uint16_t hold_time, const char* bgp_id) : BgpOpenMessage() {
    this->my_asn = my_asn;
    this->hold_time = hold_time;
    inet_pton(AF_INET, bgp_id, &(this->bgp_id));
    this->bgp_id = ntohl(this->bgp_id);
}

ssize_t BgpOpenMessage::parse(const uint8_t *from, size_t msg_sz) {
    if (msg_sz < 10) {
        uint8_t _err_data = msg_sz;
        setError(E_HEADER, E_LENGTH, &_err_data, sizeof(uint8_t));
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
        setError(E_OPEN, E_UNSPEC_OPEN, NULL, 0);
        if (opt_params_len + 10 != (uint8_t) msg_sz) 
            _bgp_error("BgpOpenMessage::parse: size of rest of message (%d) != length of opt_param (%d).\n", msg_sz - 10, opt_params_len);
        if (opt_params_len < 2)
            _bgp_error("BgpOpenMessage::parse: opt params size < 2: %d.\n", opt_params_len);
        return -1;
    }

    uint8_t parsed_opt_params_len = 0;
    uint8_t opt_params_len_left = 0;

    while ((opt_params_len_left = opt_params_len - parsed_opt_params_len) > 0) {
        if (opt_params_len_left < 2) {
            setError(E_OPEN, E_UNSPEC_OPEN, NULL, 0);
            _bgp_error("BgpOpenMessage::parse: unexpected end of opt param list.\n");
            return -1;
        }

        uint8_t param_type = getValue<uint8_t> (&buffer);
        uint8_t param_length = getValue<uint8_t> (&buffer);

        // param_type & param_length
        parsed_opt_params_len += 2;

        // opt param size exceed opt_params_len
        if (parsed_opt_params_len + param_length > opt_params_len) {
            setError(E_OPEN, E_UNSPEC_OPEN, NULL, 0);
            _bgp_error("BgpOpenMessage::parse: opt param size exceed opt_params_len.\n");
            return -1;
        }

        // not capability?
        if (param_type != 2) {
            setError(E_OPEN, E_OPT_PARAM, NULL, 0);
            _bgp_error("BgpOpenMessage::parse: unknow opt param type: %d.\n", param_type);
            return -1;
        }

        // invalid capability field?
        if (param_length < 2) {
            setError(E_OPEN, E_UNSPEC_OPEN, NULL, 0);
            _bgp_error("BgpOpenMessage::parse: invalid capability opt param length: %d.\n", param_length);
            return -1;
        }

        uint8_t parsed_capa_param_len = 0;
        uint8_t capa_param_left = 0;

        while ((capa_param_left = param_length - parsed_capa_param_len) > 0) {
            if (capa_param_left < 2) {
                setError(E_OPEN, E_UNSPEC_OPEN, NULL, 0);
                _bgp_error("BgpOpenMessage::parse: unexpected end of capa list.\n");
                return -1;
            }

            uint8_t capa_code = getValue<uint8_t> (&buffer);
            uint8_t capa_len = getValue<uint8_t> (&buffer);

            // capa_code & capa_len
            parsed_capa_param_len += 2;

            // capa value len exceed opt param
            if (parsed_capa_param_len + capa_len > param_length) {
                setError(E_OPEN, E_UNSPEC_OPEN, NULL, 0);
                _bgp_error("BgpOpenMessage::parse: capability length exceed param length.\n");
                return -1;
            }

            if (capa_code == ASN_4B) {
                use_4b_asn = true;

                if (capa_len != 4) {
                    setError(E_OPEN, E_UNSPEC_OPEN, NULL, 0);
                    _bgp_error("BgpOpenMessage::parse: capability length invalid (want 4, saw %d).\n", capa_len);
                    return -1;
                }

                uint32_t my_4b_asn = ntohl(getValue<uint32_t> (&buffer));

                // value for 4b-asn field
                parsed_capa_param_len += 4;

                // peer has 4b-asn but my_asn is not AS_TRANS
                if (my_4b_asn >= 0xffff && my_asn != 23456) {
                    setError(E_OPEN, E_UNSPEC_OPEN, NULL, 0);
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
                parsed_capa_param_len += capa_len;

                continue;
            }
        }

        assert(parsed_capa_param_len == param_length);

        parsed_opt_params_len += parsed_capa_param_len;
    }

    assert(parsed_opt_params_len == opt_params_len);

    if ((size_t) (parsed_opt_params_len + 10) != msg_sz) {
        _bgp_error("BgpOpenMessage::parse: buffer does not end after parsing finished.\n");
        setError(E_OPEN, E_UNSPEC, NULL, 0);
        return -1;
    }

    return parsed_opt_params_len + 10;
}

ssize_t BgpOpenMessage::write(uint8_t *to, size_t buf_sz) const {
    if (buf_sz < 10) {
        _bgp_error("BgpOpenMessage::write: buffer size too small (need 10, avaliable %d).\n", buf_sz);
        return -1;
    }

    uint8_t *buffer = to;

    putValue<uint8_t>(&buffer, version);
    putValue<uint16_t>(&buffer, htons(my_asn >= 0xffff ? 23456 : my_asn));
    putValue<uint16_t>(&buffer, htons(hold_time));
    putValue<uint32_t>(&buffer, htonl(bgp_id));
    
    if (!use_4b_asn) {
        // no opt_param
        putValue<uint8_t>(&buffer, 0);
        return 10;
    }

    if (buf_sz < 18) {
        _bgp_error("BgpOpenMessage::write: buffer size too small (need 18, avaliable %d).\n", buf_sz);
        return -1;
    }

    // opt_param length is always 8 if we generate a open message, sicne we
    // only support 4b-asn capability for now.
    putValue<uint8_t>(&buffer, 8);
    
    // opt_param type 2: capability
    putValue<uint8_t>(&buffer, 2);

    // opt param len: 6
    putValue<uint8_t>(&buffer, 6);

    // capability 65: 4b-asn
    putValue<uint8_t>(&buffer, 65);

    // capability length: 4
    putValue<uint8_t>(&buffer, 4);

    // put my_asn
    putValue<uint32_t>(&buffer, htonl(my_asn));

    return 18;
}

ssize_t BgpOpenMessage::print(size_t indent, uint8_t *buffer, size_t buffer_size) const {
    uint8_t *to = buffer;
    size_t buf_left = buffer_size;

    _print(indent, &to, &buf_left, "OpenMessage {\n");

    indent++; {
        _print(indent, &to, &buf_left, "Version { %d }\n", version);
        _print(indent, &to, &buf_left, "MyAsn { %d }\n", my_asn);
        _print(indent, &to, &buf_left, "HoldTimer { %d }\n", hold_time);
        _print(indent, &to, &buf_left, "FourOctetSupport { %s }\n", use_4b_asn ? "true" : "false");
    }; indent--;

     _print(indent, &to, &buf_left, "}\n");

    return buffer_size - buf_left;
}

}