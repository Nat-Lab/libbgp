#include <libbgp/bgp-fsm.h>
#include <arpa/inet.h>

// This example demos how you can filter ingress/egress routes of BgpFsm. We 
// don't peer with a real BGP speaker in this example. Instead, we have two 
// BgpFsm talks to each other.

// Implement our output handler to pass data directly to another BGP FSM. 
// BgpOutHandler is used by BGP FSM to write BGP message to peer. 
// Usually, BGP FSM will write messages to a peer with a TCP socket. (a file 
// descriptor). In that case, we use FdOutHandler, which comes with libbgp. 
// However, here we want to talk to another BGP FSM running in the same program.
class PipedOutHandler : public libbgp::BgpOutHandler {
public:
    PipedOutHandler() {
        other = NULL;
    }

    void setPeer(libbgp::BgpFsm *other) {
        this->other = other;
    }

    bool handleOut(const uint8_t *buffer, size_t length) {
        return other->run(buffer, length) >= 0;
    };

private:
    libbgp::BgpFsm *other;
};

// Since we have two FSMs here, we want to label their log output so we know
// where the log is comming from. Implement our own log handler so we can label
// the log output.
class MyLoghandler : public libbgp::BgpLogHandler {
public:
    MyLoghandler(const char *name) {
        this->name = name;
    }

protected:
    void logImpl(const char* str) {
        printf("[%s] %s", name, str);
    }

private:
    const char *name;
};

int main(void) {

    // creare loggers for "local" and "remote"
    MyLoghandler local_logger("local");
    MyLoghandler remote_logger("remote");

    // we want to see detailed messages.
    local_logger.setLogLevel(libbgp::DEBUG);
    remote_logger.setLogLevel(libbgp::DEBUG);

    // create rules set object. Filter rule sets are passed to BgpFsm through 
    // BgpConfig object. If no rules set are provided, BgpFsm accepts all 
    // ingress and egress route.

    // egress rule set (apply to routes sending to peer). Say we only want to
    // send 172.16.0.0/24 to peer, nothing else.
    libbgp::BgpFilterRules egress_rules;

    // egress: append a rule: reject 0.0.0.0/0 and all the sub-prefix (reject 
    // everything)
    egress_rules.append<libbgp::BgpFilterRuleRoute4>(
        libbgp::BgpFilterRuleRoute4(libbgp::REJECT, libbgp::M_LE, libbgp::Prefix4("0.0.0.0", 0))
    );

    // egress: append another rule: accept 172.16.0.0/24.
    egress_rules.append<libbgp::BgpFilterRuleRoute4>(
        libbgp::BgpFilterRuleRoute4(libbgp::ACCEPT, libbgp::M_EQ, libbgp::Prefix4("172.16.0.0", 24))
    );

    // inegress rule set (apply to routes received from peer). Say we only want 
    // to allow 172.17.0.0/24 to peer, nothing else.
    libbgp::BgpFilterRules ingress_rules;

    // do the simular thing for inegress rules set
    ingress_rules.append<libbgp::BgpFilterRuleRoute4>(
        libbgp::BgpFilterRuleRoute4(libbgp::REJECT, libbgp::M_NE, libbgp::Prefix4("172.17.0.0", 24))
    );

    /* configure our "local" BgpFsm */
    libbgp::BgpConfig local_bgp_config;
    PipedOutHandler pipe_local;
    local_bgp_config.asn = 65000; // set local ASN
    local_bgp_config.peer_asn = 65001; // set peer ASN
    local_bgp_config.use_4b_asn = true; // enable RFC 6793
    local_bgp_config.mp_bgp_ipv4 = false; // not using mp-bgp
    local_bgp_config.mp_bgp_ipv6 = false; // not using mp-bgp
    local_bgp_config.hold_timer = 120; // hold timer
    local_bgp_config.out_handler = &pipe_local; // handle output with bridge
    local_bgp_config.no_collision_detection = true; // no need for that
    local_bgp_config.rib4 = NULL; // let BGP FSM create RIB for us.
    local_bgp_config.rev_bus = NULL; // we don't need event bus
    local_bgp_config.clock = NULL; // use system clock.
    local_bgp_config.log_handler = &local_logger; // use local logger
    local_bgp_config.in_filters4 = ingress_rules;
    local_bgp_config.out_filters4 = egress_rules;
    inet_pton(AF_INET, "10.0.0.1", &local_bgp_config.router_id); // router id

    // nexthop selection and nexthop validation is done with peering_lan_*
    // configutaion. For simplicity, we are disabling those checks here. Router
    // server example has demonstrated how peer_lan_* were used. For detailed 
    // usage, refer to the document.

    // always use 10.0.0.1 as nexthop. 
    inet_pton(AF_INET, "10.0.0.1", &local_bgp_config.default_nexthop4); 
    local_bgp_config.forced_default_nexthop4 = true; 

    // don't validate nexthop of routes received from peer.
    local_bgp_config.no_nexthop_check4 = true; 

    /* and configure the "remote" */
    libbgp::BgpConfig remote_bgp_config;
    PipedOutHandler pipe_remote;
    remote_bgp_config.asn = 65001;
    remote_bgp_config.peer_asn = 65000;
    remote_bgp_config.use_4b_asn = true;
    remote_bgp_config.mp_bgp_ipv4 = false;
    remote_bgp_config.mp_bgp_ipv6 = false;
    remote_bgp_config.hold_timer = 120;
    remote_bgp_config.out_handler = &pipe_remote;
    remote_bgp_config.no_collision_detection = true;
    remote_bgp_config.rib4 = NULL; 
    remote_bgp_config.rev_bus = NULL; 
    remote_bgp_config.clock = NULL;
    remote_bgp_config.log_handler = &remote_logger; 
    inet_pton(AF_INET, "10.0.0.2", &remote_bgp_config.router_id);
    inet_pton(AF_INET, "10.0.0.2", &remote_bgp_config.default_nexthop4); 
    remote_bgp_config.forced_default_nexthop4 = true; 
    remote_bgp_config.no_nexthop_check4 = true; 

    // create our "local" and "remote" FSMs, and connect them with each other.
    libbgp::BgpFsm local(local_bgp_config);
    libbgp::BgpFsm remote(remote_bgp_config);
    pipe_local.setPeer(&remote);
    pipe_remote.setPeer(&local);

    // before we peer, let's add some routes to local.

    // 10.0.0.0/24 (this will be filter by egress filter)
    local.getRib4().insert(&local_logger, libbgp::Prefix4("10.0.0.0", 24), local_bgp_config.default_nexthop4);

    // 172.16.0.0/16 (this will be filter by egress filter)
    local.getRib4().insert(&local_logger, libbgp::Prefix4("172.16.0.0", 16), local_bgp_config.default_nexthop4);

    // 172.16.0.0/16 (this will be accept by egress filter)
    local.getRib4().insert(&local_logger, libbgp::Prefix4("172.16.0.0", 24), local_bgp_config.default_nexthop4);

    // and also, add some routes to remote.

    // 10.1.0.0/24 (this will be filter by local ingress filter)
    remote.getRib4().insert(&remote_logger, libbgp::Prefix4("10.1.0.0", 24), remote_bgp_config.default_nexthop4);

    // 172.17.0.0/26 (this will be filter by local ingress filter)
    remote.getRib4().insert(&remote_logger, libbgp::Prefix4("172.17.0.0", 26), remote_bgp_config.default_nexthop4);

    // 172.17.0.0/23 (this will be accept by local ingress filter)
    remote.getRib4().insert(&remote_logger, libbgp::Prefix4("172.17.0.0", 24), remote_bgp_config.default_nexthop4);

    // send the open message from local.
    local.start();

    // finished, stop FSMs.
    local.stop();
    remote.stop();

    return 0;
}