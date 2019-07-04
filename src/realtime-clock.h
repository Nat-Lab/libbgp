#ifndef REALTIME_CLOCK_H_
#define REALTIME_CLOCK_H_
#include "clock.h"

namespace libbgp {

class RealtimeClock : public Clock {
public:
    uint64_t getTime() const;
};

}

#endif // REALTIME_CLOCK_H_