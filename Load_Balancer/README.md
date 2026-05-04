# Load Balancer

This project is a small DPDK-based Layer 4 load balancer written in C.
It receives IPv4 TCP or UDP packets, rewrites the destination IP and port,
and forwards traffic to backends in round-robin order.

## Files

- `load_balancer.c` contains the packet processing logic.
- `Makefile` builds the binary.

## What It Does

- Initializes DPDK EAL and Ethernet devices.
- Creates an mbuf pool for packet buffers.
- Receives packets in bursts from RX queue 0.
- Rewrites the destination IP address.
- Rewrites the TCP or UDP destination port to `10000`.
- Recalculates IPv4 and L4 checksums.
- Tracks simple hit counters for each backend.

## Backends

The program currently rotates across these backend IPs:

- `192.168.0.101`
- `192.168.0.102`
- `192.168.0.103`

All backend ports are set to `10000`.

## Build

Set `RTE_SDK` so the compiler and linker can find DPDK headers and libraries,
then run:

```bash
make
```

## Run

You will normally launch the binary with DPDK EAL arguments first, followed by
any application arguments if you add them later. A typical example looks like:

```bash
sudo ./load_balancer -l 0-3 --no-huge --vdev=net_af_packet0,iface=eth0
```

Your exact command may vary depending on your DPDK setup, NIC binding, and hugepage configuration.

## Output

The program prints:

- A startup line for each initialized port.
- Periodic packet and drop statistics.
- Backend hit counters.

## Notes

- Only IPv4 TCP and UDP packets are rewritten.
- The current code uses a single active transmit and receive queue.
- The last initialized DPDK port becomes the active processing port.
