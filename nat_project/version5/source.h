#ifndef IP_HASH_H
#define IP_HASH_H

#include <stdint.h>
#include <rte_hash.h>
#include <rte_mbuf.h>
#include <rte_timer.h>


/* call once at startup to create the hash table */
struct rte_hash *ip_hash_create(void);

/* pass all received mbufs — stores new IPs, skips duplicates */
void hash_ips(struct rte_hash *ht, struct rte_mbuf **bufs, uint16_t nb_rx);

/* print all stored IPs */
void hash_ips_dump(struct rte_hash *ht);

// reset hash table 
void reset_ip_hash(struct rte_hash *ht);

#endif
/// outbound entry: private_ip -> public_ip (dynamic from pool)
struct nat_pool_entry {
    uint32_t private_ip;     // network byte order
    uint32_t public_ip;      // network byte order
    uint64_t last_seen_tsc;  // tsc of last packet seen
    int      in_use;         // 1 = assigned, 0 = free
};
 
// inbound entry: public_ip -> private_ip (static, loaded from file)
struct nat_inbound_entry {
    uint32_t public_ip;   // network byte order
    uint32_t private_ip;  // network byte order
};
 
// the NAT pool
struct nat_pool {
    // outbound
    struct nat_pool_entry *entries;       // array of pool_size slots
    uint32_t               pool_start_ip; // first public IP (host byte order)
    uint32_t               pool_size;     // total IPs in pool
    uint64_t               idle_tsc;      // tsc ticks for 5s idle timeout
    struct rte_hash       *outbound_map;  // private_ip -> slot index
 
    // inbound
    struct rte_hash       *inbound_map;   // public_ip -> private_ip (static)
 
    // timers
    struct rte_timer       expiry_timer;  // periodic: releases idle slots every 1s
    struct rte_timer       table_timer;   // single: deletes whole table after 10s no traffic
    int                    started;       // 1 = first packet seen, 0 = waiting
};
 
// create outbound pool: start_ip e.g "203.0.113.1", range = number of IPs
struct nat_pool *nat_pool_create(const char *start_ip, uint32_t range, uint64_t hz);
 
// add a single static inbound mapping: public_ip -> private_ip
void nat_inbound_add(struct nat_pool *pool, const char *public_ip, const char *private_ip);
 
// load inbound mappings from file
// file format: public_ip private_ip (one per line)
void nat_inbound_load_from_file(struct nat_pool *pool, const char *filepath);
 
// apply NAT on burst — no timer logic here, handled in main
void nat_pool_apply(struct nat_pool *pool, struct rte_mbuf **bufs, uint16_t nb_rx);
 
// dump all active mappings
void nat_pool_dump(struct nat_pool *pool);
 
// reset entire outbound pool
void nat_pool_reset(struct nat_pool *pool);
 
// arms/resets the "no traffic" table timer (called on every packet burst)
void nat_table_timer_arm(struct nat_pool *pool, uint64_t hz, uint32_t timeout_sec);
 
 
// reads start_ip and range from txt file and returns a nat_pool
// file format: 203.0.113.1 5
struct nat_pool *nat_load_from_file(const char *filepath, uint64_t hz);


struct rte_mempool *create_mempool(void);
void init_port(uint16_t port, struct rte_mempool *mp);
