libbgp
---
`libbgp` is a BGP (Border Gateway Protocol) library written in C++. It comes with BGP message serializer/deserializer and a BGP Finite State Machine which has all the infrastructures needed (BGP RIB, Packet Sink, Route filtering) to build a BGP speaker.

`bgp-fsm` is a finite state machine that handles a single BGP session. Multiple `bgp-fsm`s can be created to handle multiple sessions with different peers. BGP FSM will uses `route-event-bus` to communicate with other FSMs. `bgp-fsm` holds no information about the underlying transport protocol (for BGP, the standard is to use TCP), and it is only an FSM that take streams of binary data and output, a stream of binary data.

For simple usage and quick start, refer to examples. For detailed API usages, refer to document.

`bgp-fsm` is currently under development.

### Install

`libbgp` uses autotools for build automation. In general, you need the following build dependencies:

- autotools (autoconf, automake, m4)
- libtool
- Doxygen (for generating documents)

If you use a Debian based operating system, you should be able to install these with the following apt command:

```
# apt install autoconf automake m4 libtool doxygen
```

Once you have the dependencies installed, use the following commands to build libbgp:

```
$ ./autogen.sh && ./configure && make
# make install
```

### Document

### Examples

