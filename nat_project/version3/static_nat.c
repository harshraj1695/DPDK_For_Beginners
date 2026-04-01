#include "source.h"

#include <stdio.h>
#include <arpa/inet.h>

#include <rte_ether.h>
#include <rte_ip.h>
#include <rte_tcp.h>
#include <rte_udp.h>
#include <rte_hash.h>
#include <rte_jhash.h>
#include <rte_mbuf.h>
#include <rte_byteorder.h>

#define MAX_NAT_ENTRIES 1024

// create NAT hash table — key is private IP (uint32_t)
struct rte_hash *nat_table_create(void)
{
    struct rte_hash_parameters params = {
        .name      = "nat_table",
        .entries   = MAX_NAT_ENTRIES,
        .key_len   = sizeof(uint32_t),   // key = private IP
        .hash_func = rte_jhash,
        .socket_id = rte_socket_id(),
    };

    struct rte_hash *ht = rte_hash_create(&params);
    if (ht == NULL)
        rte_exit(EXIT_FAILURE, "Failed to create NAT hash table\n");

    return ht;
}

// add a 1:1 static mapping: private_ip -> public_ip
void nat_add_mapping(struct rte_hash *ht, const char *private_ip, const char *public_ip)
{
    struct nat_entry *entry = malloc(sizeof(struct nat_entry));
    if (entry == NULL)
        rte_exit(EXIT_FAILURE, "Failed to allocate NAT entry\n");

    // convert string IPs to network byte order uint32_t
    inet_pton(AF_INET, private_ip, &entry->private_ip);
    inet_pton(AF_INET, public_ip,  &entry->public_ip);

    int ret = rte_hash_add_key_data(ht, &entry->private_ip, entry);
    if (ret < 0)
        rte_exit(EXIT_FAILURE, "Failed to add NAT mapping %s -> %s\n", private_ip, public_ip);

    printf("NAT mapping added: %s -> %s\n", private_ip, public_ip);
}

// apply NAT on all packets in the burst
// rewrites src IP in-place and recalculates IP checksum
void nat_apply(struct rte_hash *ht, struct rte_mbuf **bufs, uint16_t nb_rx)
{
    for (uint16_t i = 0; i < nb_rx; i++) {
        struct rte_ether_hdr *eth = rte_pktmbuf_mtod(bufs[i], struct rte_ether_hdr *);

        // skip non-IPv4
        if (eth->ether_type != rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4))
            continue;

        struct rte_ipv4_hdr *ip = (struct rte_ipv4_hdr *)(eth + 1);

        // look up private src IP in NAT table
        struct nat_entry *entry = NULL;
        int ret = rte_hash_lookup_data(ht, &ip->src_addr, (void **)&entry);

        if (ret < 0)
            continue;  // no mapping for this IP — pass through unchanged

        // rewrite src IP to public IP
        ip->src_addr = entry->public_ip;

        // recalculate IP checksum
        ip->hdr_checksum = 0;
        ip->hdr_checksum = rte_ipv4_cksum(ip);

        // recalculate L4 checksum if TCP/UDP
        if (ip->next_proto_id == IPPROTO_TCP) {
            struct rte_tcp_hdr *tcp = (struct rte_tcp_hdr *)(ip + 1);
            tcp->cksum = 0;
            tcp->cksum = rte_ipv4_udptcp_cksum(ip, tcp);

        } else if (ip->next_proto_id == IPPROTO_UDP) {
            struct rte_udp_hdr *udp = (struct rte_udp_hdr *)(ip + 1);
            udp->dgram_cksum = 0;
            udp->dgram_cksum = rte_ipv4_udptcp_cksum(ip, udp);
        }

        char src_str[INET_ADDRSTRLEN], pub_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &entry->private_ip, src_str, sizeof(src_str));
        inet_ntop(AF_INET, &entry->public_ip,  pub_str, sizeof(pub_str));
        printf("NAT applied: %s -> %s\n", src_str, pub_str);
    }
}

// dump all stored NAT mappings
void nat_dump(struct rte_hash *ht)
{
    const void *key;
    void *data;
    uint32_t iter = 0;

    printf("\n--- NAT Table ---\n");

    while (rte_hash_iterate(ht, &key, &data, &iter) >= 0) {
        struct nat_entry *entry = data;

        char priv[INET_ADDRSTRLEN], pub[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &entry->private_ip, priv, sizeof(priv));
        inet_ntop(AF_INET, &entry->public_ip,  pub,  sizeof(pub));
        printf("  %s -> %s\n", priv, pub);
    }

    printf("-----------------\n");
}