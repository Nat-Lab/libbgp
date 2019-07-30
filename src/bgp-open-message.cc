/**
 * @file bgp-open-message.cc
 * @author Nato Morichika <nat@nat.moe>
 * @brief The BGP open message.
 * @version 0.1
 * @date 2019-07-04
 * 
 * @copyright Copyright (c) 2019
 * 
 */
#include "bgp-open-message.h"
#include "bgp-errcode.h"
#include "value-op.h"
#include <stdlib.h>
#include <arpa/inet.h>

namespace libbgp {

/**
 * @brief Construct a new Bgp Open Message:: Bgp Open Message object
 * 
 * @param logger Pointer to logger object for error logging.
 * @param use_4b_asn Enable four octets ASN support.
 */
BgpOpenMessage::BgpOpenMessage(BgpLogHandler *logger, bool use_4b_asn) : BgpMessage(logger) {
    this->type = OPEN;
    this->version = 4;
    this->use_4b_asn = use_4b_asn;
}

BgpOpenMessage::~BgpOpenMessage() { }

/**
 * @brief Construct a new Bgp Open Message:: Bgp Open Message object
 * 
 * @param use_4b_asn Enable four octets ASN support.
 * @param my_asn Local ASN (2 octets). If you want to set a four octets ASN, 
 * make sure `use_4b_asn` is true and use 
 * `BgpOpenMessage::setAsn(uint32_t my_asn)`
 * @param hold_time Hold timer.
 * @param bgp_id Local BGP ID in network byte order
 */
BgpOpenMessage::BgpOpenMessage(BgpLogHandler *logger, bool use_4b_asn, uint16_t my_asn, uint16_t hold_time, uint32_t bgp_id) : BgpOpenMessage(logger, use_4b_asn) {
    this->my_asn = my_asn;
    this->hold_time = hold_time;
    this->bgp_id = bgp_id;
    this->use_4b_asn = use_4b_asn;
    if (use_4b_asn) setAsn(my_asn);
}

/**
 * @brief Construct a new Bgp Open Message:: Bgp Open Message object
 * 
 * @param use_4b_asn Enable four octets ASN support.
 * @param my_asn Local ASN (2 octets). If you want to set a four octets ASN, 
 * make sure `use_4b_asn` is true and use 
 * `BgpOpenMessage::setAsn(uint32_t my_asn)`
 * @param hold_time Hold timer.
 * @param bgp_id Local BGP ID in dotted string notation
 */
BgpOpenMessage::BgpOpenMessage(BgpLogHandler *logger, bool use_4b_asn, uint16_t my_asn, uint16_t hold_time, const char* bgp_id) : BgpOpenMessage(logger, use_4b_asn) {
    this->my_asn = my_asn;
    this->hold_time = hold_time;
    this->use_4b_asn = use_4b_asn;
    if (use_4b_asn) setAsn(my_asn);
    inet_pton(AF_INET, bgp_id, &(this->bgp_id));
}

/**
 * @brief Parse a BGP open message body.
 * 
 * @param from Pointer to message body buffer.
 * @param msg_sz Size of message.
 * @return ssize_t Bytes read.
 * @retval -1 Parse error. Error may be logged.
 * @retval >=0 Bytes read.
 * @throws "bad_parse" Internal parser error.
 */
ssize_t BgpOpenMessage::parse(const uint8_t *from, size_t msg_sz) {
    if (msg_sz < 10) {
        uint8_t _err_data = msg_sz;
        setError(E_HEADER, E_LENGTH, &_err_data, sizeof(uint8_t));
        logger->log(ERROR, "BgpOpenMessage::parse: invalid open message size: %d.\n", msg_sz);
        return -1;
    }

    const uint8_t *buffer = from;

    version = getValue<uint8_t> (&buffer);
    my_asn = ntohs(getValue<uint16_t> (&buffer));
    hold_time = ntohs(getValue<uint16_t> (&buffer));
    bgp_id = getValue<uint32_t> (&buffer);

    uint8_t opt_params_len = getValue<uint8_t> (&buffer);

    // size of rest of message != length of opt_param or invalid length
    if (opt_params_len != msg_sz - 10 || (opt_params_len < 2 && opt_params_len != 0)) {
        setError(E_OPEN, E_UNSPEC_OPEN, NULL, 0);
        if (opt_params_len + 10 != (uint8_t) msg_sz) 
            logger->log(ERROR, "BgpOpenMessage::parse: size of rest of message (%d) != length of opt_param (%d).\n", msg_sz - 10, opt_params_len);
        if (opt_params_len < 2)
            logger->log(ERROR, "BgpOpenMessage::parse: opt params size < 2: %d.\n", opt_params_len);
        return -1;
    }

    uint8_t parsed_opt_params_len = 0;
    uint8_t opt_params_len_left = 0;

    while ((opt_params_len_left = opt_params_len - parsed_opt_params_len) > 0) {
        if (opt_params_len_left < 2) {
            setError(E_OPEN, E_UNSPEC_OPEN, NULL, 0);
            logger->log(ERROR, "BgpOpenMessage::parse: unexpected end of opt param list.\n");
            return -1;
        }

        uint8_t param_type = getValue<uint8_t> (&buffer);
        uint8_t param_length = getValue<uint8_t> (&buffer);

        // param_type & param_length
        parsed_opt_params_len += 2;

        // opt param size exceed opt_params_len
        if (parsed_opt_params_len + param_length > opt_params_len) {
            setError(E_OPEN, E_UNSPEC_OPEN, NULL, 0);
            logger->log(ERROR, "BgpOpenMessage::parse: opt param size exceed opt_params_len.\n");
            return -1;
        }

        // not capability?
        if (param_type != 2) {
            setError(E_OPEN, E_OPT_PARAM, NULL, 0);
            logger->log(ERROR, "BgpOpenMessage::parse: unknow opt param type: %d.\n", param_type);
            return -1;
        }

        // invalid capability field?
        if (param_length < 2) {
            setError(E_OPEN, E_UNSPEC_OPEN, NULL, 0);
            logger->log(ERROR, "BgpOpenMessage::parse: invalid capability opt param length: %d.\n", param_length);
            return -1;
        }

        uint8_t parsed_capa_param_len = 0;
        uint8_t capa_param_left = 0;

        while ((capa_param_left = param_length - parsed_capa_param_len) > 0) {
            if (capa_param_left < 2) {
                setError(E_OPEN, E_UNSPEC_OPEN, NULL, 0);
                logger->log(ERROR, "BgpOpenMessage::parse: unexpected end of capa list.\n");
                return -1;
            }

            const uint8_t *capa_ptr = buffer;
            uint8_t capa_code = getValue<uint8_t> (&buffer);
            uint8_t capa_len = getValue<uint8_t> (&buffer);

            BgpCapability *cap = NULL;

            switch(capa_code) {
                case ASN_4B: cap = new BgpCapability4BytesAsn(logger); break;
                case MP_BGP: cap = new BgpCapabilityMpBgp(logger); break;
                default: cap = new BgpCapabilityUnknow(logger); break;
            }

            ssize_t capa_parsed_len = cap->parse(capa_ptr, capa_len + 2);

            if (capa_parsed_len < 0) {
                forwardParseError(*cap);
                delete cap;
                return -1;
            }

            if (capa_parsed_len != capa_len + 2) {
                logger->log(FATAL, "BgpOpenMessage::parse: parsed capability length mismatch but no error reported.\n");
                throw "bad_parse";
            }

            capabilities.push_back(std::shared_ptr<BgpCapability> (cap));
            parsed_capa_param_len += capa_parsed_len;
            buffer += capa_parsed_len - 2;
        }

        if (parsed_capa_param_len != param_length) {
            logger->log(FATAL, "BgpOpenMessage::parse: parsed capabilities length mismatch but no error reported.\n");
            throw "bad_parse";
        }

        parsed_opt_params_len += parsed_capa_param_len;
    }

    if (parsed_opt_params_len != opt_params_len) {
            logger->log(FATAL, "BgpOpenMessage::parse: parsed opt params length mismatch but no error reported.\n");
            throw "bad_parse";
    }

    if ((size_t) (parsed_opt_params_len + 10) != msg_sz) {
        logger->log(ERROR, "BgpOpenMessage::parse: buffer does not end after parsing finished.\n");
        setError(E_OPEN, E_UNSPEC, NULL, 0);
        return -1;
    }

    return parsed_opt_params_len + 10;
}

ssize_t BgpOpenMessage::write(uint8_t *to, size_t buf_sz) const {
    if (buf_sz < 10) {
        logger->log(ERROR, "BgpOpenMessage::write: buffer size too small (need 10, avaliable %d).\n", buf_sz);
        return -1;
    }

    uint8_t *buffer = to;

    putValue<uint8_t>(&buffer, version);
    putValue<uint16_t>(&buffer, htons(my_asn));
    putValue<uint16_t>(&buffer, htons(hold_time));
    putValue<uint32_t>(&buffer, bgp_id);

    if (capabilities.size() == 0) {
        putValue<uint8_t>(&buffer, 0);
        return 10;
    }

    uint8_t *params_len_ptr = buffer;
    buffer++;

    // 3: opt_param type, capa_len, capa_code
    if (buf_sz < 13) {
        logger->log(ERROR, "BgpOpenMessage::write: buffer size too small.\n", buf_sz);
        return -1;
    }

    size_t opt_params_len = 2; // type & length
    // opt_param type 2: capability
    putValue<uint8_t>(&buffer, 2);

    uint8_t *param_len_ptr = buffer;
    buffer++;

    size_t opt_param_len = 0;
    for (const std::shared_ptr<BgpCapability> &capa : capabilities) {
        ssize_t capa_wrt_ret = capa->write(buffer, buf_sz - opt_params_len - 10);
        if (capa_wrt_ret < 0) {
            return capa_wrt_ret;
        }

        opt_param_len += capa_wrt_ret;
        buffer += capa_wrt_ret;
    }

    putValue<uint8_t>(&param_len_ptr, opt_param_len);

    opt_params_len += opt_param_len;
    putValue<uint8_t>(&params_len_ptr, opt_params_len);

    return opt_params_len + 10;
}

ssize_t BgpOpenMessage::doPrint(size_t indent, uint8_t **to, size_t *buf_left) const {
    size_t written = 0;
    written += _print(indent, to, buf_left, "OpenMessage {\n");

    indent++; {
        written += _print(indent, to, buf_left, "Version { %d }\n", version);
        written += _print(indent, to, buf_left, "MyAsn { %d }\n", my_asn);
        written += _print(indent, to, buf_left, "BgpId { %s }\n", inet_ntoa(*(const struct in_addr*) &bgp_id));
        written += _print(indent, to, buf_left, "HoldTimer { %d }\n", hold_time);
        if (capabilities.size() == 0) written += _print(indent, to, buf_left, "Capabilities { }\n");
        else {
            _print(indent, to, buf_left, "Capabilities {\n");
            indent++; {
                for (const std::shared_ptr<BgpCapability> &capa : capabilities) {
                    ssize_t capa_written = capa->print(indent, *to, *buf_left);
                    if (capa_written < 0) return capa_written;
                    *to += capa_written;
                    *buf_left -= capa_written;
                    written += capa_written;
                }
            }; indent--;
            _print(indent, to, buf_left, "}\n");
        }
    }; indent--;

    written += _print(indent, to, buf_left, "}\n");

    return written;
}

/**
 * @brief Get ASN.
 * 
 * Get ASN of the open message. Will check Capabilities for four octets ASN if
 * four octets ASN support is enabled.
 * 
 * @return uint32_t ASN
 */
uint32_t BgpOpenMessage::getAsn() const {
    if (!use_4b_asn) return my_asn;

    for (const std::shared_ptr<BgpCapability> &capa : capabilities) {
        if (capa->code == ASN_4B) {
            const BgpCapability4BytesAsn &as4_cap = dynamic_cast<const BgpCapability4BytesAsn &>(*capa);
            return as4_cap.my_asn;
        }
    }

    return my_asn;
}

/**
 * @brief Set ASN.
 * 
 * Set ASN of the open message. Will update/create four octets ASN capability if
 * four octets ASN support is enabled.
 * 
 * @param my_asn ASN
 * @return true ASN set
 * @return false Failed to set ASN
 */
bool BgpOpenMessage::setAsn(uint32_t my_asn) {
    this->my_asn = my_asn >= 0xffff ? 23456 : my_asn;

    if (!use_4b_asn) return true;
    
    for (std::shared_ptr<BgpCapability> &capa : capabilities) {
        if (capa->code == ASN_4B) {
            BgpCapability4BytesAsn &as4_cap = dynamic_cast<BgpCapability4BytesAsn &>(*capa);
            as4_cap.my_asn = my_asn;
            return true;
        }
    }

    BgpCapability4BytesAsn *as4_cap = new BgpCapability4BytesAsn(logger);
    as4_cap->my_asn = my_asn;

    capabilities.push_back(std::shared_ptr<BgpCapability>(as4_cap));

    return true;
}

/**
 * @brief Check if open message has a capability.
 * 
 * @param code Capability code
 * @return true open message  has capability
 * @return false open message does not have capability.
 */
bool BgpOpenMessage::hasCapability(uint8_t code) const {
    for (const std::shared_ptr<BgpCapability> cap : capabilities) {
        if (cap->code == code) return true;
    }

    return false;
}

/**
 * @brief Get capabilities list.
 * 
 * @return const std::vector<std::shared_ptr<BgpCapability>>& Capabilities.
 */
const std::vector<std::shared_ptr<BgpCapability>>& BgpOpenMessage::getCapabilities() const {
    return capabilities;
}

/**
 * @brief Add a capability.
 * 
 * @param capability The capability.
 * @return true Capability added.
 * @return false Failed to add capability.
 */
bool BgpOpenMessage::addCapability(std::shared_ptr<BgpCapability> capability) {
    capabilities.push_back(capability);
    return true;
}

}