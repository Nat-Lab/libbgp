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

namespace libbgp {

/**
 * @brief The BgpLogHandler class.
 * 
 * The log handler class is used by various other classes to print message.
 * 
 */
class BgpLogHandler {
public:
    void stdout(const char* format_str, ...);
    void stderr(const char* format_str, ...);

    /**
     * @brief Destroy the Bgp Log Handler object
     * 
     */
    virtual ~BgpLogHandler() {}
protected:

    /**
     * @brief stdout implementation, by default it writes to the real stdout.
     * You may choose to override the implementation.
     * @param str 
     */
    virtual void stdoutImpl(const char* str);

    /**
     * @brief stderr implementation, by default it writes to the real stderr.
     * You may choose to override the implementation.
     * @param str 
     */
    virtual void stderrImpl(const char* str);
private:
    std::mutex buf_mtx;
    char out_buffer[4096];
};

/** 
 * @example route-event-bus.cc
 * Example of adding new routes to RIB while BGP FSM is running. Notify BGP FSM
 * to send updates to the peer with RouteEventBus. This example also shows how
 * you can implement your own BgpOutHandler and BgpLogHandler.
 * 
 */

}

#endif // BGP_LOG_H_