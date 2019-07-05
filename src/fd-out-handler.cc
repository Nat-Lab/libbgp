/**
 * @file fd-out-handler.cc
 * @author Nato Morichika <nat@nat.moe>
 * @brief File descriptor out handler
 * @version 0.1
 * @date 2019-07-05
 * 
 * @copyright Copyright (c) 2019
 * 
 */
#include "fd-out-handler.h"
#include <unistd.h>

namespace libbgp {

/**
 * @brief Construct a new Fd Out Handler:: Fd Out Handler object
 * 
 * @param fd File descriptor to write to.
 */
FdOutHandler::FdOutHandler (int fd) {
    this->fd = fd;
}

bool FdOutHandler::handleOut(const uint8_t *buffer, size_t length) {
    return write(fd, buffer, length) == (ssize_t) length;
}

}