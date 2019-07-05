#include <libbgp/bgp-fsm.h>
#include <libbgp/bgp-config.h>
#include <libbgp/fd-out-handler.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#define ROUTER_ID "172.30.0.1"
#define BIND_ADDR INADDR_ANY
#define BIND_PORT 179
#define MY_ASN 65000

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

    config.asn = MY_ASN; // set local ASN
    config.peer_asn = 0; // 0: accept any ASN
    config.use_4b_asn = true; // enable RFC 6793
    config.hold_timer = 120; // hold timer
    config.no_nexthop_check = true; // don't validate nexthop.
    config.out_handler = &out_handler; // handle output with FdOutHandler

    // for this example, we don't need event bus or pre-defined rib. see router 
    // server example for how those are used.
    config.rib = NULL; 
    config.rev_bus = NULL; 

    config.clock = NULL; // use system clock.
    config.verbose = true; // print out all messages.
    config.log_handler = NULL; // use default log handler (write to stdout/stderr)

    inet_pton(AF_INET, ROUTER_ID, &config.router_id); // router id.
    inet_pton(AF_INET, ROUTER_ID, &config.nexthop); // nexthop

    libbgp::BgpFsm fsm(config); // create the FSM

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

    fprintf(stderr, "closing socket & clean up...\n");
    free(read_buffer);
    close(fd_sock);
    close(fd_conn);
    return 0;
}