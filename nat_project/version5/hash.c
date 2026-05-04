#include <stdio.h>
#include <stdint.h>

#include <rte_ether.h>
#include <rte_ip.h>
#include <rte_tcp.h>
#include <rte_udp.h>
#include <rte_hash.h>
#include <rte_jhash.h>
#include <rte_mbuf.h>
#include <rte_byteorder.h>

#include "source.h"

#define MAX_IPS 1024

// Key IP + PORT
struct ip_port_key {
    uint32_t ip;    
    uint16_t port;
} __attribute__((packed));

// Create hash table
struct rte_hash *ip_hash_create(void)
{
    struct rte_hash_parameters params = {
        .name       = "ip_port_table",
        .entries    = MAX_IPS,
        .key_len    = sizeof(struct ip_port_key),
        .hash_func  = rte_jhash,
        .socket_id  = rte_socket_id(),
    };

    struct rte_hash *ht = rte_hash_create(&params);
    if (ht == NULL) {
        rte_exit(EXIT_FAILURE, "Failed to create hash table\n");
    }

    return ht;
}

// Print IP:PORT
static inline void print_ip_port(uint32_t ip_be, uint16_t port_be)
{
    uint32_t ip = rte_be_to_cpu_32(ip_be);
    uint16_t port = rte_be_to_cpu_16(port_be);

    printf("%u.%u.%u.%u: \t%u",
           (ip >> 24) & 0xff,
           (ip >> 16) & 0xff,
           (ip >> 8)  & 0xff,
           (ip >> 0)  & 0xff,
           port);
}

// Process packets
void hash_ips(struct rte_hash *ht, struct rte_mbuf **bufs, uint16_t nb_rx)
{
    for (uint16_t i = 0; i < nb_rx; i++) {

        struct rte_ether_hdr *eth =
            rte_pktmbuf_mtod(bufs[i], struct rte_ether_hdr *);

        // skip non IPv4
        if (eth->ether_type != rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4))
            continue;

        struct rte_ipv4_hdr *ip = (struct rte_ipv4_hdr *)(eth + 1);

        uint16_t src_port = 0;

        // extract L4 ports
        if (ip->next_proto_id == IPPROTO_TCP) {
            struct rte_tcp_hdr *tcp = (struct rte_tcp_hdr *)(ip + 1);
            src_port = tcp->src_port;

        } else if (ip->next_proto_id == IPPROTO_UDP) {
            struct rte_udp_hdr *udp = (struct rte_udp_hdr *)(ip + 1);
            src_port = udp->src_port;
        } else {
            continue;
        }

        // SRC key
        struct ip_port_key src_key = {
            .ip = ip->src_addr,
            .port = src_port
        };

        int ret = rte_hash_lookup(ht, &src_key);
        if (ret < 0) {
            // store dummy data (safe pattern)
            rte_hash_add_key_data(ht, &src_key, (void *)(uintptr_t)1);

            printf("New SRC: ");
            print_ip_port(src_key.ip, src_key.port);
            printf("\n");
        } else {
            printf("SRC exists: ");
            print_ip_port(src_key.ip, src_key.port);
            printf("\n");
        }

        //  DST key
        // struct ip_port_key dst_key = {
        //     .ip = ip->dst_addr,
        //     .port = dst_port
        // };

        // ret = rte_hash_lookup(ht, &dst_key);
        // if (ret < 0) {
        //     rte_hash_add_key_data(ht, &dst_key, (void *)(uintptr_t)1);

        //     printf("New DST: ");
        //     print_ip_port(dst_key.ip, dst_key.port);
        //     printf("\n");
        // } else {
        //     printf("DST exists: ");
        //     print_ip_port(dst_key.ip, dst_key.port);
        //     printf("\n");
        // }
    }
}

// Dump all stored entries
void hash_ips_dump(struct rte_hash *ht)
{
    const void *key;
    void *data;
    uint32_t iter = 0;

    printf("\n--- Stored IP:PORT ---\n");

    while (rte_hash_iterate(ht, &key, &data, &iter) >= 0) {
        const struct ip_port_key *k = key;

        print_ip_port(k->ip, k->port);
        printf("\n");
    }

    printf("----------------------\n");
}

// Reset hash table
void reset_ip_hash(struct rte_hash *ht)
{
    if (ht == NULL)
        return;

    rte_hash_reset(ht);
}