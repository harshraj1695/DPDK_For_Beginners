# DPDK RSS Example

## Overview

This project demonstrates a minimal **Receive Side Scaling (RSS)** setup
using DPDK.

The program configures a network port with multiple RX queues and
enables hardware RSS so that incoming packets from different flows are
distributed across multiple CPU cores. Each lcore polls one RX queue and
prints which queue received the packet.

This example is intended as a simple learning reference for
understanding how RSS works in DPDK.

------------------------------------------------------------------------

## What the Program Does

-   Initializes the DPDK Environment Abstraction Layer (EAL)
-   Creates a packet mbuf memory pool
-   Configures one NIC port
-   Enables RSS in hardware
-   Creates multiple RX queues
-   Launches one lcore per RX queue
-   Continuously polls packets using `rte_eth_rx_burst()`
-   Prints the queue ID and lcore ID for each received packet
-   Frees packets after processing

The program does not forward packets.\
It only demonstrates RSS-based packet distribution.

------------------------------------------------------------------------

## Why This Example Exists

Without RSS: - All packets arrive on one RX queue - One CPU becomes the
bottleneck - Performance collapses at high packet rates

With RSS: - Packets are hashed by the NIC - Flows are distributed across
RX queues - Each queue is processed by a different CPU core - Packet
processing becomes parallel - Cache locality improves - Lock contention
is reduced

This example shows the minimal configuration required to enable RSS in
DPDK.

------------------------------------------------------------------------

## RSS Configuration in This Program

The program enables RSS on:

-   IPv4 / IPv6 headers
-   TCP ports
-   UDP ports

The NIC computes a hash using packet header fields and assigns packets
to RX queues using the hardware indirection table.

Each lcore polls exactly one RX queue, ensuring proper parallel
processing.

------------------------------------------------------------------------

## Requirements

-   Linux system with DPDK installed
-   NIC supported by DPDK
-   NIC must support multiple RX queues
-   Root privileges to run the program
-   Hugepages configured

------------------------------------------------------------------------

## Build Instructions

Compile using pkg-config:

``` bash
gcc rss_example.c -o rss_example $(pkg-config --cflags --libs libdpdk)
```

------------------------------------------------------------------------

## Run Instructions

Example run:

``` bash
sudo ./rss_example -l 0-4 -n 4
```

Meaning:

-   Core 0 is master
-   Cores 1--4 are worker lcores
-   Each worker polls one RX queue

Ensure the number of worker cores is equal to or greater than the number
of RX queues.

------------------------------------------------------------------------

## How to Test RSS Works

RSS requires multiple flows to distribute traffic.

### Incorrect test

Single flow traffic:

``` bash
iperf -c <server>
```

All packets will likely appear on one queue.

### Correct test

Multiple parallel flows:

``` bash
iperf -c <server> -P 16
```

Packets should now be distributed across queues.

------------------------------------------------------------------------

## Expected Output

When RSS works correctly, you should see packets arriving on multiple
queues:

    Packet on queue 0 (lcore 1)
    Packet on queue 2 (lcore 3)
    Packet on queue 1 (lcore 2)
    Packet on queue 3 (lcore 4)

If all packets appear on queue 0, RSS may not be configured correctly or
traffic may contain only one flow.

------------------------------------------------------------------------

## Key Learning Points

-   RSS must be enabled in port configuration
-   Multiple RX queues must be created
-   Each queue should be polled by a separate lcore
-   Traffic must contain multiple flows for RSS to distribute packets
-   RSS improves throughput and scalability in high-speed networking
    applications

------------------------------------------------------------------------

## Possible Extensions

You can extend this example to:

-   Print RSS hash values per packet
-   Forward packets to another port
-   Measure per-queue statistics
-   Integrate with DPDK graph framework
-   Implement flow-aware load balancing
-   Add NUMA-aware memory pools

------------------------------------------------------------------------

