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

// single mapping entry: private ip -> public ip with idle tracking
struct nat_pool_entry {
    uint32_t private_ip;     // network byte order
    uint32_t public_ip;      // network byte order
    uint64_t last_seen_tsc;  // tsc of last packet seen for this mapping
    int      in_use;         // 1 = assigned, 0 = free
};
 
// the NAT pool
struct nat_pool {
    struct nat_pool_entry *entries;   // array of pool_size slots
    uint32_t               pool_start_ip;  // first public IP (host byte order)
    uint32_t               pool_size;      // total IPs in pool
    uint64_t               idle_tsc;       // tsc ticks for 5s idle timeout
    struct rte_hash       *map;            // private_ip -> slot index
};
 
// create pool: start_ip e.g "203.0.113.1", range = number of IPs
struct nat_pool *nat_pool_create(const char *start_ip, uint32_t range, uint64_t hz);
 
// apply dynamic NAT on burst — find_free_slot handles idle release internally
void nat_pool_apply(struct nat_pool *pool, struct rte_mbuf **bufs, uint16_t nb_rx);
 
// dump current active mappings
void nat_pool_dump(struct nat_pool *pool);
 
// dump all mappings
void nat_dump(struct rte_hash *ht);

 
// reads start_ip and range from txt file and returns a nat_pool
// file format: 203.0.113.1 5
struct nat_pool *nat_load_from_file(const char *filepath, uint64_t hz);


struct rte_mempool *create_mempool(void);
void init_port(uint16_t port, struct rte_mempool *mp);