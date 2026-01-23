#include <inttypes.h>
#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_ip.h>
#include <rte_lpm.h>
#include <stdint.h>
#include <stdio.h>

#define NB_MBUF 8192
#define RX_RING_SIZE 1024
#define TX_RING_SIZE 1024
#define BURST_SIZE 32

struct rte_lpm *lpm_table;

#define IPv4(a, b, c, d)                                                       \
  ((uint32_t)((a & 0xFF) << 24 | (b & 0xFF) << 16 | (c & 0xFF) << 8 |          \
              (d & 0xFF)))

/* Add routes to LPM table */
static void init_lpm(void) {
  struct rte_lpm_config lpm_conf = {
      .max_rules = 1024, .number_tbl8s = 256, .flags = 0};

  lpm_table = rte_lpm_create("LPM_TABLE", rte_socket_id(), &lpm_conf);
  if (!lpm_table)
    rte_exit(EXIT_FAILURE, "Could not create LPM table\n");

  rte_lpm_add(lpm_table, IPv4(10, 1, 2, 0), 24, 1);
  rte_lpm_add(lpm_table, IPv4(10, 1, 0, 0), 16, 2);
  rte_lpm_add(lpm_table, IPv4(192, 0, 0, 0), 8, 3);
  rte_lpm_add(lpm_table, IPv4(172, 0, 0, 0), 4, 4);

  printf("LPM routes added successfully!\n");
}

/* Updated: now receives mbuf_pool as parameter */
static void init_port(uint16_t port, struct rte_mempool *mbuf_pool) {
  struct rte_eth_conf port_conf = {0};
  const uint16_t rx_rings = 1, tx_rings = 1;
  int ret;

  ret = rte_eth_dev_configure(port, rx_rings, tx_rings, &port_conf);
  if (ret < 0)
    rte_exit(EXIT_FAILURE, "Cannot configure device: err=%d\n", ret);

  /* Correct: use existing mbuf_pool, do NOT create inside */
  ret = rte_eth_rx_queue_setup(port, 0, RX_RING_SIZE,
                               rte_eth_dev_socket_id(port), NULL, mbuf_pool);
  if (ret < 0)
    rte_exit(EXIT_FAILURE, "RX queue setup failed\n");

  ret = rte_eth_tx_queue_setup(port, 0, TX_RING_SIZE,
                               rte_eth_dev_socket_id(port), NULL);
  if (ret < 0)
    rte_exit(EXIT_FAILURE, "TX queue setup failed\n");

  ret = rte_eth_dev_start(port);
  if (ret < 0)
    rte_exit(EXIT_FAILURE, "Device start failed\n");

  rte_eth_promiscuous_enable(port);

  printf("Port %u initialized.\n", port);
}

/* Processing loop */
static void main_loop(uint16_t port) {
  struct rte_mbuf *pkts[BURST_SIZE];

  while (1) {
    uint16_t nb_rx = rte_eth_rx_burst(port, 0, pkts, BURST_SIZE);
    if (nb_rx == 0)
      continue;

    for (int i = 0; i < nb_rx; i++) {
      struct rte_mbuf *m = pkts[i];

      struct rte_ether_hdr *eth = rte_pktmbuf_mtod(m, struct rte_ether_hdr *);

      if (eth->ether_type != rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4)) {
        printf("Non-IPv4 packet ignored\n");
        continue;
      }

      struct rte_ipv4_hdr *ip = rte_pktmbuf_mtod_offset(
          m, struct rte_ipv4_hdr *, sizeof(struct rte_ether_hdr));

      uint32_t dst_ip = rte_be_to_cpu_32(ip->dst_addr);

      uint32_t next_hop;
      if (rte_lpm_lookup(lpm_table, dst_ip, &next_hop) == 0) {
        printf("IP %u.%u.%u.%u → Next Hop %u\n", (dst_ip >> 24) & 0xFF,
               (dst_ip >> 16) & 0xFF, (dst_ip >> 8) & 0xFF, dst_ip & 0xFF,
               next_hop);
      } else {
        printf("No route for IP %u.%u.%u.%u\n", (dst_ip >> 24) & 0xFF,
               (dst_ip >> 16) & 0xFF, (dst_ip >> 8) & 0xFF, dst_ip & 0xFF);
      }

      rte_eth_tx_burst(port, 0, &m, 1);
    }
  }
}

int main(int argc, char **argv) {
  int ret = rte_eal_init(argc, argv);
  if (ret < 0)
    rte_exit(EXIT_FAILURE, "Failed to init EAL\n");

  struct rte_mempool *mbuf_pool = rte_pktmbuf_pool_create(
      "MBUF_POOL", NB_MBUF, 0, 0, RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());

  if (!mbuf_pool)
    rte_exit(EXIT_FAILURE, "Cannot create mbuf pool\n");

  uint16_t port = 0;

  init_lpm();
  init_port(port, mbuf_pool);

  printf("Starting main loop (LPM router)...\n");
  main_loop(port);

  return 0;
}
