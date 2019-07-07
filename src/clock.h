/**
 * @file clock.h
 * @author Nato Morichika <nat@nat.moe>
 * @brief The libbgp clock interface.
 * @version 0.1
 * @date 2019-07-07
 * 
 * @copyright Copyright (c) 2019
 * 
 */
#ifndef CLOCK_H_
#define CLOCK_H_
#include <stdint.h>

namespace libbgp {

// Clock: interface for bgp-fsm to get current time (for time-based events like
// hold_timer)

/**
 * @brief The Clock interface.
 * 
 * The clock interface is used by BgpFsm to get current time to check for 
 * time-based events like peer hold timer expired, and determine the timing of 
 * KEEPALIVE message. BgpFsm comes with a default Clock implementation, 
 * RealtimeClock. RealtimeClock is a clock that uses system time as time.
 * 
 * This might be useful if BgpFsm is used inside a simulator like ns3. A 
 * Ns3Clock class should be implemented to report simulated time to BgpFsm.
 */
class Clock {
public:

    /**
     * @brief Get the current time.
     * 
     * @return uint64_t current time in second.
     */
    virtual uint64_t getTime() const = 0;
    virtual ~Clock() {}
};

}

#endif // CLOCK_H_