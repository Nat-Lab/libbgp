#include "realtime-clock.h"
#include <time.h>

namespace libbgp {

uint64_t RealtimeClock::getTime() const {
    return time(NULL);
}

}
