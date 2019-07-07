/**
 * @file fd-out-handler.h
 * @author Nato Morichika <nat@nat.moe>
 * @brief File descriptor out handler
 * @version 0.1
 * @date 2019-07-05
 * 
 * @copyright Copyright (c) 2019
 * 
 */
#ifndef FD_OUT_HANDLER_H_
#define FD_OUT_HANDLER_H_
#include "bgp-out-handler.h"

namespace libbgp {

/**
 * @brief The FdOutHandler class.
 * 
 * FdOutHandler is a simple BgpOutHandler that write output to a file descriptor
 * 
 */
class FdOutHandler : public BgpOutHandler {
public:
    FdOutHandler(int fd);
    bool handleOut(const uint8_t *buffer, size_t length);
private:
    int fd;
};

/**
 * @example peer-and-print.cc
 * A simple BGP speaker listen on TCP 0.0.0.0:179, wait for a peer, and print 
 * all BGP messages sent/received with BgpFsm. 
 * 
 */

}
#endif // FD_OUT_HANDLER_H_