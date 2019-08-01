libbgp
---
`libbgp` is a BGP (Border Gateway Protocol) library written in C++. It comes with BGP message serializer/deserializer and a BGP Finite State Machine which has all the infrastructures needed (BGP RIB, Packet Sink, Route filtering) to build a BGP speaker.

`BgpPacket` ([document](https://lab.nat.moe/libbgp-doc/classlibbgp_1_1BgpPacket.html)) is a BGP message  deserialization/serialization tool.

`BgpFsm` ([document](https://lab.nat.moe/libbgp-doc/classlibbgp_1_1BgpFsm.html)) is a finite state machine that handles a single BGP session. Multiple `BgpFsm`s can be created to handle multiple sessions with different peers. BGP FSM will uses `RouteEventBus` to communicate with other FSMs. `BgpFsm` holds no information about the underlying transport protocol (for BGP, the standard is to use TCP), and it is only an FSM that take streams of binary data and output, a stream of binary data.

For simple usage and quick start, refer to examples. For detailed API usages, refer to document.

### Install

`libbgp` uses autotools for build automation. In general, you need the following build dependencies:

- g++ (or any other c++ compiler)
- make
- autotools (autoconf, automake, m4)
- autoconf-archive
- libtool
- Doxygen (for generating documents)

If you use a Debian based operating system, you should be able to install these with the following apt command:

```
# apt install g++ make autoconf autoconf-archive automake m4 libtool doxygen
```

Once you have the dependencies installed, use the following commands to build libbgp:

```
$ ./autogen.sh && ./configure && make
# make install
```

### Document

libbgp document is available online at <https://lab.nat.moe/libbgp-doc>. You may also build the document by running `doxygen` command under the project root directory. (where the `Doxyfile` is located) You will find the document under `docs/` folder.

### Examples

Examples are available under the `examples/` directory. You may build example programs with the following command after installing libbgp:

```
$ c++ name_of_example.cc -lbgp -o name_of_example
```

### License

MIT