# sim

Simulates the entire sign, using mostly the same codebase.

## Building

Requirements:

- A recent version of CMake

## Running

The system communicates with named pipes for emulating the UART.

Create two with

```
$ mkfifo stoe
$ mkfifo etos
```

then point the new `stmsim` and `espsim` binaries at them:

```
$ # terminal 1
$ ./stmsim <etos 2>stoe

----

$ # terminal 2
$ ./espsim <stoe >etos
```

You can also use more magic if you want to record the messages, possibly to be looked at with `caplog`, or combined with the logging utilities below:

```
$ # terminal 1
$ ./stmsim <etos 2>stoe 2> >(logtime.py stoelog.txt)

----

$ # terminal 2
$ ./espsim <stoe >etos > >(logtime.py etoslog.txt)
```

(`>(something)` becomes a file descriptor, `> >(something)` redirects that normally)

Alternatively, make two more named pipes and pass them into `caplog`'s `realtime` mode.

You also need to setup a TAP interface to get the ESP network working.

```
# add a bridge that our machine is on: this will forward packets
# to/from the virtual esp
# if you have a vm system setup with a virbr0 interface, you can skip all the way to the tap0 setup and just use it instead

$ sudo ip link add br0 type bridge
$ sudo ip addr add <some ip unused, e.g. 192.168.122.1/24> dev br0
$ sudo ip link set br0 up

# setup nat

$ echo 1 > /proc/sys/net/ipv4/ip_forward
$ sudo iptables -t nat -A POSTROUTING -o <your wifi/ethernet port> -j MASQUERADE
$ sudo iptables -A FORWARD -i br0 -j ACCEPT
$ sudo ip route add <subnet for the bridge, e.g. 192.168.122.0/24> dev br0 proto kernel scope link src <ip of bridge, e.g. 192.168.122.1>

# setup the tap

$ sudo ip tuntap add dev tap0 mode tap user `whoami`
$ sudo ip link set tap0 master br0
$ sudo ip link set address 02:12:34:56:78:ab dev tap0
$ sudo ip link set tap0 up

# set config for espsim

$ export PRECONFIGURED_TAPIF=tap0
$ export SIMESP_IP <some ip on the subnet of the bridge, e.g 192.168.122.2>
$ export SIMESP_NETMASK <netmask of the bridge, e.g. 255.255.255.0>
$ export SIMESP_GATEWAY <ip of the bridge, e.g. 192.168.122.1>
```

The esp sim gets its SD card contents from the file `sdcard.bin` in the working directory, which should be a raw image of an SD card. You can create one with `fdisk` and `mkfs.vfat` and modify it
by loop mounting it; or by grabbing an actual SD card in use and taking a `dd`-style image of it.

## Structure

The `src` folder contains files to be mixed with the actual sources, any files here have precedence over those elsewhere, allowing for "overriding" of the original sources with mocked-up versions.
The two binaries communicate over standard file streams.

`espsim` sends data to `stmsim` over `stdout` and receives over `stdin`, sending debug data to `stderr`. To allow for terminal coloring, `stmsim` sends over `stderr`, but still receives on `stdin`.

The only file the fake ESP currently reads is the `config.txt`, placed in the working directory and with exactly identical contents to what it would be on the real sign.

## Logging

There are two programs provided here, `logtime.py` and `mergelog.py`, which create a logfile with extra bytes for the time and merge two of those logs into a normal log respectively.
