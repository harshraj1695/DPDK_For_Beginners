#include "source.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>

#include <rte_ether.h>
#include <rte_ip.h>
#include <rte_tcp.h>
#include <rte_udp.h>
#include <rte_hash.h>
#include <rte_jhash.h>
#include <rte_cycles.h>
#include <rte_byteorder.h>
#include <rte_timer.h>
#include <rte_lcore.h>

#define MAX_INBOUND_ENTRIES  256
#define MAX_LINE             64
#define IDLE_TIMEOUT_SEC     5
#define TABLE_TIMEOUT_SEC    10
#define EXPIRY_INTERVAL_SEC  1

// forward declarations
static void expiry_timer_cb(struct rte_timer *tim, void *arg);
static void table_timer_cb(struct rte_timer *tim, void *arg);

// ----------------------------------------------------------------
// create the NAT pool
// ----------------------------------------------------------------
struct nat_pool *nat_pool_create(const char *start_ip, uint32_t range, uint64_t hz)
{
    struct nat_pool *pool = malloc(sizeof(struct nat_pool));
    if (!pool)
        rte_exit(EXIT_FAILURE, "Failed to allocate nat_pool\n");

    pool->entries = calloc(range, sizeof(struct nat_pool_entry));
    if (!pool->entries)
        rte_exit(EXIT_FAILURE, "Failed to allocate nat_pool entries\n");

    struct in_addr addr;
    inet_pton(AF_INET, start_ip, &addr);
    pool->pool_start_ip = rte_be_to_cpu_32(addr.s_addr);
    pool->pool_size     = range;
    pool->idle_tsc      = hz * IDLE_TIMEOUT_SEC;
    pool->started       = 0;

    // pre-fill public IPs for each outbound slot
    for (uint32_t i = 0; i < range; i++) {
        pool->entries[i].public_ip     = rte_cpu_to_be_32(pool->pool_start_ip + i);
        pool->entries[i].in_use        = 0;
        pool->entries[i].private_ip    = 0;
        pool->entries[i].last_seen_tsc = 0;
    }

    // outbound hash: private_ip -> slot index
    struct rte_hash_parameters out_params = {
        .name      = "nat_outbound_map",
        .entries   = range * 2,
        .key_len   = sizeof(uint32_t),
        .hash_func = rte_jhash,
        .socket_id = rte_socket_id(),
    };
    pool->outbound_map = rte_hash_create(&out_params);
    if (!pool->outbound_map)
        rte_exit(EXIT_FAILURE, "Failed to create outbound NAT hash\n");

    // inbound hash: public_ip -> private_ip (static)
    struct rte_hash_parameters in_params = {
        .name      = "nat_inbound_map",
        .entries   = MAX_INBOUND_ENTRIES,
        .key_len   = sizeof(uint32_t),
        .hash_func = rte_jhash,
        .socket_id = rte_socket_id(),
    };
    pool->inbound_map = rte_hash_create(&in_params);
    if (!pool->inbound_map)
        rte_exit(EXIT_FAILURE, "Failed to create inbound NAT hash\n");

    // timer 1: periodic expiry — fires every 1s, releases idle slots
    rte_timer_init(&pool->expiry_timer);
    rte_timer_reset(&pool->expiry_timer,
                    hz * EXPIRY_INTERVAL_SEC,
                    PERIODICAL,
                    rte_lcore_id(),
                    expiry_timer_cb,
                    pool);

    // timer 2: table delete — SINGLE, not armed yet
    // armed in main on first packet, reset on every packet
    rte_timer_init(&pool->table_timer);

    char end_ip_str[INET_ADDRSTRLEN];
    uint32_t end_ip_be = rte_cpu_to_be_32(pool->pool_start_ip + range - 1);
    inet_ntop(AF_INET, &end_ip_be, end_ip_str, sizeof(end_ip_str));
    printf("NAT outbound pool created: %s - %s (%u IPs)\n", start_ip, end_ip_str, range);
    printf("  idle slot timeout  : %ds\n", IDLE_TIMEOUT_SEC);
    printf("  table reset timeout: %ds no traffic\n", TABLE_TIMEOUT_SEC);

    return pool;
}

// ----------------------------------------------------------------
// timer 1 callback — fires every 1s, releases idle slots
// ----------------------------------------------------------------
static void expiry_timer_cb(struct rte_timer *tim __rte_unused, void *arg)
{
    struct nat_pool *pool = arg;
    uint64_t now = rte_get_timer_cycles();

    for (uint32_t i = 0; i < pool->pool_size; i++) {
        if (!pool->entries[i].in_use)
            continue;

        if ((now - pool->entries[i].last_seen_tsc) >= pool->idle_tsc) {
            char priv_str[INET_ADDRSTRLEN], pub_str[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &pool->entries[i].private_ip, priv_str, sizeof(priv_str));
            inet_ntop(AF_INET, &pool->entries[i].public_ip,  pub_str,  sizeof(pub_str));
            printf("[expiry_timer] idle slot released: %s -> %s\n", priv_str, pub_str);

            rte_hash_del_key(pool->outbound_map, &pool->entries[i].private_ip);
            pool->entries[i].in_use        = 0;
            pool->entries[i].private_ip    = 0;
            pool->entries[i].last_seen_tsc = 0;
        }
    }
}

// timer 2 callback — fires after 10s consecutive no traffic
static void table_timer_cb(struct rte_timer *tim __rte_unused, void *arg)
{
    struct nat_pool *pool = arg;

    printf("[table_timer] No packets for %ds. Resetting entire NAT table.\n",
           TABLE_TIMEOUT_SEC);

    nat_pool_dump(pool);

    // clear all outbound entries
    for (uint32_t i = 0; i < pool->pool_size; i++) {
        pool->entries[i].in_use        = 0;
        pool->entries[i].private_ip    = 0;
        pool->entries[i].last_seen_tsc = 0;
    }

    rte_hash_reset(pool->outbound_map);
    pool->started = 0;

    printf("[table_timer] NAT table cleared. Waiting for next packet.\n");
}

void nat_table_timer_arm(struct nat_pool *pool, uint64_t hz, uint32_t timeout_sec)
{
    rte_timer_reset(&pool->table_timer,
                    hz * timeout_sec,
                    SINGLE,
                    rte_lcore_id(),
                    table_timer_cb,
                    pool);
}

// reset entire pool externally if needed
void nat_pool_reset(struct nat_pool *pool)
{
    rte_timer_stop(&pool->table_timer);

    for (uint32_t i = 0; i < pool->pool_size; i++) {
        pool->entries[i].in_use        = 0;
        pool->entries[i].private_ip    = 0;
        pool->entries[i].last_seen_tsc = 0;
    }

    rte_hash_reset(pool->outbound_map);
    pool->started = 0;
}

// inbound: add single static mapping
void nat_inbound_add(struct nat_pool *pool, const char *public_ip, const char *private_ip)
{
    struct nat_inbound_entry *entry = malloc(sizeof(struct nat_inbound_entry));
    if (!entry)
        rte_exit(EXIT_FAILURE, "Failed to allocate inbound NAT entry\n");

    inet_pton(AF_INET, public_ip,  &entry->public_ip);
    inet_pton(AF_INET, private_ip, &entry->private_ip);

    int ret = rte_hash_add_key_data(pool->inbound_map, &entry->public_ip, entry);
    if (ret < 0)
        rte_exit(EXIT_FAILURE, "Failed to add inbound mapping %s -> %s\n",
                 public_ip, private_ip);

    printf("Inbound mapping added: %s -> %s\n", public_ip, private_ip);
}

// inbound: load from file
void nat_inbound_load_from_file(struct nat_pool *pool, const char *filepath)
{
    FILE *f = fopen(filepath, "r");
    if (f == NULL) {
        printf("Error: could not open inbound NAT config: %s\n", filepath);
        return;
    }

    char line[MAX_LINE];
    char public_ip[32];
    char private_ip[32];
    int  count = 0;

    while (fgets(line, sizeof(line), f) != NULL) {
        if (line[0] == '\n' || line[0] == '#')
            continue;

        if (sscanf(line, "%31s %31s", public_ip, private_ip) != 2) {
            printf("Skipping invalid line: %s", line);
            continue;
        }

        nat_inbound_add(pool, public_ip, private_ip);
        count++;
    }

    fclose(f);
    printf("Loaded %d inbound mappings from %s\n", count, filepath);
}

// find free outbound slot — pure scan, expiry_timer handles idle release
static int find_free_slot(struct nat_pool *pool)
{
    for (uint32_t i = 0; i < pool->pool_size; i++) {
        if (!pool->entries[i].in_use)
            return (int)i;
    }
    return -1;
}

// rewrite IP + recalculate checksums
static void rewrite_and_recalc(struct rte_ipv4_hdr *ip, uint32_t new_ip, int is_src)
{
    if (is_src)
        ip->src_addr = new_ip;
    else
        ip->dst_addr = new_ip;

    ip->hdr_checksum = 0;
    ip->hdr_checksum = rte_ipv4_cksum(ip);

    if (ip->next_proto_id == IPPROTO_TCP) {
        struct rte_tcp_hdr *tcp = (struct rte_tcp_hdr *)(ip + 1);
        tcp->cksum = 0;
        tcp->cksum = rte_ipv4_udptcp_cksum(ip, tcp);
    } else if (ip->next_proto_id == IPPROTO_UDP) {
        struct rte_udp_hdr *udp = (struct rte_udp_hdr *)(ip + 1);
        udp->dgram_cksum = 0;
        udp->dgram_cksum = rte_ipv4_udptcp_cksum(ip, udp);
    }
}

// apply NAT — no timer logic here, all timer handling is in main
void nat_pool_apply(struct nat_pool *pool, struct rte_mbuf **bufs, uint16_t nb_rx)
{
    for (uint16_t i = 0; i < nb_rx; i++) {
        struct rte_ether_hdr *eth = rte_pktmbuf_mtod(bufs[i], struct rte_ether_hdr *);

        if (eth->ether_type != rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4))
            continue;

        struct rte_ipv4_hdr *ip = (struct rte_ipv4_hdr *)(eth + 1);
        void *data = NULL;

        // -- INBOUND: dst IP in static inbound map → translate, else drop --
        if (rte_hash_lookup_data(pool->inbound_map, &ip->dst_addr, &data) >= 0) {
            struct nat_inbound_entry *entry = data;

            char pub_str[INET_ADDRSTRLEN], priv_str[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &ip->dst_addr,      pub_str,  sizeof(pub_str));
            inet_ntop(AF_INET, &entry->private_ip, priv_str, sizeof(priv_str));
            printf("Inbound NAT: %s -> %s\n", pub_str, priv_str);

            rewrite_and_recalc(ip, entry->private_ip, 0);
            continue;
        }

        // -- OUTBOUND: existing mapping → reuse --
        data = NULL;
        if (rte_hash_lookup_data(pool->outbound_map, &ip->src_addr, &data) >= 0) {
            int slot = (int)(uintptr_t)data;
            pool->entries[slot].last_seen_tsc = rte_get_timer_cycles();
            rewrite_and_recalc(ip, pool->entries[slot].public_ip, 1);
            continue;
        }

        // -- OUTBOUND: new src IP → assign from pool --
        int slot = find_free_slot(pool);

        if (slot < 0) {
            char src_str[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &ip->src_addr, src_str, sizeof(src_str));
            printf("WARNING: NAT pool exhausted! Dropping packet from %s\n", src_str);
            rte_pktmbuf_free(bufs[i]);
            bufs[i] = NULL;
            continue;
        }

        pool->entries[slot].private_ip    = ip->src_addr;
        pool->entries[slot].in_use        = 1;
        pool->entries[slot].last_seen_tsc = rte_get_timer_cycles();

        rte_hash_add_key_data(pool->outbound_map, &ip->src_addr, (void *)(uintptr_t)slot);

        char priv_str[INET_ADDRSTRLEN], pub_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &ip->src_addr,                 priv_str, sizeof(priv_str));
        inet_ntop(AF_INET, &pool->entries[slot].public_ip, pub_str,  sizeof(pub_str));
        printf("Outbound NAT assigned: %s -> %s\n", priv_str, pub_str);

        rewrite_and_recalc(ip, pool->entries[slot].public_ip, 1);
    }
}

// dump all active mappings
void nat_pool_dump(struct nat_pool *pool)
{
    printf("\n--- Inbound NAT Map (static) ---\n");
    const void *key;
    void *data;
    uint32_t iter = 0;
    while (rte_hash_iterate(pool->inbound_map, &key, &data, &iter) >= 0) {
        struct nat_inbound_entry *e = data;
        char pub_str[INET_ADDRSTRLEN], priv_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &e->public_ip,  pub_str,  sizeof(pub_str));
        inet_ntop(AF_INET, &e->private_ip, priv_str, sizeof(priv_str));
        printf("  %s -> %s\n", pub_str, priv_str);
    }

    printf("\n--- Outbound NAT Pool (dynamic) ---\n");
    uint32_t used = 0;
    for (uint32_t i = 0; i < pool->pool_size; i++)
        if (pool->entries[i].in_use) used++;

    printf("Pool: %u slots | Used: %u | Free: %u\n",
           pool->pool_size, used, pool->pool_size - used);

    for (uint32_t i = 0; i < pool->pool_size; i++) {
        if (!pool->entries[i].in_use)
            continue;
        char priv_str[INET_ADDRSTRLEN], pub_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &pool->entries[i].private_ip, priv_str, sizeof(priv_str));
        inet_ntop(AF_INET, &pool->entries[i].public_ip,  pub_str,  sizeof(pub_str));
        printf("  [%u] %s -> %s\n", i, priv_str, pub_str);
    }
    printf("-----------------------------------\n");
}
