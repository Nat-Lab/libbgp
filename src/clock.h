#ifndef CLOCK_H_
#define CLOCK_H_
#include <stdint.h>

namespace libbgp {

// Clock: interface for bgp-fsm to get current time (for time-based events like
// hold_timer)
class Clock {
public:
    virtual uint64_t getTime() const = 0;
    virtual ~Clock() {}
};

}

#endif // CLOCK_H_