#include <rte_ethdev.h>
#include <rte_mempool.h>
#include <stdio.h>

void init_port(uint16_t port_id, struct rte_mempool *mp) {
  struct rte_eth_conf port_conf = {0};

  if (rte_eth_dev_configure(port_id, 1, 1, &port_conf) < 0)
    rte_exit(EXIT_FAILURE, "Port configure failed\n");

  if (rte_eth_rx_queue_setup(port_id, 0, 1024, rte_eth_dev_socket_id(port_id),
                             NULL, mp) < 0)
    rte_exit(EXIT_FAILURE, "RX queue setup failed\n");

  if (rte_eth_tx_queue_setup(port_id, 0, 1024, rte_eth_dev_socket_id(port_id),
                             NULL) < 0)
    rte_exit(EXIT_FAILURE, "TX queue setup failed\n");

  if (rte_eth_dev_start(port_id) < 0)
    rte_exit(EXIT_FAILURE, "Port start failed\n");

  printf("Port %u initialized and started.\n", port_id);
  struct rte_eth_dev_info info;
  rte_eth_dev_info_get(0, &info);
  printf("Driver: %s\n", info.driver_name);
}
