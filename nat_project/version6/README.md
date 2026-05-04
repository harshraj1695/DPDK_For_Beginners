# DPDK NAT Project (version6)

This directory contains a small DPDK application that:

- Receives packets on NIC `port 0`
- Applies a simple IPv4 **NAPT/PAT** (IP + port translation) for TCP/UDP
- Transmits the modified packets back out of the same port

The NAT implementation in `version6` is **flow-based**:

- A NAT mapping is created on the first outbound packet of a 5‑tuple.
- Return traffic is translated back using a reverse lookup table.
- Each NAT mapping has its **own DPDK timer** and expires after `NAT_TIMEOUT_SEC`.

This is not a “full inbound DNAT/port-forwarding NAT”. Inbound traffic is translated only if it matches an existing mapping created by outbound traffic.

## Quick Start (Build/Run)

### Build

From this directory:

```bash
make
```

Build uses `pkg-config libdpdk` flags (see `makefile`).

### Run

You must launch with correct DPDK EAL arguments for your machine (hugepages, lcores, and your port/vdev). Example (AF_PACKET vdev):

```bash
sudo ./main -l 0-2 -m 512 --huge-dir=/dev/hugepages --vdev=net_af_packet0,iface=eth0 --log-level=8
```

Change `iface=eth0` and EAL options to match your setup.

## Configuration (What to Edit)

- **Public IP used for NAT** is currently hardcoded in `main.c` (`RTE_IPV4(1, 2, 3, 4)`).
- NAT parameters live in `source.h`:
  - `NAT_TIMEOUT_SEC` (entry lifetime, seconds)
  - `MIN_PORT` / `MAX_PORT` (public port allocation range)
- `nat.txt` and `nat_inbound.txt` exist in this directory but are **not used** by `version6` code today.
- `config.c` contains a skeleton `nat_load_from_file()` but currently returns `NULL` (not wired into `main.c`).

## NAT Data Structures (Two Hash Tables + One Entry Object)

Defined in `source.h`:

- `struct nat_key` (5‑tuple): `{src_ip, dst_ip, src_port, dst_port, proto}`
- `struct nat_entry` stores:
  - Private endpoint: `private_ip:private_port`
  - Public endpoint: `public_ip:public_port`
  - Remote endpoint: `remote_ip:remote_port`
  - Two keys: `fwd_key` and `rev_key`
  - One timer: `struct rte_timer timer`

At runtime `static_nat.c` creates two DPDK hash tables:

- `fwd_hash`: lookup for **outbound** packets (private → public)
- `rev_hash`: lookup for **inbound** packets (public → private)

Important detail: hash `.name` must be unique per table (this version uses `nat_fwd_table` and `nat_rev_table`).

## Packet Processing (RX → NAT → TX)

Main loop (`main.c`):

1. `rte_eal_init()`
2. `rte_timer_subsystem_init()`
3. Create mempool (`create_mempool()`)
4. Initialize `port 0` (`init_port()`)
5. Initialize NAT (`nat_init(public_ip_be)`)
6. Loop:
   - RX burst from port 0
   - For each packet: call `nat_process_packet()`
   - TX forwarded packets; free dropped packets
   - Call `nat_timer_manage()` every loop (even when no packets)

### NAT decision logic (`nat_process_packet()`)

For each received packet:

1. Parse L2 and require IPv4.
2. Parse L4 and require TCP or UDP.
3. Determine direction:
   - If `dip == PUBLIC_IP`: treat as **incoming/return traffic**
     - Lookup in `rev_hash` using key `{remote_ip, public_ip, remote_port, public_port, proto}`
     - If found: refresh timer and rewrite destination to `private_ip:private_port` (DNAT for return traffic)
     - If not found: drop
   - Else: treat as **outgoing traffic**
     - Lookup in `fwd_hash` using key `{private_ip, remote_ip, private_port, remote_port, proto}`
     - If found: refresh timer
     - If not found: create a new `nat_entry`, allocate a public port, insert into both hashes, and arm timer
     - Rewrite source to `public_ip:public_port` (SNAT/PAT)

After any rewrite, checksums are recomputed:

- IPv4 header checksum (`rte_ipv4_cksum`)
- TCP/UDP checksum (`rte_ipv4_udptcp_cksum`)

## Timer/Expiry Logic (Per NAT Entry)

`NAT_TIMEOUT_SEC` (default 30s) is the lifetime of an entry.

- On entry create: timer is armed as `SINGLE` for `NAT_TIMEOUT_SEC`.
- On every matching packet (inbound or outbound): `refresh()` stops and rearms the same timer.
- On expiry: `nat_entry_expire()` runs and:
  - Deletes the forward key from `fwd_hash`
  - Deletes the reverse key from `rev_hash`
  - Frees the `nat_entry` (`rte_free`)

DPDK timers only fire when you call `rte_timer_manage()`. `main.c` calls `nat_timer_manage()` every loop iteration (including when RX returns 0).

## NAT Behavior (What Kind of NAT Is This?)

This behaves like an **endpoint-dependent / symmetric** NAT because the mapping is keyed on the full 5‑tuple (including the remote IP/port). Return traffic must match the same remote endpoint to hit `rev_hash`.

It does **not** provide inbound-initiated connectivity (no port forwarding/static DNAT rules).

## Debug Output

This version prints useful runtime information:

- `mempool.c`: mempool create parameters and errors (`rte_errno`)
- `static_nat.c`: NAT init parameters, hash table creation, and per-entry create/expiry logs

If traffic is heavy, the per-entry logs can be noisy.

## File-by-File Overview

- `main.c`: EAL init, timers init, mempool + port init, RX/NAT/TX loop.
- `static_nat.c`: NAT tables, packet parsing, rewrite, per-flow timer expiry.
- `source.h`: NAT structs/constants and function prototypes (plus a small IP:port hash API).
- `mempool.c`: creates the mbuf pool.
- `port.c`: sets up port 0 (1 RX queue, 1 TX queue) and prints driver name.
- `hash.c`: standalone sample hash of `(ip, port)`; currently not used by `main.c`.
- `config.c`: placeholder for reading NAT config from a file; not wired in and currently returns `NULL`.

## Current Limitations / Notes

- IPv4 only; TCP/UDP only (no ICMP handling).
- No support for IPv4 options or fragmentation handling.
- One public IP (hardcoded) + simple sequential port allocator; no collision handling if a port is already in use.
- `rte_hash_add_key_data()` return values are not checked (insertion failures are not handled).
- Design assumes a single main lcore doing NAT/timer management; multi-lcore scaling would need additional synchronization or a per-lcore design.
