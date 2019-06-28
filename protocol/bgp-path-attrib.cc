#include "bgp-path-attrib.h"
#include "bgp-error.h"
#include "bgp-errcode.h"
#include "value-op.h"
#include <stdlib.h>
#include <assert.h>

namespace bgpfsm {

int8_t BgpPathAttrib::GetTypeFromBuffer(const uint8_t *from, size_t buffer_sz) {
    if (buffer_sz < 3) {
        _bgp_error("BgpPathAttrib::GetTypeFromBuffer: buffer too small.\n");
        return -1;
    }
    return *((uint8_t *) (from + 1));
}

BgpPathAttrib::BgpPathAttrib() {
    err_buf_len = 0;
    err_code = 0;
    err_subcode = 0;
    optional = transitive = partial = extened = false;
    err_buf = NULL;
    value_ptr = NULL;
}

BgpPathAttrib::~BgpPathAttrib() {
    if (err_buf_len > 0) free(err_buf);
    if (value_ptr != NULL) free(value_ptr);
}

ssize_t BgpPathAttrib::parse(const uint8_t *from, size_t length) {
    ssize_t header_len = parseHeader(from, length);

    if (header_len < 0) return -1;

    const uint8_t *buffer = from + 3;

    // Well-Known, Mandatory = !optional, transitive
    // Well-Known, Discretionary = !optional, !transitive
    // Optional, Transitive = optional, transitive
    // Optional, Non-Transitive = optional, !transitive
    if (!optional && transitive) {
        // well-known mandatory, but not recognized
        setError(E_UPDATE, E_BAD_WELL_KNOWN, from, value_len + header_len);
        _bgp_error("BgpPathAttrib::parse: flag indicates well-known, mandatory but this attribute is unknown.\n");
        // set value_len = 0, so we won't free() nullptr when destruct.
        value_len = 0; 
        return -1;
    }

    if (optional && !transitive && partial) {
        // optional non-transitive must not be partial
        setError(E_UPDATE, E_ATTR_FLAG, from, value_len + header_len);
        _bgp_error("BgpPathAttrib::parse: optional non-transitive must not be partial.\n");
        // set value_len = 0, so we won't free() nullptr when destruct.
        value_len = 0; 
        return -1;
    }

    value_ptr = (uint8_t *) malloc(value_len);
    memcpy(value_ptr, buffer, value_len);

    return value_len + header_len;
}

ssize_t BgpPathAttrib::write(uint8_t *to, size_t buffer_sz) const {
    if (buffer_sz < (size_t) (value_len + 3)) {
        _bgp_error("BgpPathAttrib::write: destination buffer size too small.\n");
        return -1;
    }

    if (!extened && value_len >= 0xffff) {
        _bgp_error("BgpPathAttrib::write: non-extended value has size > 65535: %d\n", value_len);
        return -1;
    }

    if (writeHeader(to, buffer_sz) < 2) return -1;

    uint8_t *buffer = to + 2;
    if (extened) putValue<uint16_t>(&buffer, value_len);
    else putValue<uint8_t>(&buffer, value_len);

    if (value_len > 0) memcpy(buffer, value_ptr, value_len);

    return value_len + 3;   
}

ssize_t BgpPathAttrib::parseHeader(const uint8_t *from, size_t buffer_sz) {
    if (buffer_sz < 3) {
        setError(E_UPDATE, E_UNSPEC_UPDATE, NULL, 0);
        _bgp_error("BgpPathAttrib::parseHeader: invalid attribute header size.\n");
        return -1;
    }
    const uint8_t *buffer = from;

    uint8_t flags = getValue<uint8_t>(&buffer);

    optional = (flags >> 7) & 0x1;
    transitive = (flags >> 6) & 0x1;
    partial = (flags >> 5) & 0x1;
    extened = (flags >> 4) & 0x1;
    type_code = getValue<uint8_t>(&buffer);
    if (extened && buffer_sz < 4) {
        setError(E_UPDATE, E_UNSPEC_UPDATE, NULL, 0);
        _bgp_error("BgpPathAttrib::parseHeader: invalid attribute header size (extened but size < 4).\n");
        return -1;
    }

    if (extened) value_len = getValue<uint16_t>(&buffer);
    else value_len = getValue<uint8_t>(&buffer);

    if (value_len > buffer_sz - 3) {
        err_code = E_UPDATE;
        // This is kind of "invalid length", but we are not using E_ATTR_LEN.
        // E_ATTR_LEN: "Attribute Length that conflict with the expected length
        // (based on the attribute type code)." This is not based on type code,
        // but it is buffer overflow, so we set subcode to E_UNSPEC,
        setError(E_UPDATE, E_UNSPEC_UPDATE, NULL, 0);
        _bgp_error("BgpPathAttrib::parseHeader: value_length (%d) < buffer left (%d).\n", value_len, buffer_sz - 3);
        return -1;
    }

    return extened ? 4 : 3;
}

ssize_t BgpPathAttrib::writeHeader(uint8_t *to, size_t buffer_sz) const {
    if (buffer_sz < (extened ? 3 : 4)) {
        _bgp_error("BgpPathAttrib::writeHeader: dst buffer too small: %d\n", buffer_sz);
        return -1;
    }
    
    uint8_t *buffer = to;
    uint8_t flags = (optional << 7) | (transitive << 6)| (partial << 5) | (extened << 4);
    putValue<uint8_t>(&buffer, flags);
    putValue<uint8_t>(&buffer, type_code);
    return 2;
}

void BgpPathAttrib::setError(uint8_t err, uint8_t suberr, const uint8_t *data, size_t data_len) {
    err_code = err;
    err_subcode = suberr;

    // err_buf_len not 0, setError() when error already there?
    assert(err_buf_len == 0);

    if (data_len == 0) return;
    err_buf_len = data_len;
    err_buf = (uint8_t *) malloc(err_buf_len);
    memcpy(err_buf, data, data_len);
}

uint8_t BgpPathAttrib::getErrorCode() const {
    return err_code;
}

uint8_t BgpPathAttrib::getErrorSubCode() const {
    return err_subcode;
}

const uint8_t* BgpPathAttrib::getError() const {
    return err_buf;
}

size_t BgpPathAttrib::getErrorLength() const {
    return err_buf_len;
}

BgpPathAttribOrigin::BgpPathAttribOrigin() {
    transitive = true;
    type_code = ORIGIN;
}

ssize_t BgpPathAttribOrigin::parse(const uint8_t *from, size_t length) {
    ssize_t header_length = parseHeader(from, length);
    if (header_length < 0) return -1;

    assert(type_code == ORIGIN);

    const uint8_t *buffer = from + 3;

    if (value_len < 1) {
        _bgp_error("BgpPathAttribOrigin::parse: incomplete attrib.\n");
        setError(E_UPDATE, E_UNSPEC_UPDATE, NULL, 0);
        return -1;
    }

    if (value_len != 1) {
        _bgp_error("BgpPathAttribOrigin::parse: bad length, want 1, saw %d.\n", value_len);
        setError(E_UPDATE, E_ATTR_LEN, from, value_len + header_length);
        return -1;
    }

    if (optional || !transitive || extened || partial) {
        _bgp_error("BgpPathAttribOrigin::parse: bad flag bits, must be !optional, !extended, !partial, transitive.\n");
        setError(E_UPDATE, E_ATTR_FLAG, from, value_len + header_length);
        return -1;
    }

    origin = getValue<uint8_t>(&buffer);

    if (origin > 2) {
        setError(E_UPDATE, E_ORIGIN, from, 4);
        _bgp_error("BgpPathAttribOrigin::parse: Bad Origin Value: %d.\n", origin);
        return -1;
    }

    return 4;
}

ssize_t BgpPathAttribOrigin::write(uint8_t *to, size_t buffer_sz) const {
    if (buffer_sz < 4) {
        _bgp_error("BgpPathAttribOrigin::write: destination buffer size too small.\n");
        return -1;
    }

    if (writeHeader(to, buffer_sz) < 0) return -1;
    uint8_t *buffer = to + 2;

    putValue<uint8_t>(&buffer, 1); // length = 1
    putValue<uint8_t>(&buffer, origin);
    return 4;
}

BgpPathAttribAsPath::BgpPathAttribAsPath(bool is_4b) {
    this->is_4b = is_4b;
    transitive = true;
    type_code = AS_PATH;
}

BgpAsPathSegment::BgpAsPathSegment(bool is_4b, uint8_t type) {
    this->is_4b = is_4b;
    this->type = type;
}

size_t BgpAsPathSegment::getCount() const {
    return value.size();
}

bool BgpAsPathSegment::prepend(uint32_t asn) {
    if (value.size() >= (is_4b ? 127 : 255)) return false;
    uint32_t prepend_asn = is_4b ? asn : (asn >= 0xffff ? 23456 : asn);

    value.insert(value.begin(), prepend_asn);
    return true;
}

ssize_t BgpPathAttribAsPath::parse(const uint8_t *from, size_t length) {
    ssize_t header_length = parseHeader(from, length);
    if (header_length < 0) return -1;

    assert(type_code == AS_PATH);

    if (optional || !transitive || extened || partial) {
        _bgp_error("BgpPathAttribAsPath::parse: bad flag bits, must be !optional, !extended, !partial, transitive.\n");
        setError(E_UPDATE, E_ATTR_FLAG, from , value_len + header_length);
        return -1;
    }

    const uint8_t *buffer = from + 3;

    // empty as_path
    if (value_len == 0) return 3; 

    uint8_t parsed_len = 0;

    while (parsed_len < value_len) {
        // bad as_path
        if (value_len - parsed_len < 3) {
            _bgp_error("BgpPathAttribAsPath::parse: incomplete as_path segment.\n");
            setError(E_UPDATE, E_AS_PATH, NULL, 0);
            return -1;
        }

        uint8_t type = getValue<uint8_t>(&buffer);
        uint8_t n_asn = getValue<uint8_t>(&buffer);

        // type & count
        parsed_len += 2;

        uint8_t asns_length = is_4b ? n_asn * sizeof(uint32_t) : n_asn * sizeof(uint16_t);

        // overflow
        if (parsed_len + asns_length > value_len) {
            _bgp_error("BgpPathAttribAsPath::parse: as_path overflow attribute length.\n");
            setError(E_UPDATE, E_AS_PATH, NULL, 0);
            return -1;
        }

        BgpAsPathSegment path(is_4b, type);
        if (is_4b) for (int i = 0; i < n_asn; i++) path.value.push_back(getValue<uint32_t>(&buffer));
        else for (int i = 0; i < n_asn; i++) path.value.push_back(getValue<uint16_t>(&buffer));
        as_paths.push_back(path);

        // parsed asns
        parsed_len += asns_length;
    }

    assert(parsed_len == value_len);

    return parsed_len + 3;
}

void BgpPathAttribAsPath::addSeg(uint32_t asn) {
    BgpAsPathSegment segment(is_4b, AS_SEQUENCE);
    segment.prepend(asn);
    as_paths.push_back(segment);
}

bool BgpPathAttribAsPath::prepend(uint32_t asn) {
    if (as_paths.size() == 0) {
        // nothing here yet, add a new sequence. (5.1.2.b.3)
        addSeg(asn);
        return true;
    }

    // something here already. what to do?
    BgpAsPathSegment *segment = as_paths.data();

    if (segment->type == AS_SET) {
        // seg is set, create a new segment of type AS_SEQUENCE (5.1.2.b.2)
        // FIXME: checks needed: really create a new segment? 
        addSeg(asn);
        return true;
    } else if (segment->type == AS_SEQUENCE) {
        if (segment->getCount() >= (is_4b ? 127 : 255)) {
            // seg full, create a new segment of type AS_SEQUENCE (5.1.2.b.1)
            addSeg(asn);
            return true;
        } else {
            segment->prepend(asn);
            return true;
        }
    }

    _bgp_error("BgpPathAttribAsPath::prepend: unknow first segment type: %d, can't append.\n", segment->type);
    return false;
}

ssize_t BgpPathAttribAsPath::write(uint8_t *to, size_t buffer_sz) const {
    if (buffer_sz < 3) {
        _bgp_error("BgpPathAttribAsPath::write: destination buffer size too small.\n");
        return -1;
    }

    if (writeHeader(to, buffer_sz) < 0) return -1;
    uint8_t *buffer = to + 2;

    // keep track of length field so we can write it later
    uint8_t *len_field = buffer;

    // skip length field for now
    buffer++;

    uint8_t written_len = 0;

    for (const BgpAsPathSegment &seg : as_paths) {
        if (seg.is_4b != !is_4b) { // maybe allow 2b-seg in 4b-mode?
            _bgp_error("BgpPathAttribAsPath::write: segment 4b-mode and message 4b-mode mismatch.\n");
            return -1;
        }

        size_t asn_count = seg.value.size();

        if (asn_count > is_4b ? 127 : 255) {
            _bgp_error("BgpPathAttribAsPath::write: segment size too big: %d\n", asn_count);
            return -1;
        }

        size_t bytes_need = asn_count * (is_4b ? sizeof(uint32_t) : sizeof(uint16_t)) + 2;

        if (written_len + bytes_need > buffer_sz) {
            _bgp_error("BgpPathAttribAsPath::write: destination buffer size too small.\n");
            return -1;
        }

        putValue<uint8_t>(&buffer, seg.type);
        putValue<uint8_t>(&buffer, asn_count);

        if (seg.is_4b) for (uint32_t asn : seg.value) putValue<uint32_t>(&buffer, asn);
        else for (uint16_t asn : seg.value) putValue<uint16_t>(&buffer, asn);

        written_len += bytes_need;
    }

    // fill in the length.
    putValue<uint8_t>(&len_field, written_len);

    // written_len: the as_paths, 3: attr header (flag, typecode, length)
    return written_len + 3;
}


BgpPathAttribNexthop::BgpPathAttribNexthop() {
    type_code = NEXT_HOP;
    transitive = true;
}

ssize_t BgpPathAttribNexthop::parse(const uint8_t *from, size_t length) {
    ssize_t header_length = parseHeader(from, length);
    if (header_length < 0) return -1;

    assert(type_code == NEXT_HOP);

    const uint8_t *buffer = from + 3;

    if (value_len < 4) {
        _bgp_error("BgpPathAttribNexthop::parse: incomplete attrib.\n");
        setError(E_UPDATE, E_UNSPEC_UPDATE, NULL, 0);
        return -1;
    }

    if (value_len != 4) {
        _bgp_error("BgpPathAttribNexthop::parse: bad length, want 4, saw %d.\n", value_len);
        setError(E_UPDATE, E_ATTR_LEN, from, value_len + header_length);
        return -1;
    }

    if (optional || !transitive || extened || partial) {
        _bgp_error("BgpPathAttribNexthop::parse: bad flag bits, must be !optional, !extended, !partial, transitive.\n");
        setError(E_UPDATE, E_ATTR_FLAG, from, value_len + header_length);
        return -1;
    }

    next_hop = getValue<uint32_t>(&buffer);

    // todo: validate nexthop

    return 7;
}

ssize_t BgpPathAttribNexthop::write(uint8_t *to, size_t buffer_sz) const {
    if (buffer_sz < 7) {
        _bgp_error("BgpPathAttribNexthop::write: destination buffer size too small.\n");
        return -1;
    }

    if (writeHeader(to, buffer_sz) < 0) return -1;
    uint8_t *buffer = to + 2;

    putValue<uint8_t>(&buffer, 4); // length = 4
    putValue<uint32_t>(&buffer, next_hop);
    return 7;
}


BgpPathAttribMed::BgpPathAttribMed() {
    type_code = MULTI_EXIT_DISC;
    optional = true;
}

ssize_t BgpPathAttribMed::parse(const uint8_t *from, size_t length) {
    ssize_t header_length = parseHeader(from, length);
    if (header_length < 0) return -1;

    assert(type_code == MULTI_EXIT_DISC);

    const uint8_t *buffer = from + 3;

    if (value_len < 4) {
        _bgp_error("BgpPathAttribMed::parse: incomplete attrib.\n");
        setError(E_UPDATE, E_UNSPEC_UPDATE, NULL, 0);
        return -1;
    }

    if (value_len != 4) {
        _bgp_error("BgpPathAttribMed::parse: bad length, want 4, saw %d.\n", value_len);
        setError(E_UPDATE, E_ATTR_LEN, from, value_len + header_length);
        return -1;
    }

    if (!optional || transitive || extened || partial) {
        _bgp_error("BgpPathAttribMed::parse: bad flag bits, must be optional, !extended, !partial, !transitive.\n");
        setError(E_UPDATE, E_ATTR_FLAG, from, value_len + header_length);
        return -1;
    }

    med = getValue<uint32_t>(&buffer);

    return 7;
}

ssize_t BgpPathAttribMed::write(uint8_t *to, size_t buffer_sz) const {
    if (buffer_sz < 7) {
        _bgp_error("BgpPathAttribMed::write: destination buffer size too small.\n");
        return -1;
    }

    if (writeHeader(to, buffer_sz) < 0) return -1;
    uint8_t *buffer = to + 2;

    putValue<uint8_t>(&buffer, 4); // length = 4
    putValue<uint32_t>(&buffer, med);
    return 7;
}


BgpPathAttribLocalPref::BgpPathAttribLocalPref() {
    type_code = LOCAL_PREF;
}

ssize_t BgpPathAttribLocalPref::parse(const uint8_t *from, size_t length) {
    ssize_t header_length = parseHeader(from, length);
    if (header_length < 0) return -1;

    assert(type_code == LOCAL_PREF);

    const uint8_t *buffer = from + 3;

    if (value_len < 4) {
        _bgp_error("BgpPathAttribLocalPref::parse: incomplete attrib.\n");
        setError(E_UPDATE, E_UNSPEC_UPDATE, NULL, 0);
        return -1;
    }

    if (value_len != 4) {
        _bgp_error("BgpPathAttribLocalPref::parse: bad length, want 4, saw %d.\n", value_len);
        setError(E_UPDATE, E_ATTR_LEN, from, value_len + header_length);
        return -1;
    }

    if (optional || transitive || extened || partial) {
        _bgp_error("BgpPathAttribLocalPref::parse: bad flag bits, must be !optional, !extended, !partial, !transitive.\n");
        setError(E_UPDATE, E_ATTR_FLAG, from, value_len + header_length);
        return -1;
    }

    local_pref = getValue<uint32_t>(&buffer);

    return 7;
}

ssize_t BgpPathAttribLocalPref::write(uint8_t *to, size_t buffer_sz) const {
    if (buffer_sz < 7) {
        _bgp_error("BgpPathAttribLocalPref::write: destination buffer size too small.\n");
        return -1;
    }

    if (writeHeader(to, buffer_sz) < 0) return -1;
    uint8_t *buffer = to + 2;

    putValue<uint8_t>(&buffer, 4); // length = 4
    putValue<uint32_t>(&buffer, local_pref);
    return 7;
}


BgpPathAttribAtomicAggregate::BgpPathAttribAtomicAggregate() {
    type_code = ATOMIC_AGGREGATE;
}

ssize_t BgpPathAttribAtomicAggregate::parse(const uint8_t *from, size_t length) {
    ssize_t header_length = parseHeader(from, length);
    if (header_length < 0) return -1;

    assert(type_code == ATOMIC_AGGREGATE);

    if (value_len != 0) {
        _bgp_error("BgpPathAttribAtomicAggregate::parse: bad length, want 0, saw %d.\n", value_len);
        setError(E_UPDATE, E_ATTR_LEN, from, value_len + header_length);
        return -1;
    }

    if (optional || transitive || extened || partial) {
        _bgp_error("BgpPathAttribAtomicAggregate::parse: bad flag bits, must be !optional, !extended, !partial, !transitive.\n");
        setError(E_UPDATE, E_ATTR_FLAG, from, value_len + header_length);
        return -1;
    }

    return 3;
}

ssize_t BgpPathAttribAtomicAggregate::write(uint8_t *to, size_t buffer_sz) const {
    if (buffer_sz < 3) {
        _bgp_error("BgpPathAttribAtomicAggregate::write: destination buffer size too small.\n");
        return -1;
    }

    if (writeHeader(to, buffer_sz) < 0) return -1;
    uint8_t *buffer = to + 2;

    putValue<uint8_t>(&buffer, 0); // length = 0

    return 3;
}


BgpPathAttribAggregator::BgpPathAttribAggregator(bool is_4b) {
    this->is_4b = is_4b;
    type_code = AGGREATOR;
    optional = true;
    transitive = true;
}

ssize_t BgpPathAttribAggregator::parse(const uint8_t *from, size_t length) {
    ssize_t header_length = parseHeader(from, length);
    if (header_length < 0) return -1;

    assert(type_code == AGGREATOR);

    const uint8_t *buffer = from + 3;
    const uint8_t want_len = (is_4b ? 8 : 6);

    if (value_len < want_len) {
        _bgp_error("BgpPathAttribAggregator::parse: incomplete attrib.\n");
        setError(E_UPDATE, E_UNSPEC_UPDATE, NULL, 0);
        return -1;
    }

    if (value_len != want_len) {
        _bgp_error("BgpPathAttribAggregator::parse: bad length, want %d, saw %d.\n", want_len, value_len);
        setError(E_UPDATE, E_ATTR_LEN, from, value_len + header_length);
        return -1;
    }

    if (!optional || !transitive || extened || partial) {
        _bgp_error("BgpPathAttribAggregator::parse: bad flag bits, must be optional, !extended, !partial, transitive.\n");
        setError(E_UPDATE, E_ATTR_FLAG, from, value_len + header_length);
        return -1;
    }

    if (is_4b) aggregator_asn = getValue<uint32_t>(&buffer);
    else aggregator_asn = getValue<uint16_t>(&buffer);
    aggregator = getValue<uint32_t>(&buffer);

    return 3 + want_len;
}

ssize_t BgpPathAttribAggregator::write(uint8_t *to, size_t buffer_sz) const {
    uint8_t write_value_sz = (is_4b ? 6 : 8);

    if (buffer_sz < (size_t) (write_value_sz + 3)) {
        _bgp_error("BgpPathAttribAggregator::write: destination buffer size too small.\n");
        return -1;
    }

    if (writeHeader(to, buffer_sz) < 0) return -1;
    uint8_t *buffer = to + 2;

    putValue<uint8_t>(&buffer, write_value_sz);
    
    if (!is_4b && aggregator_asn >= 0xffff) {
        _bgp_error("BgpPathAttribAggregator::write: bad asn. not 4b but asn is %d.\n", aggregator_asn);
        return -1;
    }

    if (is_4b) putValue<uint32_t>(&buffer, aggregator_asn);
    else putValue<uint16_t>(&buffer, aggregator_asn);
    putValue<uint32_t>(&buffer, aggregator);

    return write_value_sz + 3;
}


BgpPathAttribAs4Path::BgpPathAttribAs4Path() {
    optional = true;
    transitive = true;
    type_code = AS4_PATH;
}

ssize_t BgpPathAttribAs4Path::parse(const uint8_t *from, size_t length) {
    ssize_t header_length = parseHeader(from, length);
    if (header_length < 0) return -1;

    assert(type_code == AS4_PATH);

    if (!optional || !transitive || extened || partial) {
        _bgp_error("BgpPathAttribAs4Path::parse: bad flag bits, must be optional, !extended, !partial, transitive.\n");
        setError(E_UPDATE, E_ATTR_FLAG, from , value_len + header_length);
        return -1;
    }

    const uint8_t *buffer = from + 3;

    // empty as_path
    if (value_len == 0) return 3; 

    uint8_t parsed_len = 0;

    while (parsed_len < value_len) {
        // bad as_path
        if (value_len - parsed_len < 3) {
            _bgp_error("BgpPathAttribAs4Path::parse: incomplete as_path segment.\n");
            setError(E_UPDATE, E_AS_PATH, NULL, 0);
            return -1;
        }

        uint8_t type = getValue<uint8_t>(&buffer);
        uint8_t n_asn = getValue<uint8_t>(&buffer);

        // type & count
        parsed_len += 2;

        uint8_t asns_length = n_asn * sizeof(uint32_t);

        // overflow
        if (parsed_len + asns_length > value_len) {
            _bgp_error("BgpPathAttribAs4Path::parse: as_path overflow attribute length.\n");
            setError(E_UPDATE, E_AS_PATH, NULL, 0);
            return -1;
        }

        BgpAsPathSegment path(true, type);
        for (int i = 0; i < n_asn; i++) path.value.push_back(getValue<uint32_t>(&buffer));
        as4_paths.push_back(path);

        // parsed asns
        parsed_len += asns_length;
    }

    assert(parsed_len == value_len);

    return parsed_len + 3;
}

void BgpPathAttribAs4Path::addSeg(uint32_t asn) {
    BgpAsPathSegment segment(true, AS_SEQUENCE);
    segment.prepend(asn);
    as4_paths.push_back(segment);
}

bool BgpPathAttribAs4Path::prepend(uint32_t asn) {
    if (as4_paths.size() == 0) {
        // nothing here yet, add a new sequence. (5.1.2.b.3)
        addSeg(asn);
        return true;
    }

    // something here already. what to do?
    BgpAsPathSegment *segment = as4_paths.data();

    if (segment->type == AS_SET) {
        // seg is set, create a new segment of type AS_SEQUENCE (5.1.2.b.2)
        // FIXME: checks needed: really create a new segment? 
        addSeg(asn);
        return true;
    } else if (segment->type == AS_SEQUENCE) {
        if (segment->getCount() >= 127) {
            // seg full, create a new segment of type AS_SEQUENCE (5.1.2.b.1)
            addSeg(asn);
            return true;
        } else {
            segment->prepend(asn);
            return true;
        }
    }

    _bgp_error("BgpPathAttribAs4Path::prepend: unknow first segment type: %d, can't append.\n", segment->type);
    return false;
}

ssize_t BgpPathAttribAs4Path::write(uint8_t *to, size_t buffer_sz) const {
    if (buffer_sz < 3) {
        _bgp_error("BgpPathAttribAs4Path::write: destination buffer size too small.\n");
        return -1;
    }

    if (writeHeader(to, buffer_sz) < 0) return -1;
    uint8_t *buffer = to + 2;

    // keep track of length field so we can write it later
    uint8_t *len_field = buffer;

    // skip length field for now
    buffer++;

    uint8_t written_len = 0;

    for (const BgpAsPathSegment &seg4 : as4_paths) {
        size_t asn_count = seg4.value.size();

        if (asn_count > 127) {
            _bgp_error("BgpPathAttribAs4Path::write: segment size too big: %d\n", asn_count);
            return -1;
        }

        // asn list + seg type & asn count
        size_t bytes_need = asn_count * sizeof(uint32_t) + 2;

        if (written_len + bytes_need > buffer_sz) {
            _bgp_error("BgpPathAttribAs4Path::write: destination buffer size too small.\n");
            return -1;
        }

        // put type
        putValue<uint8_t>(&buffer, seg4.type);

        // put asn count
        putValue<uint8_t>(&buffer, asn_count);

        // put asns
        for (uint32_t asn : seg4.value) {
            putValue<uint32_t>(&buffer, asn);
        }

        written_len += bytes_need;
    }

    // fill in the length.
    putValue<uint8_t>(&len_field, written_len);

    // written_len: the as_paths, 3: attr header (flag, typecode, length)
    return written_len + 3;
}


BgpPathAttribAs4Aggregator::BgpPathAttribAs4Aggregator() {
    type_code = AS4_AGGREGATOR;
    optional = true;
    transitive = true;
}

ssize_t BgpPathAttribAs4Aggregator::parse(const uint8_t *from, size_t length) {
    ssize_t header_length = parseHeader(from, length);
    if (header_length < 0) return -1;

    assert(type_code == AS4_AGGREGATOR);

    const uint8_t *buffer = from + 3;

    if (value_len < 8) {
        _bgp_error("BgpPathAttribAs4Aggregator::parse: incomplete attrib.\n");
        setError(E_UPDATE, E_UNSPEC_UPDATE, NULL, 0);
        return -1;
    }

    if (value_len != 8) {
        _bgp_error("BgpPathAttribAs4Aggregator::parse: bad length, want 8, saw %d.\n", value_len);
        setError(E_UPDATE, E_ATTR_LEN, from, value_len + header_length);
        return -1;
    }

    if (!optional || !transitive || extened || partial) {
        _bgp_error("BgpPathAttribAs4Aggregator::parse: bad flag bits, must be optional, !extended, !partial, transitive.\n");
        setError(E_UPDATE, E_ATTR_FLAG, from, value_len + header_length);
        return -1;
    }

    aggregator_asn4 = getValue<uint32_t>(&buffer);
    aggregator = getValue<uint32_t>(&buffer);

    return 11;
}

ssize_t BgpPathAttribAs4Aggregator::write(uint8_t *to, size_t buffer_sz) const {
    if (buffer_sz < 11) {
        _bgp_error("BgpPathAttribAs4Aggregator::write: destination buffer size too small.\n");
        return -1;
    }

    if (writeHeader(to, buffer_sz) < 0) return -1;
    uint8_t *buffer = to + 2;

    putValue<uint8_t>(&buffer, 11);

    putValue<uint32_t>(&buffer, aggregator_asn4);
    putValue<uint32_t>(&buffer, aggregator);

    return 11;
}


BgpPathAttribCommunity::BgpPathAttribCommunity() {
    type_code = LOCAL_PREF;
    optional = true;
    transitive = true;
}

ssize_t BgpPathAttribCommunity::parse(const uint8_t *from, size_t length) {
    ssize_t header_length = parseHeader(from, length);
    if (header_length < 0) return -1;

    assert(type_code == COMMUNITY);

    const uint8_t *buffer = from + 3;

    if (value_len < 4) {
        _bgp_error("BgpPathAttribCommunity::parse: incomplete attrib.\n");
        setError(E_UPDATE, E_UNSPEC_UPDATE, NULL, 0);
        return -1;
    }

    if (value_len != 4) {
        _bgp_error("BgpPathAttribCommunity::parse: bad length, want 4, saw %d.\n", value_len);
        setError(E_UPDATE, E_ATTR_LEN, from, value_len + header_length);
        return -1;
    }

    if (optional || transitive || extened || partial) {
        _bgp_error("BgpPathAttribCommunity::parse: bad flag bits, must be !optional, !extended, !partial, !transitive.\n");
        setError(E_UPDATE, E_ATTR_FLAG, from, value_len + header_length);
        return -1;
    }

    community = getValue<uint32_t>(&buffer);

    return 7;
}

ssize_t BgpPathAttribCommunity::write(uint8_t *to, size_t buffer_sz) const {
    if (buffer_sz < 7) {
        _bgp_error("BgpPathAttribCommunity::write: destination buffer size too small.\n");
        return -1;
    }

    if (writeHeader(to, buffer_sz) < 0) return -1;
    uint8_t *buffer = to + 2;

    putValue<uint8_t>(&buffer, 4); // length = 4
    putValue<uint32_t>(&buffer, community);
    return 7;
}
}