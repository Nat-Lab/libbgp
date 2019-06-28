#ifndef BGP_MSG_H_
#define BGP_MSG_H_
#include <stdint.h>
#include <unistd.h>
#include "serializable.h"

namespace bgpfsm {

enum BgpMessageType {
    OPEN = 1,
    UPDATE = 2,
    KEEPALIVE = 3,
    NOTIFICATION = 4
};

class BgpMessage : public Serializable {
public:
    BgpMessage();
    BgpMessageType type;

    // get error code
    virtual uint8_t getErrorCode() const;

    // get error subcode
    virtual uint8_t getErrorSubCode() const;

    // get error payload (data field of NOTIFICATION message)
    virtual const uint8_t* getError() const;

    // get length of error payload
    virtual size_t getErrorLength() const;

    virtual ~BgpMessage();
protected:
    // utility function to error related values
    void setError(uint8_t err, uint8_t suberr, const uint8_t *data, size_t data_len);

    uint8_t err_code;
    uint8_t err_subcode;
    size_t err_len;
    uint8_t *err_data;
};

}
#endif // BGP_MSG_H_