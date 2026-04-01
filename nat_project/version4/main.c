#include "source.h"
#include <rte_cycles.h>
#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_lcore.h>
#include <rte_mbuf.h>
#include <stdio.h>

#define BURST_SIZE 32
#define port_id 0

#define NAT_POOL_START "203.0.113.1"
#define NAT_POOL_RANGE 5

int main(int argc, char *argv[]) {
  rte_eal_init(argc, argv);

  uint64_t hz = rte_get_timer_hz();

  // set once on first packet, never touched again until reset
  uint64_t start_tsc = 0;

  // set on every packet received, to track idle time
  uint64_t last_rx_tsc = 0;

  int started = 0;

  struct rte_mempool *mp = create_mempool();
  init_port(port_id, mp);

  // IP tracking hash — only for non-NAT IPs
  struct rte_hash *ht = ip_hash_create();

  // // dynamic NAT pool — 203.0.113.1 to 203.0.113.5
  // struct nat_pool *pool = nat_pool_create(NAT_POOL_START, NAT_POOL_RANGE,
  // hz);
  struct nat_pool *pool;
  int options = 0;
  printf("Enter 1 to load NAT pool from file, 0 to use default config or any "
         "other Number to use config file\n");
  scanf("%d", &options);
  if (options == 1) {
    // load NAT pool from file — reads start_ip and range from nat_config.txt
   pool = nat_load_from_file("nat_config.txt", hz);
  } else if (options == 0) {
    // default config
    pool = nat_pool_create(NAT_POOL_START, NAT_POOL_RANGE, hz);
  } else {
    // load NAT pool from file — reads start_ip and range from nat_config.txt
    pool = nat_load_from_file("nat_config.txt", hz);
  }
  struct rte_mbuf *bufs[BURST_SIZE];

  while (1) {
    uint16_t nb_rx = rte_eth_rx_burst(port_id, 0, bufs, BURST_SIZE);

    if (nb_rx > 0) {
      if (!started) {
        start_tsc = rte_get_timer_cycles();
        started = 1;
        printf("First packet received. Timer started.\n");
      }

      // store non-NAT IPs for monitoring
      // hash_ips(ht,bufs, nb_rx);

      // apply dynamic NAT — idle release happens inside find_free_slot
      nat_pool_apply(pool, bufs, nb_rx);

      last_rx_tsc = rte_get_timer_cycles();

      uint64_t elapsed = (rte_get_timer_cycles() - start_tsc) / hz;
      printf("Received %u packets | Time: %lu sec\n", nb_rx, elapsed);

      // tx — skip NULL bufs dropped by pool exhaustion
      for (int i = 0; i < nb_rx; i++) {
        if (bufs[i] == NULL)
          continue;
        rte_eth_tx_burst(port_id, 0, &bufs[i], 1);
      }

    } else {
      if (started) {
        uint64_t idle = (rte_get_timer_cycles() - last_rx_tsc) / hz;

        if (idle >= 15) {
          printf("No packets for 15 seconds. Timer reset.\n");

          start_tsc = 0;
          last_rx_tsc = 0;
          started = 0;

          // dump non-NAT IPs seen so far
          // hash_ips_dump(ht);

          // dump active NAT pool mappings
          nat_pool_dump(pool);

          // reset IP tracking hash
          reset_ip_hash(ht);
        }
      }
    }
  }

  return 0;
}