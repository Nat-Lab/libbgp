#include <libbgp/bgp-fsm.h>
#include <libbgp/fd-out-handler.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <string.h>
#include <getopt.h>
#include <thread>
#include <utility>
#include <chrono>

class RouteServerLogHandler : public libbgp::BgpLogHandler {
public:
    RouteServerLogHandler(const char *peer_addr) {
        this->peer_addr = peer_addr;
        this->asn = 0;
    }

    void setAsn(uint32_t asn) {
        this->asn = asn;
    }

    void setAddr(const char *peer_addr) {
        this->peer_addr = peer_addr;
    }

protected:
    void logImpl(const char* str) {
        if (asn == 0) {
            printf("[AS??? %s] %s", peer_addr, str);
        } else printf("[AS%d %s] %s", asn, peer_addr, str);
    }

private:
    const char *peer_addr;
    uint32_t asn;
};

class RouteServer {
public:
    RouteServer(uint32_t my_asn, const char *bgp_id, const char *peering_lan_prefix, uint8_t peering_lan_length) : root_logger(0), rib(&root_logger) {
        base_config.asn = my_asn;
        base_config.peer_asn = 0; // 0: accept any
        base_config.use_4b_asn = true;
        base_config.hold_timer = 120;
        base_config.rib = &rib;
        base_config.rev_bus = &bus;
        base_config.clock = NULL;
        base_config.peering_lan_length = peering_lan_length;
        base_config.forced_default_nexthop = false;

        inet_pton(AF_INET, bgp_id, &(base_config.router_id));
        inet_pton(AF_INET, peering_lan_prefix, &(base_config.peering_lan_prefix));

        root_logger.setAddr(bgp_id);
        root_logger.setAsn(base_config.asn);
        log_level = libbgp::INFO;

        root_logger.setLogLevel(log_level);
    }

    void setLogLevel(libbgp::LogLevel lvl) {
        log_level = lvl;
        root_logger.setLogLevel(lvl);
    }

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
    void session_handler(const char *peer_addr, int fd) {
        uint8_t this_buffer[4096];

        RouteServerLogHandler this_logger (peer_addr);
        this_logger.setLogLevel(log_level);

        libbgp::FdOutHandler this_out (fd);
        libbgp::BgpConfig this_config (base_config);
        this_config.log_handler = &this_logger;
        this_config.out_handler = &this_out;

        libbgp::BgpFsm this_fsm(this_config);

        ssize_t read_ret = -1;

        fsm_list_mtx.lock();
        fsms.push_back(&this_fsm);
        fsm_list_mtx.unlock();

        while ((read_ret = read(fd, this_buffer, 4096)) > 0) {
            int fsm_ret = this_fsm.run(this_buffer, (size_t) read_ret);
            this_logger.setAsn(this_fsm.getPeerAsn());
            if (fsm_ret <= 0 || fsm_ret == 2) break;
        }

        // force FSM to go IDLE and remove routes.
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

    libbgp::BgpRib rib;
    libbgp::BgpConfig base_config;
    libbgp::RouteEventBus bus;
    libbgp::LogLevel log_level;
    std::mutex fsm_list_mtx;
    std::vector<libbgp::BgpFsm*> fsms;
};

void print_help (const char* me) {
    fprintf(stderr, "%s: simple bgp route server implementation with libbgp.\n", me);
    fprintf(stderr, "usage: %s -i router_id -a my_asn -p lan_prefix -l lan_prefix_length\n", me);
    fprintf(stderr, "example: start a route server for peering lan 10.0.0.0/24, use 65000 as local\n");
    fprintf(stderr, "         asn, and 10.0.0.1 as router id.\n");
    fprintf(stderr, "         %s -i 10.0.0.1 -a 65000 -p 10.0.0.0 -l 24\n", me);
    fprintf(stderr, "\n");
    fprintf(stderr, "the route server accepts peering from any asn.\n");
}

int main (int argc, char **argv) {
    char *bgp_id = NULL;
    char *lan_prefix = NULL;
    uint8_t lan_len = 0;
    uint32_t my_asn = 0;

    char opt;
    while ((opt = getopt(argc, argv, "i:a:p:l:")) != -1) {
        switch (opt) {
            case 'i': bgp_id = strdup(optarg); break;
            case 'a': my_asn = atoi(optarg); break;
            case 'p': lan_prefix = strdup(optarg); break;
            case 'l': lan_len = atoi(optarg); break;
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
    rs.loop_forever();

    return 0;
}