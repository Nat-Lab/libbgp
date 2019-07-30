/**
 * @file peer-and-print.cc
 * @author Nato Morichika <nat@nat.moe>
 * @brief isten on TCP 0.0.0.0:179, wait for a peer, and print all BGP messages
 * sent/received with BgpFsm
 * @version 0.1
 * @date 2019-07-14
 * 
 * @copyright Copyright (c) 2019
 * 
 */
#include <libbgp/bgp-fsm.h>
#include <libbgp/bgp-config.h>
#include <libbgp/fd-out-handler.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <condition_variable>
#include <thread>
#include <chrono>

// configurations
#define ROUTER_ID "172.30.0.1"
#define BIND_ADDR INADDR_ANY
#define BIND_PORT 179
#define MY_ASN 65000

// for terminating the "ticker" thread. see comment below.
bool running = false;
std::condition_variable cv;
std::mutex ticker_mtx;

// a simple "ticker" to tick the FSM's clock for checking time-based events.
// (For example, peer hold timer expired, sending keepalives) If you are brave, 
// you might be okay with omitting the ticker, since BGP FSM tick the clock 
// itself when it receives any messages. (so if peer sends KEEPALIVE message, 
// the clock will tick, and the time-based events will be checked and handled)
void ticker(libbgp::BgpFsm &fsm) {
    fprintf(stderr, "ticker(): ticker started.\n");
    while (true) {
        if (!running) break;
        std::unique_lock<std::mutex> lock(ticker_mtx);
        if (cv.wait_for(lock, std::chrono::seconds(1)) == std::cv_status::timeout) {
            fsm.tick();
        } else break;
    }
    fprintf(stderr, "ticker(): ticker stopped.\n");
}

int main(void) {
    /* socket related stuff... */

    int fd_sock = -1, fd_conn = -1;
    struct sockaddr_in server_addr, client_addr;

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = BIND_ADDR;
    server_addr.sin_port = htons(BIND_PORT);

    fd_sock = socket(AF_INET, SOCK_STREAM, 0);

    if (fd_sock < 0) {
        fprintf(stderr, "socket(): %s\n", strerror(errno));
        return 1;
    }

    int bind_ret = bind(fd_sock, (struct sockaddr *) &server_addr, sizeof(server_addr));

    if (bind_ret < 0) {
        fprintf(stderr, "bind(): %s\n", strerror(errno));
        close(fd_sock);
        return 1;
    }

    int listen_ret = listen(fd_sock, 1);

    if (listen_ret < 0) {
        fprintf(stderr, "listen(): %s\n", strerror(errno));
        close(fd_sock);
        return 1;
    }

    fprintf(stderr, "waiting for any client...\n");
    char ip_str[INET_ADDRSTRLEN];
    socklen_t caddr_len = sizeof(client_addr);
    fd_conn = accept(fd_sock, (struct sockaddr *) &client_addr, &caddr_len);

    if (fd_conn < 0) {
        fprintf(stderr, "accept(): %s\n", strerror(errno));
        close(fd_sock);
        close(fd_conn);
        return 1;
    }

    inet_ntop(AF_INET, &(client_addr.sin_addr), ip_str, INET_ADDRSTRLEN);

    fprintf(stderr, "accept(): new client from %s.\n", ip_str);

    // we only do one at a time, no new client.
    close(fd_sock);

    /* end of socket related stuff. */

    // create an out handler. Out handler is to allow BGP FSM to write outgoing 
    // messages. Here we use the FdOutHandler since we are writing to socket 
    // file descriptor.
    libbgp::FdOutHandler out_handler(fd_conn);

    // Create a BGP FSM configuration object, so we can pass configuration
    // parameters to FSM.
    libbgp::BgpConfig config;

    // get a default logger (output to stderr), and set to debug level to get
    // all log messages.
    libbgp::BgpLogHandler logger;
    logger.setLogLevel(libbgp::DEBUG);

    config.asn = MY_ASN; // set local ASN
    config.peer_asn = 0; // 0: accept any ASN
    config.use_4b_asn = true; // enable RFC 6793
    config.mp_bgp_ipv4 = true;
    config.mp_bgp_ipv6 = true;
    config.hold_timer = 120; // hold timer
    config.no_nexthop_check4 = true; // don't validate nexthop.
    config.no_nexthop_check6 = true; // don't validate nexthop.
    config.out_handler = &out_handler; // handle output with FdOutHandler
    config.no_collision_detection = true; // we are the only one

    // for this example, we don't need event bus or pre-defined rib. see router 
    // server example for how those are used.
    config.rib4 = NULL; 
    config.rib6 = NULL;
    config.rev_bus = NULL; 

    config.clock = NULL; // use system clock.
    config.log_handler = &logger;

    inet_pton(AF_INET, ROUTER_ID, &config.router_id); // router id.

    libbgp::BgpFsm fsm(config); // create the FSM
    
    // start the "ticker" thread
    running = true;
    std::thread ticker_thread(ticker, std::ref(fsm));

    uint8_t *read_buffer = (uint8_t *) malloc(65536);

    while (true) {
        ssize_t read_ret = read(fd_conn, read_buffer, 65536);

        if (read_ret < 0) {
            fprintf(stderr, "read(): %s.\n", strerror(errno));
            break;
        }

        if (read_ret == 0) {
            fprintf(stderr, "read(): peer gone.\n");
            break;
        }

        // pass the message to fsm
        int fsm_ret = fsm.run(read_buffer, (size_t) read_ret); 

        // ret 0: fatal error/reset by peer.
        // ret 2: notification sent to peer
        if (fsm_ret <= 0 || fsm_ret == 2) break;

    }

    running = false;
    cv.notify_all();
    if (ticker_thread.joinable()) {
        fprintf(stderr, "waiting for the ticker thread to stop...\n");
        ticker_thread.join();
    }
    
    fsm.stop();
    fprintf(stderr, "closing socket & clean up...\n");
    free(read_buffer);
    close(fd_conn);
    return 0;
}