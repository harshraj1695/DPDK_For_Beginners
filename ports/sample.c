#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_lcore.h>
#include <stdint.h>
#include <stdio.h>

int main(int argc, char **argv) {
  rte_eal_init(argc, argv);
  printf("DPDK EAL initialized successfully.\n");

  // No of ports available
  uint16_t nb_ports = rte_eth_dev_count_avail();
  printf("Found %u ports\n", nb_ports);
}