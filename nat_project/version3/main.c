#include "source.h"
// #include "static_nat.h"
#include <rte_cycles.h>
#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_lcore.h>
#include <rte_mbuf.h>
#include <stdio.h>

#define BURST_SIZE 32
#define port_id 0

char *nat_config_file = "nat_config.txt";
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

  // rte hash init for IP tracking
  struct rte_hash *ht = ip_hash_create();

  // static NAT table init
  struct rte_hash *nat = nat_table_create();

  // add static 1:1 NAT mappings here
  //   nat_add_mapping(nat, "172.30.0.1", "203.0.113.5");
  //   nat_add_mapping(nat, "192.168.1.10", "203.0.113.5");
  //   nat_add_mapping(nat, "192.168.1.11", "203.0.113.6");

  int option;
  printf("Enter 1 to LOAD Nat mapping from config file and 2 to Manually enter "
         "data\n");
  scanf("%d", &option);
  if (option == 1) {
    // load NAT mappings from file
    nat_load_from_file(nat, nat_config_file);
  } else {
    // Manually add NAT mappings
    char private_ip[32];
    char public_ip[32];
    while (1) {
      printf("Enter private IP (or 'exit' to finish): ");
      scanf("%31s", private_ip);
      if (strcmp(private_ip, "exit") == 0)
        break;

      printf("Enter public IP: ");
      scanf("%31s", public_ip);

      nat_add_mapping(nat, private_ip, public_ip);
    }
  }
  struct rte_mbuf *bufs[BURST_SIZE];

  while (1) {
    uint16_t nb_rx = rte_eth_rx_burst(port_id, 0, bufs, BURST_SIZE);

    if (nb_rx > 0) {
      if (!started) {
        // first packet — start timer
        start_tsc = rte_get_timer_cycles();
        started = 1;
        printf("First packet received. Timer started.\n");
      }

      // process packets and store IPs in hash table
      hash_ips(ht, bufs, nb_rx);

      // apply static NAT — rewrites src IP before tx
      nat_apply(nat, bufs, nb_rx);

      // always update last_rx on any packet
      last_rx_tsc = rte_get_timer_cycles();

      // elapsed keeps going from start_tsc, not reset on each packet
      uint64_t elapsed = (rte_get_timer_cycles() - start_tsc) / hz;

      printf("Received %u packets | Time: %lu sec\n", nb_rx, elapsed);

      uint16_t nb_tx = rte_eth_tx_burst(port_id, 0, bufs, nb_rx);
      if (nb_tx < nb_rx) {
        for (int i = nb_tx; i < nb_rx; i++)
          rte_pktmbuf_free(bufs[i]);
      }

    } else {
      // no packet — check idle time against last_rx, not start_tsc
      if (started) {
        uint64_t idle = (rte_get_timer_cycles() - last_rx_tsc) / hz;

        if (idle >= 5) {
          printf("No packets for 5 seconds. Timer reset.\n");

          start_tsc = 0;
          last_rx_tsc = 0;
          started = 0;

          // print all stored IPs before reset
          hash_ips_dump(ht);

          // dump NAT table
          nat_dump(nat);

          // reset IP hash table
          reset_ip_hash(ht);
        }
      }
    }
  }

  return 0;
}