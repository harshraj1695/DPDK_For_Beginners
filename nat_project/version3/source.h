#ifndef IP_HASH_H
#define IP_HASH_H

#include <stdint.h>
#include <rte_hash.h>
#include <rte_mbuf.h>

/* call once at startup to create the hash table */
struct rte_hash *ip_hash_create(void);

/* pass all received mbufs — stores new IPs, skips duplicates */
void hash_ips(struct rte_hash *ht, struct rte_mbuf **bufs, uint16_t nb_rx);

/* print all stored IPs */
void hash_ips_dump(struct rte_hash *ht);

// reset hash table 
void reset_ip_hash(struct rte_hash *ht);

#endif

// NAT mapping stored as value in the hash table
struct nat_entry {
    uint32_t private_ip;  // original source IP (network byte order)
    uint32_t public_ip;   // mapped public IP   (network byte order)
};
 
// create the NAT hash table (key = private_ip uint32_t)
struct rte_hash *nat_table_create(void);
 
// add a static 1:1 mapping: private_ip -> public_ip
// both IPs passed as strings e.g. "192.168.1.10", "203.0.113.5"
void nat_add_mapping(struct rte_hash *ht, const char *private_ip, const char *public_ip);
 
// apply NAT on burst — rewrites src IP + recalculates checksum in-place
void nat_apply(struct rte_hash *ht, struct rte_mbuf **bufs, uint16_t nb_rx);
 
// dump all mappings
void nat_dump(struct rte_hash *ht);


// reads private_ip public_ip pairs from a txt file and adds to NAT table
// file format (one mapping per line):
// 192.168.1.10 203.0.113.5
// 192.168.1.11 203.0.113.6
void nat_load_from_file(struct rte_hash *nat, const char *filepath);


struct rte_mempool *create_mempool(void);
void init_port(uint16_t port, struct rte_mempool *mp);