#include "realtime-clock.h"
#include <time.h>

namespace bgpfsm {

uint64_t RealtimeClock::getTime() const {
    return time(NULL);
}

}
