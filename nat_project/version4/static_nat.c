#include "source.h"

#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>

#include <rte_ether.h>
#include <rte_ip.h>
#include <rte_tcp.h>
#include <rte_udp.h>
#include <rte_hash.h>
#include <rte_jhash.h>
#include <rte_cycles.h>
#include <rte_byteorder.h>

// create the NAT pool
struct nat_pool *nat_pool_create(const char *start_ip, uint32_t range, uint64_t hz)
{
    struct nat_pool *pool = malloc(sizeof(struct nat_pool));
    if (!pool)
        rte_exit(EXIT_FAILURE, "Failed to allocate nat_pool\n");

    pool->entries = calloc(range, sizeof(struct nat_pool_entry));
    if (!pool->entries)
        rte_exit(EXIT_FAILURE, "Failed to allocate nat_pool entries\n");

    // convert start IP to host byte order and store
    struct in_addr addr;
    inet_pton(AF_INET, start_ip, &addr);
    pool->pool_start_ip = rte_be_to_cpu_32(addr.s_addr);
    pool->pool_size     = range;
    pool->idle_tsc      = hz * 5;  // 5 second idle timeout

    // pre-fill public IPs for each slot
    for (uint32_t i = 0; i < range; i++) {
        pool->entries[i].public_ip     = rte_cpu_to_be_32(pool->pool_start_ip + i);
        pool->entries[i].in_use        = 0;
        pool->entries[i].private_ip    = 0;
        pool->entries[i].last_seen_tsc = 0;
    }

    // hash table: private_ip (uint32_t) -> slot index
    struct rte_hash_parameters params = {
        .name      = "nat_pool_map",
        .entries   = range * 2,
        .key_len   = sizeof(uint32_t),
        .hash_func = rte_jhash,
        .socket_id = rte_socket_id(),
    };
    pool->map = rte_hash_create(&params);
    if (!pool->map)
        rte_exit(EXIT_FAILURE, "Failed to create NAT pool hash\n");

    char end_ip_str[INET_ADDRSTRLEN];
    uint32_t end_ip_be = rte_cpu_to_be_32(pool->pool_start_ip + range - 1);
    inet_ntop(AF_INET, &end_ip_be, end_ip_str, sizeof(end_ip_str));
    printf("NAT pool created: %s - %s (%u IPs)\n", start_ip, end_ip_str, range);

    return pool;
}

// single O(n) pass — finds free slot AND releases idle entries at the same time
static int find_free_slot(struct nat_pool *pool)
{
    uint64_t now = rte_get_timer_cycles();
    int free_slot = -1;

    for (uint32_t i = 0; i < pool->pool_size; i++) {
        if (!pool->entries[i].in_use) {
            // already free — grab the first one
            if (free_slot == -1)
                free_slot = (int)i;

        } else {
            // in use — check if idle timeout passed
            if ((now - pool->entries[i].last_seen_tsc) >= pool->idle_tsc) {
                char priv_str[INET_ADDRSTRLEN], pub_str[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &pool->entries[i].private_ip, priv_str, sizeof(priv_str));
                inet_ntop(AF_INET, &pool->entries[i].public_ip,  pub_str,  sizeof(pub_str));
                printf("NAT idle timeout: releasing %s -> %s back to pool\n", priv_str, pub_str);

                // remove from hash and free the slot
                rte_hash_del_key(pool->map, &pool->entries[i].private_ip);
                pool->entries[i].in_use        = 0;
                pool->entries[i].private_ip    = 0;
                pool->entries[i].last_seen_tsc = 0;

                // use this newly freed slot if we don't have one yet
                if (free_slot == -1)
                    free_slot = (int)i;
            }
        }
    }

    return free_slot;  // -1 only if all slots active and none expired
}

// apply dynamic NAT on all packets in burst
void nat_pool_apply(struct nat_pool *pool, struct rte_mbuf **bufs, uint16_t nb_rx)
{
    for (uint16_t i = 0; i < nb_rx; i++) {
        struct rte_ether_hdr *eth = rte_pktmbuf_mtod(bufs[i], struct rte_ether_hdr *);

        // skip non-IPv4
        if (eth->ether_type != rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4))
            continue;

        struct rte_ipv4_hdr *ip = (struct rte_ipv4_hdr *)(eth + 1);

        void *data = NULL;
        int ret = rte_hash_lookup_data(pool->map, &ip->src_addr, &data);

        if (ret >= 0) {
            // mapping exists — reuse and refresh idle timer
            int slot = (int)(uintptr_t)data;
            pool->entries[slot].last_seen_tsc = rte_get_timer_cycles();
            ip->src_addr = pool->entries[slot].public_ip;

        } else {
            // no mapping — find a free slot (also releases idle ones in same pass)
            int slot = find_free_slot(pool);

            if (slot < 0) {
                // pool fully exhausted — drop and warn
                char src_str[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &ip->src_addr, src_str, sizeof(src_str));
                printf("WARNING: NAT pool exhausted! Dropping packet from %s\n", src_str);
                rte_pktmbuf_free(bufs[i]);
                bufs[i] = NULL;
                continue;
            }

            // assign slot to this private IP
            pool->entries[slot].private_ip    = ip->src_addr;
            pool->entries[slot].in_use        = 1;
            pool->entries[slot].last_seen_tsc = rte_get_timer_cycles();

            rte_hash_add_key_data(pool->map, &ip->src_addr, (void *)(uintptr_t)slot);

            char priv_str[INET_ADDRSTRLEN], pub_str[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &ip->src_addr,                 priv_str, sizeof(priv_str));
            inet_ntop(AF_INET, &pool->entries[slot].public_ip, pub_str,  sizeof(pub_str));
            printf("NAT assigned: %s -> %s\n", priv_str, pub_str);

            ip->src_addr = pool->entries[slot].public_ip;
        }

        // recalculate IP checksum
        ip->hdr_checksum = 0;
        ip->hdr_checksum = rte_ipv4_cksum(ip);

        // recalculate L4 checksum
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
}

// dump all active mappings
void nat_pool_dump(struct nat_pool *pool)
{
    printf("\n--- NAT Pool Mappings ---\n");

    uint32_t used = 0;
    for (uint32_t i = 0; i < pool->pool_size; i++)
        if (pool->entries[i].in_use) used++;

    printf("Pool size: %u | Used: %u | Free: %u\n",
           pool->pool_size, used, pool->pool_size - used);

    for (uint32_t i = 0; i < pool->pool_size; i++) {
        if (!pool->entries[i].in_use)
            continue;
        char priv_str[INET_ADDRSTRLEN], pub_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &pool->entries[i].private_ip, priv_str, sizeof(priv_str));
        inet_ntop(AF_INET, &pool->entries[i].public_ip,  pub_str,  sizeof(pub_str));
        printf("  [%u] %s -> %s\n", i, priv_str, pub_str);
    }
    printf("-------------------------\n");
}