libbgp Examples
---

Here's some example of using libbgp. You can compile examples with the following command:

```
$ c++ name_of_example.cc -lbgp -o name_of_example
```

For example, to compile `deserialize-and-serialize.cc`, do the following:

```
$ c++ deserialize-and-serialize.cc -lbgp -o deserialize-and-serialize
```

Then you can run the example with `./deserialize-and-serialize`.

The following examples are avaliable: 

- `deserialize-and-serialize.cc`: Deserializing and serializing BGP message with `BgpPacket`.
- `peer-and-print.cc`: listen on TCP `0.0.0.0:179`, wait for a peer, and print all BGP messages sent/received with `BgpFsm`.

All the example codes are distributed under the  [Unlicense](https://unlicense.org) license.