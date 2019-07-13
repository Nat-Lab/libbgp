libbgp Examples
---

Here's some example of using libbgp. You can compile examples with the following command:

```
$ c++ name_of_example.cc -lbgp -o name_of_example
```

Some examples require you to link `pthread` too. Add `-lpthread` after `-lbgp` to compile those examples. For example, to compile `peer-and-print.cc`, do the following:

```
$ c++ peer-and-print.cc -lbgp -lpthread -o peer-and-print
```

Then you can run the example with `./peer-and-print`. (you may need root for this specific example since this example open a TCP socket listening on a well-known port)

The following examples are avaliable: 

- `deserialize-and-serialize.cc`: Deserializing and serializing BGP message with `BgpPacket`.
- `peer-and-print.cc`: listen on TCP `0.0.0.0:179`, wait for a peer, and print all BGP messages sent/received with `BgpFsm`. (`pthread` needed for the `ticker` thread)
- `route-event-bus.cc`: Example of adding new routes to RIB while BGP FSM is running. Notify BGP FSM to send updates to the peer with `RouteEventBus`. This example also shows how you can implement your own `BgpOutHandler` and `BgpLogHandler`.
- `route-filter.cc`: Example of using ingress/egress route filtering feature of BgpFsm. This example also shows how you can implement your own `BgpOutHandler` and `BgpLogHandler`.
- `route-server.cc`: Simple BGP route server implements with libbgp. Use of `RouteEventBus` and shared `BgpRib` is demoed in this example.  This example also shows how you can implement your own `BgpLogHandler`. (`pthread` needed)

All the example codes are distributed under the  [Unlicense](https://unlicense.org) license.