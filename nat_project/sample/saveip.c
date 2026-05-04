#include <stdio.h>
#include <stdint.h>

#include <rte_ether.h>
#include <rte_ip.h>
#include <rte_hash.h>
#include <rte_jhash.h>
#include <rte_mbuf.h>
#include <rte_byteorder.h>

#include "source.h"

#define MAX_IPS 1024

/* 🔹 Create hash table */
struct rte_hash *ip_hash_create(void)
{
    struct rte_hash_parameters params = {
        .name       = "ip_table",
        .entries    = MAX_IPS,
        .key_len    = sizeof(uint32_t),
        .hash_func  = rte_jhash,
        .socket_id  = rte_socket_id(),
    };

    struct rte_hash *ht = rte_hash_create(&params);
    if (ht == NULL) {
        rte_exit(EXIT_FAILURE, "Failed to create IP hash table\n");
    }

    return ht;
}

/* 🔹 Helper: print IP (host order) */
static inline void print_ip(uint32_t ip_be)
{
    uint32_t ip = rte_be_to_cpu_32(ip_be);

    printf("%u.%u.%u.%u",
           (ip >> 24) & 0xff,
           (ip >> 16) & 0xff,
           (ip >> 8)  & 0xff,
           (ip >> 0)  & 0xff);
}

/* 🔹 Process packets */
void hash_ips(struct rte_hash *ht, struct rte_mbuf **bufs, uint16_t nb_rx)
{
    for (uint16_t i = 0; i < nb_rx; i++) {

        struct rte_ether_hdr *eth =
            rte_pktmbuf_mtod(bufs[i], struct rte_ether_hdr *);

        /* skip non-IPv4 */
        if (eth->ether_type != rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4))
            continue;

        struct rte_ipv4_hdr *ip = (struct rte_ipv4_hdr *)(eth + 1);

        /* 🔹 SRC IP */
        uint32_t src = ip->src_addr;

        int ret = rte_hash_lookup(ht, &src);
        if (ret < 0) {
            rte_hash_add_key_data(ht, &src, (void *)(uintptr_t)src);

            printf("New SRC IP added: ");
            print_ip(src);
            printf("\n");
        } else {
            printf("SRC IP already exists: ");
            print_ip(src);
            printf("\n");
        }

        /* 🔹 DST IP */
        uint32_t dst = ip->dst_addr;

        ret = rte_hash_lookup(ht, &dst);
        if (ret < 0) {
            rte_hash_add_key_data(ht, &dst, (void *)(uintptr_t)dst);

            printf("New DST IP added: ");
            print_ip(dst);
            printf("\n");
        } else {
            printf("DST IP already exists: ");
            print_ip(dst);
            printf("\n");
        }
    }
}

/* 🔹 Dump all stored IPs */
void hash_ips_dump(struct rte_hash *ht)
{
    const void *key;
    void *data;
    uint32_t iter = 0;

    printf("\n--- Stored IPs ---\n");

    while (rte_hash_iterate(ht, &key, &data, &iter) >= 0) {
        uint32_t ip = (uint32_t)(uintptr_t)data;

        print_ip(ip);
        printf("\n");
    }

    printf("------------------\n");
}