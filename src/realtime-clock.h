/**
 * @file realtime-clock.h
 * @author Nato Morichika <nat@nat.moe>
 * @brief A Clock implementation to use system time as time. 
 * @version 0.1
 * @date 2019-07-07
 * 
 * @copyright Copyright (c) 2019
 * 
 */
#ifndef REALTIME_CLOCK_H_
#define REALTIME_CLOCK_H_
#include "clock.h"

namespace libbgp {

/**
 * @brief The RealtimeClock class.
 * 
 * A Clock implementation to use system time as time. 
 */
class RealtimeClock : public Clock {
public:
    uint64_t getTime() const;
};

}

#endif // REALTIME_CLOCK_H_