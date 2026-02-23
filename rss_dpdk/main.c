#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_lcore.h>
#include <rte_mbuf.h>

#define RX_RING 1024
#define TX_RING 1024
#define NUM_MBUFS 8191
#define MBUF_CACHE 250
#define BURST 32
#define RX_QUEUES 4

// what this program will do
// 1. Initialize DPDK and configure a single port with multiple RX queues
// 2. Enable RSS to distribute incoming packets across RX queues
// 3. Launch a separate RX loop on each lcore to poll its assigned queue


// note -> this will not run with af_packet, you need to run it with a DPDK-compatible NIC 
// and driver (e.g., igb_uio or vfio-pci) and bind the NIC to DPDK using dpdk-devbind.py script.
// because af_packet has only one RX queue and does not support RSS.


static const uint16_t port_id = 0;
struct rte_mempool *mbuf_pool;

// RX loop for each lcore
static int lcore_rx(void *arg) {
  uint16_t queue_id = (uintptr_t)arg;
  struct rte_mbuf *bufs[BURST];

  printf("Lcore %u polling queue %u\n", rte_lcore_id(), queue_id);

  while (1) {
    uint16_t nb = rte_eth_rx_burst(port_id, queue_id, bufs, BURST);

    if (nb == 0)
      continue;

    for (int i = 0; i < nb; i++) {
      printf("Packet on queue %u (lcore %u)\n", queue_id, rte_lcore_id());
      rte_pktmbuf_free(bufs[i]);
    }
  }
  return 0;
}

int main(int argc, char **argv) {
  int ret = rte_eal_init(argc, argv);
  if (ret < 0)
    rte_exit(EXIT_FAILURE, "EAL init failed\n");

  uint16_t nb_ports = rte_eth_dev_count_avail();
  if (nb_ports == 0)
    rte_exit(EXIT_FAILURE, "No ports found\n");

  mbuf_pool =
      rte_pktmbuf_pool_create("MBUF_POOL", NUM_MBUFS, MBUF_CACHE, 0,
                              RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());

  if (mbuf_pool == NULL)
    rte_exit(EXIT_FAILURE, "Cannot create mbuf pool\n");

  // Configure the Ethernet device
  struct rte_eth_conf port_conf = {0};

  port_conf.rxmode.mq_mode = RTE_ETH_MQ_RX_RSS;

  port_conf.rx_adv_conf.rss_conf.rss_hf =
      RTE_ETH_RSS_IP | RTE_ETH_RSS_TCP | RTE_ETH_RSS_UDP;

  // port_conf.rx_adv_conf.rss_conf.rss_key = NULL; // Use default RSS key
  ret = rte_eth_dev_configure(port_id, RX_QUEUES, 0, &port_conf);
  if (ret < 0)
    rte_exit(EXIT_FAILURE, "Port configure failed\n");

  // Setup RX queues
  for (int q = 0; q < RX_QUEUES; q++) {
    ret = rte_eth_rx_queue_setup(
        port_id, q, RX_RING, rte_eth_dev_socket_id(port_id), NULL, mbuf_pool);
    if (ret < 0)
      rte_exit(EXIT_FAILURE, "RX queue setup failed\n");
  }

  ret = rte_eth_dev_start(port_id);
  if (ret < 0)
    rte_exit(EXIT_FAILURE, "Port start failed\n");

  printf("RSS enabled with %d RX queues\n", RX_QUEUES);

  // Launch RX loop on each lcore
  uint16_t q = 0;
  RTE_LCORE_FOREACH_WORKER(ret) {
    if (q >= RX_QUEUES)
      break;

    rte_eal_remote_launch(lcore_rx, (void *)(uintptr_t)q, ret);
    q++;
  }


  // Main lcore can also handle RX if there are more queues than workers
  if (q < RX_QUEUES)
    lcore_rx((void *)(uintptr_t)q);

  rte_eal_mp_wait_lcore();
  return 0;
}