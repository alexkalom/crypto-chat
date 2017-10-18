# crypto-chat

## Description

This repository contains the source code for a simple encrypted chat. In order to encrypt the messages, the cryptodev-linux-1.9 is used. The chat offers a reliable channel of communication, since it uses tcp over ip and detects any incomplete messages and disregards them.
## Installation

Download and install the cryptode-linux-1.9

```
$ wget http://nwl.cc/pub/cryptodev-linux/cryptodev-linux-1.9.tar.gz
$ gunzip cryptodev-linux-1.9.tar.gz
$ tar -xvf cryptodev-linux-1.9.tar
$ cd cryptodev-linux-1.9
$ make
$ sudo insmod cryptodev.ko
```

and then build the client and the server
```
$ make
```

To run the server simply issue
```
$ ./server
```
and for the client
```
$ ./client address port
```
where the address is the address of the server (if on the same machine then the address is localhost) and the port is the port reported by the server program.