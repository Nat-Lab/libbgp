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

Then you can run the example with `./peer-and-print`. (you may need root for this specific example since example open a TCP socket listening on a well-known port)

The following examples are avaliable: 

- `deserialize-and-serialize.cc`: Deserializing and serializing BGP message with `BgpPacket`.
- `peer-and-print.cc`: listen on TCP `0.0.0.0:179`, wait for a peer, and print all BGP messages sent/received with `BgpFsm`. (`pthread` needed for the `ticker` thread)

All the example codes are distributed under the  [Unlicense](https://unlicense.org) license.