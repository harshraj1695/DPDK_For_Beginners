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


// timeout for each NAT entry (seconds)
#define NAT_TIMEOUT_SEC 30

// port range for allocation
#define MAX_PORT 65535
#define MIN_PORT 1024


// key used for hash lookup
// used in both forward and reverse tables
struct nat_key {
    uint32_t src_ip;
    uint32_t dst_ip;
    uint16_t src_port;
    uint16_t dst_port;
    uint8_t  proto;
} __attribute__((packed));


// NAT entry storing mapping info
struct nat_entry {

    // key for outgoing lookup (private -> public)
    struct nat_key fwd_key;

    // key for incoming lookup (public -> private)
    struct nat_key rev_key;

    // original private client
    uint32_t private_ip;
    uint16_t private_port;

    // translated public address
    uint32_t public_ip;
    uint16_t public_port;

    // remote server (needed for reverse match)
    uint32_t remote_ip;
    uint16_t remote_port;

    // protocol (tcp/udp)
    uint8_t proto;

    // timer for expiry
    struct rte_timer timer;
};


// initialize NAT system
int nat_init(uint32_t public_ip);


// process one packet
// return 0 = forward
// return -1 = drop
int nat_process_packet(struct rte_mbuf *mbuf);


// run timer subsystem (call in main loop)
void nat_timer_manage(void);
// reads start_ip and range from txt file and returns a nat_pool
// file format: 203.0.113.1 5
struct nat_pool *nat_load_from_file(const char *filepath, uint64_t hz);


struct rte_mempool *create_mempool(void);
void init_port(uint16_t port, struct rte_mempool *mp);
