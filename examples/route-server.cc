/**
 * @file route-server.cc
 * @author Nato Morichika <nat@nat.moe>
 * @brief A BGP route server.
 * @version 0.1
 * @date 2019-07-12
 * 
 * Simple BGP route server implements with libbgp.
 * 
 * @copyright Copyright (c) 2019
 * 
 */
#include <libbgp/bgp-fsm.h>
#include <libbgp/fd-out-handler.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <string.h>
#include <getopt.h>
#include <thread>
#include <utility>
#include <chrono>

// Implement our logger so we can tag the log message with source ASN and peer 
// address.
class RouteServerLogHandler : public libbgp::BgpLogHandler {
public:
    RouteServerLogHandler(const char *peer_addr) {
        this->peer_addr = peer_addr;
        this->asn = 0;
        is_self = false;
    }

    void setAsn(uint32_t asn) {
        this->asn = asn;
    }

    void setAddr(const char *peer_addr) {
        this->peer_addr = peer_addr;
    }

    void setSelf() {
        is_self = true;
    }

protected:
    void logImpl(const char* str) {
        if (is_self) {
            printf("[route-server] %s", str);
            return;
        }
        if (asn == 0) {
            printf("[AS??? %s] %s", peer_addr, str);
        } else printf("[AS%d %s] %s", asn, peer_addr, str);
    }

private:
    const char *peer_addr;
    uint32_t asn;
    bool is_self;
};

// The actual route server.
class RouteServer {
public:
    RouteServer(uint32_t my_asn, const char *bgp_id, const char *peering_lan_prefix, uint8_t peering_lan_length) : 
    root_logger(0), rib(&root_logger) { // The constructor of BgpRib needs a logger. Let's pass that in.

        /* create a "base" config that every FSMs will be copying from */
        base_config.asn = my_asn; // set local ASN
        base_config.peer_asn = 0; // 0: accept peer with any ASN.
        base_config.use_4b_asn = true; // Enable four octets ASN support.
        base_config.mp_bgp_ipv4 = true; // enable mp-bgp
        base_config.mp_bgp_ipv6 = true; // also allow ipv6 route over ipv4 session
        base_config.hold_timer = 120; // hold timer
        base_config.rib4 = &rib; // all FSMs will share an RIB.
        base_config.rev_bus = &bus; // all FSMs should be communicating using the event bus.
        base_config.clock = NULL; // use system clock to check time-based evetns.
        base_config.peering_lan4 = libbgp::Prefix4(peering_lan_prefix, peering_lan_length);
        base_config.forced_default_nexthop4 = false;

        inet_pton(AF_INET, bgp_id, &(base_config.router_id));

        root_logger.setSelf();
        log_level = libbgp::INFO;

        root_logger.setLogLevel(log_level);
    }

    void setLogLevel(libbgp::LogLevel lvl) {
        log_level = lvl;
        root_logger.setLogLevel(lvl);
    }

    // The entry point of the route server. It handles socket.
    bool loop_forever() {
        int fd_sock = -1, fd_conn = -1;
        struct sockaddr_in server_addr, client_addr;

        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = INADDR_ANY;
        server_addr.sin_port = htons(179);

        fd_sock = socket(AF_INET, SOCK_STREAM, 0);

        if (fd_sock < 0) {
            root_logger.log(libbgp::FATAL, "socket(): %s\n", strerror(errno));
            return false;
        }

        int bind_ret = bind(fd_sock, (struct sockaddr *) &server_addr, sizeof(server_addr));

        if (bind_ret < 0) {
            root_logger.log(libbgp::FATAL, "bind(): %s\n", strerror(errno));
            close(fd_sock);
            return false;
        }

        int listen_ret = listen(fd_sock, 1);

        if (listen_ret < 0) {
            root_logger.log(libbgp::FATAL, "listen(): %s\n", strerror(errno));
            close(fd_sock);
            return false;
        }

        root_logger.log(libbgp::INFO, "waiting for any client...\n");
        socklen_t caddr_len = sizeof(client_addr);

        char client_addr_str[INET_ADDRSTRLEN];

        std::thread ticker_thread(&RouteServer::ticker, this);
        ticker_thread.detach();
        
        while (true) {
            fd_conn = accept(fd_sock, (struct sockaddr *) &client_addr, &caddr_len);

            if (fd_conn < 0) {
                root_logger.log(libbgp::ERROR, "accept(): %s\n", strerror(errno));
                close(fd_conn);
                continue;
            }

            inet_ntop(AF_INET, &(client_addr.sin_addr), client_addr_str, INET_ADDRSTRLEN);

            root_logger.log(libbgp::INFO, "accept(): new client from %s.\n", client_addr_str);

            std::thread handler_thread(&RouteServer::session_handler, this, client_addr_str, fd_conn);
            handler_thread.detach();
        }
    }

private:
    // Handle a session. 
    void session_handler(const char *peer_addr, int fd) {
        uint8_t this_buffer[4096];

        // create logger for the session
        RouteServerLogHandler this_logger (peer_addr);
        this_logger.setLogLevel(log_level);

        // use FdOutHandler to write FSM output to socket
        libbgp::FdOutHandler this_out (fd);

        // copy from base_config and set log/out handler
        libbgp::BgpConfig this_config (base_config);
        this_config.log_handler = &this_logger;
        this_config.out_handler = &this_out;

        // build the FSM.
        libbgp::BgpFsm this_fsm(this_config);

        ssize_t read_ret = -1;

        fsm_list_mtx.lock();
        fsms.push_back(&this_fsm);
        fsm_list_mtx.unlock();

        while ((read_ret = read(fd, this_buffer, 4096)) > 0) {
            int fsm_ret = this_fsm.run(this_buffer, (size_t) read_ret);
            this_logger.setAsn(this_fsm.getPeerAsn());

            // ret 0: fatal error/reset by peer.
            // ret 2: notification sent to peer
            if (fsm_ret <= 0 || fsm_ret == 2) break;
        }

        // force FSM to go IDLE and remove routes. (needed in case of TCP socket
        // disconected but FSM still active.
        this_fsm.resetHard();

        fsm_list_mtx.lock();
        for (std::vector<libbgp::BgpFsm *>::const_iterator iter = fsms.begin(); iter != fsms.end(); iter++) {
            if (*iter == &this_fsm) {
                fsms.erase(iter);
                break;
            }
        }
        fsm_list_mtx.unlock();

        root_logger.log(libbgp::INFO, "session with AS%d (%s) closed.\n", this_fsm.getPeerAsn(), peer_addr);
    }

    // "ticker" to tick FSM every one second.
    void ticker() {
        root_logger.log(libbgp::INFO, "clock started.\n");
        while (true) {
            fsm_list_mtx.lock();
            for (libbgp::BgpFsm *fsm : fsms) {
                root_logger.log(libbgp::DEBUG, "ticking FSM of AS%d.\n", fsm->getPeerAsn());
                fsm->tick();
            }
            fsm_list_mtx.unlock();
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        return;
    }

    RouteServerLogHandler root_logger;

    libbgp::BgpRib4 rib;
    libbgp::BgpConfig base_config;
    libbgp::RouteEventBus bus;
    libbgp::LogLevel log_level;
    std::mutex fsm_list_mtx;
    std::vector<libbgp::BgpFsm*> fsms;
};

void print_help (const char* me) {
    fprintf(stderr, "simple bgp route server implementation with libbgp.\n");
    fprintf(stderr, "the route server accepts peering from any asn.\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "usage: %s [-v] -i router_id -a my_asn -p lan_prefix -l lan_length\n", me);
    fprintf(stderr, "    -a my_asn             asn of the route server.\n");
    fprintf(stderr, "    -i route_id           router id of the route server.\n");
    fprintf(stderr, "    -l lan_length         prefix length of the peering lan in cidr notation.\n");
    fprintf(stderr, "    -p lan_prefix         prefix of the peering lan.\n");
    fprintf(stderr, "    -v                    enable debug output.\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "for example, running with '-i 10.0.0.1 -a 65000 -p 10.0.0.0 -l 24' starts a\n");
    fprintf(stderr, "route server for peering lan 10.0.0.0/24, use 65000 as local asn and 10.0.0.1\n");
    fprintf(stderr, "as router id.\n");
}

int main (int argc, char **argv) {
    char *bgp_id = NULL;
    char *lan_prefix = NULL;
    uint8_t lan_len = 0;
    uint32_t my_asn = 0;
    bool verbose = false;

    char opt;
    while ((opt = getopt(argc, argv, "i:a:p:l:v")) != -1) {
        switch (opt) {
            case 'i': bgp_id = strdup(optarg); break;
            case 'a': my_asn = atoi(optarg); break;
            case 'p': lan_prefix = strdup(optarg); break;
            case 'l': lan_len = atoi(optarg); break;
            case 'v': verbose = true; break;
            default:
                print_help(argv[0]);
                return 1;
        }
    }

    if (bgp_id == NULL || lan_prefix == NULL || lan_len == 0 || my_asn == 0) {
        print_help (argv[0]);
        return 1;
    }

    RouteServer rs (my_asn, bgp_id, lan_prefix, lan_len);
    if (verbose) rs.setLogLevel(libbgp::DEBUG);
    rs.loop_forever();

    return 0;
}