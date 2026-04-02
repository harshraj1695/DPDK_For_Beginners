# DPDK NAT Project (version5)

This directory contains a small DPDK application that receives packets on a NIC (port `0`), applies simple IPv4 NAT rules, and transmits packets back out.

It implements two NAT behaviors:

- **Inbound static NAT (DNAT)**: `public_ip -> private_ip` mappings loaded from `nat_inbound.txt`.
- **Outbound dynamic NAT pool (SNAT)**: `private_ip -> public_ip` mappings allocated from a configured public IP pool (either hardcoded defaults or loaded from `nat.txt`).

It also uses **two DPDK timers**:

- A **periodic** timer that runs every 1 second to release idle outbound mappings.
- A **single-shot** timer that fires only after **10 consecutive seconds of no traffic** and clears the entire outbound NAT table.

## Quick Start (Build/Run)

### Build

From this directory:

```bash
make
```

This uses `pkg-config libdpdk` to get include flags and link flags (see `makefile`).

### Run

This app calls `rte_eal_init(argc, argv)` and then runs its RX/TX loop. You must launch it with the DPDK EAL arguments that match your machine (hugepages, lcores, device binding).

Typical usage pattern:

```bash
sudo ./main <EAL args...>
```

At runtime it prompts:

- Enter `0` to use the default outbound NAT pool (`NAT_POOL_START`, `NAT_POOL_RANGE`).
- Enter `1` to load outbound pool config from `nat.txt`.

## Packet Processing Logic (What Happens to Each Packet)

All packet rewriting is inside `nat_pool_apply()` in `static_nat.c`. The main loop in `main.c` does RX, calls the NAT function, then TX.

For each received packet `mbuf`:

1. Parse Ethernet header.
2. If not IPv4 (`ether_type != RTE_ETHER_TYPE_IPV4`): do nothing and continue.
3. Parse IPv4 header.
4. **Inbound static NAT check (DNAT)**:
   - Look up the packet `dst_addr` in `pool->inbound_map` (loaded from `nat_inbound.txt`).
   - If found: rewrite `dst_addr` to the configured private IP and fix checksums.
   - Then `continue` to the next packet (no outbound logic for this packet).
5. **Outbound NAT (SNAT)**:
   - Look up `src_addr` in `pool->outbound_map`.
   - If mapping exists: update `last_seen_tsc`, rewrite `src_addr` to the assigned public IP, fix checksums.
   - If mapping does not exist: find a free public IP slot from the pool:
     - If pool exhausted: free the mbuf and set it to `NULL` (caller skips TX).
     - Else: create a new mapping `private_ip -> slot`, store it in the hash, set `last_seen_tsc`, rewrite `src_addr`, fix checksums.

### Checksums

After rewriting the IPv4 `src_addr` or `dst_addr`, the code:

- Recomputes IPv4 header checksum (`rte_ipv4_cksum`).
- Recomputes L4 checksum for TCP or UDP (`rte_ipv4_udptcp_cksum`).

This logic is in `rewrite_and_recalc()` in `static_nat.c`.

## Timer Logic (The "Two Timers" Design)

DPDK timers only fire when you call `rte_timer_manage()` on the lcore that owns the timers. This app calls `rte_timer_manage()` once per main-loop iteration in `main.c`.

### Timer 1: Periodic per-entry expiry (`expiry_timer`)

Purpose: continuously release outbound NAT pool slots that have been idle for too long.

- Armed in `nat_pool_create()` in `static_nat.c`.
- Type: `PERIODICAL`.
- Period: `EXPIRY_INTERVAL_SEC` (currently 1 second).
- Idle threshold: `IDLE_TIMEOUT_SEC` (currently 5 seconds) converted to cycles via `pool->idle_tsc = hz * IDLE_TIMEOUT_SEC`.

Callback: `expiry_timer_cb()`

- Runs every second.
- Scans all pool slots.
- For any slot `in_use == 1`:
  - If `now - last_seen_tsc >= idle_tsc`, it deletes the mapping from `outbound_map` and marks the slot free.

This means individual mappings can expire even while overall traffic is still flowing, if that specific private IP stops sending packets.

### Timer 2: Single-shot "no traffic" table reset (`table_timer`)

Purpose: detect a global idle period (no packets at all) and clear the whole outbound table after a long silence.

Design:

- It is **not** armed inside `nat_pool_create()`. It is armed from the RX path.
- On the **first** packet, `pool->started` is set to `1` (just a print/flag).
- On **every** packet burst, the app calls `nat_table_timer_arm(pool, hz, TABLE_TIMEOUT_SEC)`.
  - That function resets the timer as `SINGLE` for `timeout_sec` seconds.
  - Resetting on every packet pushes the deadline forward, so the timer only expires after `timeout_sec` seconds of no packets.

Callback: `table_timer_cb()`

- Prints a message that no packets arrived for `TABLE_TIMEOUT_SEC`.
- Calls `nat_pool_dump()` to show current mappings.
- Clears all outbound entries and calls `rte_hash_reset(pool->outbound_map)`.
- Sets `pool->started = 0` so the next packet is treated as "first packet after idle".

Why `SINGLE` (not `PERIODICAL`):

- With `SINGLE`, the timer fires once per idle period, then stops.
- With `PERIODICAL`, once traffic stops it would keep firing every 10 seconds forever during a long idle (repeated clears/prints), which usually is not desired for a "one-time idle event".

## Configuration Files

### `nat.txt` and `nat_config.txt` (Outbound pool)

Format:

```
start_ip range
```

Example:

```
203.0.113.1 5
```

This creates a pool of public IPs:

- `203.0.113.1`
- `203.0.113.2`
- ...

`main.c` loads from `nat.txt` when you choose option `1`. `nat_config.txt` is the same format, but is currently not used by the code.

### `nat_inbound.txt` (Inbound static mappings)

Format:

```
public_ip private_ip
```

Example:

```
203.0.113.1 192.168.1.10
```

Packets whose IPv4 destination equals `203.0.113.1` will have their destination rewritten to `192.168.1.10`.

Note: the comment in `nat_inbound.txt` says "all others are dropped", but the code does **not** drop them. If the inbound lookup fails, the packet falls through to the outbound NAT logic.

## File-by-File Breakdown

### `main.c`

Responsibilities:

- Initializes EAL (`rte_eal_init`).
- Initializes timer subsystem (`rte_timer_subsystem_init`) and reads `hz` (`rte_get_timer_hz`).
- Creates the packet mempool (`create_mempool`) and initializes the port (`init_port`).
- Creates an IP:port tracking hash (`ip_hash_create`) (currently unused).
- Loads the outbound NAT pool (default or from `nat.txt`).
- Loads inbound static NAT mappings from `nat_inbound.txt`.
- Main loop:
  - Calls `rte_timer_manage()` each iteration so timers can fire.
  - Receives a burst (`rte_eth_rx_burst`).
  - If any packets:
    - Arms/resets the "no traffic" table timer (`nat_table_timer_arm`) on every burst.
    - Calls `nat_pool_apply()` to rewrite packets.
    - Transmits non-NULL packets (`rte_eth_tx_burst`).

### `static_nat.c`

Implements all NAT logic and the timer callbacks.

Key functions:

- `nat_pool_create(start_ip, range, hz)`:
  - Allocates `struct nat_pool` and `entries[]`.
  - Creates 2 hashes:
    - `outbound_map`: `private_ip -> slot_index`.
    - `inbound_map`: `public_ip -> struct nat_inbound_entry*`.
  - Initializes the periodic `expiry_timer`.
  - Initializes (but does not arm) `table_timer`.
- `nat_inbound_add()` / `nat_inbound_load_from_file()`:
  - Loads inbound static mappings into `inbound_map`.
- `nat_pool_apply()`:
  - Applies inbound DNAT first, then outbound SNAT.
- `nat_table_timer_arm(pool, hz, timeout_sec)`:
  - Resets the single-shot idle timer used by `main.c`.
- `nat_pool_dump()`:
  - Prints inbound and outbound tables.
- `nat_pool_reset()`:
  - Stops the idle timer and clears outbound mappings.

Timer callbacks:

- `expiry_timer_cb()`:
  - Called every 1 second to free idle outbound mappings.
- `table_timer_cb()`:
  - Called once after 10 seconds of no packets to clear the whole outbound table.

### `config.c`

Implements `nat_load_from_file(filepath, hz)`:

- Reads the first non-empty, non-comment line.
- Parses `start_ip` and `range`.
- Calls `nat_pool_create(start_ip, range, hz)`.

### `port.c`

Implements `init_port(port_id, mp)`:

- Configures the NIC with 1 RX queue and 1 TX queue.
- Sets up RX/TX queues (size 1024).
- Starts the device.
- Prints the driver name.

### `mempool.c`

Implements `create_mempool()`:

- Creates an mbuf pool via `rte_pktmbuf_pool_create`.

### `hash.c`

Implements an optional monitoring hash table keyed by `(ip, port)`:

- `ip_hash_create()` creates `ip_port_table`.
- `hash_ips()` walks packets and records new source IP:port pairs (TCP/UDP only).
- `hash_ips_dump()` prints stored keys.
- `reset_ip_hash()` clears the table.

This is currently not wired into the RX path (the call is commented out in `main.c`).

### `source.h`

Declares:

- IP:port hash functions.
- NAT pool structs and functions.
- Port and mempool helpers.

## Current Limitations / Notes

- Outbound NAT is **IP-only** (no ports). Multiple flows from the same private IP share one public IP, and there is no PAT (port translation).
- Inbound static NAT allocates `struct nat_inbound_entry` with `malloc` and never frees them (OK for a long-running app with a fixed config, but it is technically a leak).
- Timer callbacks run on the lcore that calls `rte_timer_manage()` and that owns the timer (this app uses `rte_lcore_id()` in timer resets).

