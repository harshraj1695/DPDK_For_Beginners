#include <rte_byteorder.h>
#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_ether.h>
#include <rte_ip.h>
#include <rte_mbuf.h>
#include <rte_udp.h>
#include <stdio.h>
#include <string.h>

#define MBUF_COUNT 1024
uint16_t port_id = 0;
static void build_udp_packet(struct rte_mbuf *m) {
  const char *payload = "Hello from DPDK AF_PACKET!";
  uint16_t payload_len = strlen(payload);

  // Pointers to headers
  struct rte_ether_hdr *eth;
  struct rte_ipv4_hdr *ip;
  struct rte_udp_hdr *udp;

  // Set total packet length
  uint16_t pkt_len = sizeof(*eth) + sizeof(*ip) + sizeof(*udp) + payload_len;
  m->data_len = pkt_len;
  m->pkt_len = pkt_len;

  // Start building at packet headroom
  eth = rte_pktmbuf_mtod(m, struct rte_ether_hdr *);
  ip = (struct rte_ipv4_hdr *)(eth + 1);
  udp = (struct rte_udp_hdr *)(ip + 1);

  /* ---------------- ETH HEADER ---------------- */
  struct rte_ether_addr dst = {
      {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}}; // broadcast
  struct rte_ether_addr src = {
      {0x02, 0x00, 0x00, 0x00, 0x00, 0x01}}; // fake MAC

  rte_ether_addr_copy(&dst, &eth->dst_addr);
  rte_ether_addr_copy(&src, &eth->src_addr);

  eth->ether_type = rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4);

  /* ---------------- IP HEADER ---------------- */
  ip->version_ihl = 0x45; // IPv4, header = 20 bytes
  ip->type_of_service = 0;
  ip->total_length = rte_cpu_to_be_16(sizeof(*ip) + sizeof(*udp) + payload_len);
  ip->packet_id = 0;
  ip->fragment_offset = 0;
  ip->time_to_live = 64;
  ip->next_proto_id = IPPROTO_UDP;

  ip->src_addr = rte_cpu_to_be_32(0x0A000001); // 10.0.0.1
  ip->dst_addr = rte_cpu_to_be_32(0x0A000002); // 10.0.0.2

  ip->hdr_checksum = 0;
  ip->hdr_checksum = rte_ipv4_cksum(ip);

  /* ---------------- UDP HEADER ---------------- */
  udp->src_port = rte_cpu_to_be_16(1234);
  udp->dst_port = rte_cpu_to_be_16(5678);
  udp->dgram_len = rte_cpu_to_be_16(sizeof(*udp) + payload_len);
  udp->dgram_cksum = 0; // optional

  /* ---------------- PAYLOAD ---------------- */
  char *data = (char *)(udp + 1);
  memcpy(data, payload, payload_len);
}

int main(int argc, char **argv) {
  int ret = rte_eal_init(argc, argv);
  if (ret < 0)
    rte_exit(EXIT_FAILURE, "EAL init failed\n");

  /* Create a mempool */
  struct rte_mempool *mp =
      rte_pktmbuf_pool_create("MBUF_POOL", MBUF_COUNT, 0, 0,
                              RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());

  if (!mp)
    rte_exit(EXIT_FAILURE, "Cannot create mempool\n");

  /* Configure port 0 (AF_PACKET PMD) */
  // uint16_t port_id = 0;

  if (rte_eth_dev_configure(port_id, 1, 1, NULL) < 0)
    rte_exit(EXIT_FAILURE, "Port configure failed\n");

  if (rte_eth_rx_queue_setup(port_id, 0, 128, rte_eth_dev_socket_id(port_id),
                             NULL, mp) < 0)
    rte_exit(EXIT_FAILURE, "RX queue setup failed\n");

  if (rte_eth_tx_queue_setup(port_id, 0, 128, rte_eth_dev_socket_id(port_id),
                             NULL) < 0)
    rte_exit(EXIT_FAILURE, "TX queue setup failed\n");

  if (rte_eth_dev_start(port_id) < 0)
    rte_exit(EXIT_FAILURE, "Port start failed\n");
  struct rte_eth_dev_info info;
  rte_eth_dev_info_get(0, &info);
  printf("Driver: %s\n", info.driver_name);

  /* Allocate an mbuf */
  struct rte_mbuf *m = rte_pktmbuf_alloc(mp);
  if (!m)
    rte_exit(EXIT_FAILURE, "mbuf alloc failed\n");

  /* Build UDP packet */
  build_udp_packet(m);

  printf("Transmitting packet...\n");

  /* Transmit it */
  uint16_t sent = rte_eth_tx_burst(port_id, 0, &m, 1);
  if (sent == 0) {
    printf("Failed to send\n");
    rte_pktmbuf_free(m);
  } else {
    printf("Packet sent!\n");
  }

  return 0;
}
