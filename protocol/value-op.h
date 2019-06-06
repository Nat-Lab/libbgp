#ifndef VALUE_OP_H_
#define VALUE_OP_H_
#include <stdint.h>
#include <unistd.h>

namespace bgpfsm {

template <typename T> T getValue(const uint8_t **buffer);
template <typename T> size_t putValue(uint8_t **buffer, T value);

}
#endif // VALUE_OP_H_