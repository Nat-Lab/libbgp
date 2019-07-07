/**
 * @file realtime-clock.cc
 * @author Nato Morichika <nat@nat.moe>
 * @brief A Clock implementation to use system time as time. 
 * @version 0.1
 * @date 2019-07-07
 * 
 * @copyright Copyright (c) 2019
 * 
 */
#include "realtime-clock.h"
#include <time.h>

namespace libbgp {

uint64_t RealtimeClock::getTime() const {
    return time(NULL);
}

}
