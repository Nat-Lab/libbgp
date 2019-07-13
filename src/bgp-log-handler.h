/**
 * @file bgp-log-handler.h
 * @author Nato Morichika <nat@nat.moe>
 * @brief BGP log handler
 * @version 0.1
 * @date 2019-07-05
 * 
 * @copyright Copyright (c) 2019
 * 
 */
#ifndef BGP_LOG_H_
#define BGP_LOG_H_
#include <mutex>
#include "serializable.h"

// log helper macro. some log taks a lot of resources to produce (e.g., print
// BgpPacket). This allow us to disable logging completely with macro and also
// force log level check.
#ifndef DISBALE_LOG_MACROS
#define LIBBGP_LOG(logger, level) if (logger->getLogLevel() >= level)
#else
#define LIBBGP_LOG(logger, level)
#endif

namespace libbgp {

class Serializable;

/**
 * @brief Log levels for logger (BgpLogHandler)
 * 
 */
enum LogLevel {
    FATAL,
    ERROR,
    WARN,
    INFO,
    DEBUG
};

/**
 * @brief The BgpLogHandler class.
 * 
 * The log handler class is used by various other classes to print message.
 * 
 */
class BgpLogHandler {
public:
    BgpLogHandler();

    void log(LogLevel level, const char* format_str, ...);
    void log(LogLevel level, const Serializable &serializable);
    void setLogLevel(LogLevel level);
    LogLevel getLogLevel() const;

    /**
     * @brief Destroy the Bgp Log Handler object
     * 
     */
    virtual ~BgpLogHandler() {}
protected:

    /**
     * @brief Log implementation. By default, it writes to stderr. You may
     * implement your own BgpLogHandler to write message to a different place.
     * 
     * @param str Log message.
     */
    virtual void logImpl(const char* str);

private:
    LogLevel level;
    std::mutex buf_mtx;
    char out_buffer[4096];
};

/** 
 * @example route-event-bus.cc
 * Example of adding new routes to RIB while BGP FSM is running. Notify BGP FSM
 * to send updates to the peer with RouteEventBus. This example also shows how
 * you can implement your own BgpOutHandler and BgpLogHandler.
 * 
 * @example route-filter.cc
 * Example of using ingress/egress route filtering feature of BgpFsm. This 
 * example also shows how you can implement your own BgpOutHandler and 
 * BgpLogHandler.
 * 
 * @example route-server.cc
 * Simple BGP route server implements with libbgp. Use of RouteEventBus and 
 * shared BgpRib is demoed in this example. This example also shows how you can
 * implement your own BgpLogHandler.
 */

}

#endif // BGP_LOG_H_