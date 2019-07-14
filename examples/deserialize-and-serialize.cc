/**
 * @file deserialize-and-serialize.cc
 * @author Nato Morichika <nat@nat.moe>
 * @brief serializing and deserializing with BgpPacket
 * @version 0.1
 * @date 2019-07-14
 * 
 * @copyright Copyright (c) 2019
 * 
 */
#include <libbgp/bgp-packet.h>
#include <libbgp/bgp-update-message.h>
#include <libbgp/bgp-open-message.h>
#include <arpa/inet.h>
#include <stdio.h>

int main (void) {
    // a logger for serializer/deserializer to print out error message. It 
    // prints to stdout/stderr by default.
    libbgp::BgpLogHandler logger;

    /* deserializing */

    // an update message
    const uint8_t *update_msg_buffer = (const uint8_t *) "\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\x00\x1c\x02\x00\x05\x1c\x8d\xc1\x15\x10\x00\x00";

    // create a BgpPacket object for deserializing. The "true" parameter is for
    // enableing four octets ASN support.
    libbgp::BgpPacket deserializer(&logger, true);

    // parse the message
    ssize_t deserializer_ret = deserializer.parse(update_msg_buffer, 28);

    if (deserializer_ret < 0) {
        fprintf(stderr, "failed to deserialize.\n");
        return 1;
    }

    char message_visualized[4096];

    // print the message
    deserializer.print((uint8_t *) message_visualized, 4096);
    printf("%s", message_visualized);

    // to access message data fields programmatically, get the message and cast 
    // it to corresponding type.
    const libbgp::BgpMessage *msg = deserializer.getMessage();

    // you can get the type information of the message by msg->type, so you
    // will know which type you should cast the message to.
    const libbgp::BgpUpdateMessage *update_msg =
        dynamic_cast<const libbgp::BgpUpdateMessage *>(msg);

    printf("Size of withdrawn routes: %li\n", update_msg->withdrawn_routes.size());

    /* end deserializing */

    /* serializing */

    uint32_t bgp_id = 0;
    inet_pton(AF_INET, "172.30.0.1", &bgp_id);

    // create an open message object to be serialized.
    // true: enable four octets ASN support, 0: local ASN, 120: hold timer.
    // BgpOpenMessage constructer does not accept four octects ASN, you should 
    // set four octets ASN with BgpOpenMessage::setAsn.
    libbgp::BgpOpenMessage open(&logger, true, 0, 120, bgp_id);

    // set the actual four octects ASN. 
    open.setAsn(396303);

    // create a BGP packet object to wrap the open message. You don't use the
    // write/parse method on BgpMessage class directly. They only 
    // deserialize/serialize message body.
    libbgp::BgpPacket serializer(&logger, true, &open);

    uint8_t message_serialized[4096];
    ssize_t serializer_ret = serializer.write(message_serialized, 4096);

    if (serializer_ret < 0) {
        fprintf(stderr, "failed to serialize.\n");
        return 1;
    }

    // print the message
    serializer.print((uint8_t *) message_visualized, 4096);
    printf("%s", message_visualized);

    /* end serializing */

    return 0;
}