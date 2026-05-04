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

#endif


struct rte_mempool *create_mempool(void);
 void init_port(uint16_t port, struct rte_mempool *mp);