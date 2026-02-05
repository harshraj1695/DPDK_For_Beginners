#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_mbuf.h>
#include <rte_mempool.h>
#include <stdio.h>
#include <stdlib.h>

int port_id = 0;

static void parser(struct rte_mbuf *mbuf) {
  uint16_t *pkt = rte_pktmbuf_mtod(mbuf, uint16_t *);
  uint16_t offset = 0;
  printf("packet length = %u\n", mbuf->pkt_len);
  struct rte_ether_hdr *eth = (struct rte_ether_hdr *)pkt;

  printf("L2: Ethernet\n");
  printf("   SRC MAC: %02x:%02x:%02x:%02x:%02x:%02x\n",
         eth->src_addr.addr_bytes[0], eth->src_addr.addr_bytes[1],
         eth->src_addr.addr_bytes[2], eth->src_addr.addr_bytes[3],
         eth->src_addr.addr_bytes[4], eth->src_addr.addr_bytes[5]);

  printf("   DST MAC: %02x:%02x:%02x:%02x:%02x:%02x\n",
         eth->dst_addr.addr_bytes[0], eth->dst_addr.addr_bytes[1],
         eth->dst_addr.addr_bytes[2], eth->dst_addr.addr_bytes[3],
         eth->dst_addr.addr_bytes[4], eth->dst_addr.addr_bytes[5]);

  uint16_t eth_type = rte_be_to_cpu_16(eth->ether_type);
  offset = sizeof(struct rte_ether_hdr);
}



// packet operations

void operations(struct rte_mbuf *m) {
    // Print initial state of mbuf
   printf("\nInitial Mbuf State\n");
    printf("data_off   = %u\n", m->data_off);
    printf("data_len   = %u\n", m->data_len);
    printf("pkt_len    = %u\n", m->pkt_len);
    printf("headroom   = %u\n", rte_pktmbuf_headroom(m));
    printf("tailroom   = %u\n", rte_pktmbuf_tailroom(m));


    //Append some data
    const char *msg = "DPDK-MBUF";
    uint16_t len = strlen(msg) + 1;

    char *data = rte_pktmbuf_append(m, len);
    if (!data)
        rte_exit(EXIT_FAILURE, "Append failed\n");

    memcpy(data, msg, len);

    printf("\nAfter Append(%u bytes) \n", len);
    printf("data_off   = %u\n", m->data_off);
    printf("data_len   = %u\n", m->data_len);
    printf("pkt_len    = %u\n", m->pkt_len);
    printf("headroom   = %u\n", rte_pktmbuf_headroom(m));
    printf("tailroom   = %u\n", rte_pktmbuf_tailroom(m));


    // Prepend some bytes (add header)
    char *hdr = rte_pktmbuf_prepend(m, 10);
    if (hdr) {
        memset(hdr, 'H', 10);
    }

    printf("\nAfter Prepend(10 bytes)\n");
    printf("data_off   = %u\n", m->data_off);
    printf("data_len   = %u\n", m->data_len);
    printf("pkt_len    = %u\n", m->pkt_len);
    printf("headroom   = %u\n", rte_pktmbuf_headroom(m));
    printf("tailroom   = %u\n", rte_pktmbuf_tailroom(m));


    // Remove 5 bytes from beginning (adj)
    rte_pktmbuf_adj(m, 5);

    printf("\n After Adj(5 bytes removed)\n");
    printf("data_off   = %u\n", m->data_off);
    printf("data_len   = %u\n", m->data_len);
    printf("pkt_len    = %u\n", m->pkt_len);
    printf("headroom   = %u\n", rte_pktmbuf_headroom(m));
    printf("tailroom   = %u\n", rte_pktmbuf_tailroom(m));


    // Trim tail (remove last bytes)
    rte_pktmbuf_trim(m, 4);

    printf("\nAfter Trim(4 bytes)\n");
    printf("data_off   = %u\n", m->data_off);
    printf("data_len   = %u\n", m->data_len);
    printf("pkt_len    = %u\n", m->pkt_len);
    printf("headroom   = %u\n", rte_pktmbuf_headroom(m));
    printf("tailroom   = %u\n", rte_pktmbuf_tailroom(m));
}
int main(int argc, char **argv) {

  int rt = rte_eal_init(argc, argv);
  if (rt < 0) {
    rte_exit(EXIT_FAILURE, "EAL init failed\n");
  }

  struct rte_mempool *mbuf_pool = rte_pktmbuf_pool_create(
      "MBUF_POOL", 1024, 32, 0, RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());

  if (!mbuf_pool) {
    rte_exit(EXIT_FAILURE, "mbuf pool create failed\n");
  }

  printf("Mempool created\n");

  struct rte_eth_conf port_conf = {0};

  if (rte_eth_dev_configure(port_id, 1, 1, &port_conf) < 0)
    rte_exit(EXIT_FAILURE, "Port configure failed\n");

  if (rte_eth_rx_queue_setup(port_id, 0, 1024, rte_eth_dev_socket_id(port_id),
                             NULL, mbuf_pool) < 0)
    rte_exit(EXIT_FAILURE, "RX queue setup failed\n");

  if (rte_eth_tx_queue_setup(port_id, 0, 1024, rte_eth_dev_socket_id(port_id),
                             NULL) < 0)
    rte_exit(EXIT_FAILURE, "TX queue setup failed\n");

  if (rte_eth_dev_start(port_id) < 0)
    rte_exit(EXIT_FAILURE, "Port start failed\n");

  printf("Port %u initialized and started.\n", port_id);
  struct rte_mbuf *mbuf[32];

  while (1) {

    uint16_t nb_rx = rte_eth_rx_burst(port_id, 0, mbuf, 32);

    if (nb_rx > 0) {

      printf("Received %u packets\n", nb_rx);

      for (int i = 0; i < nb_rx; i++) {

        // printf("Packet length = %u\n", mbuf[i]->pkt_len);

        parser(mbuf[i]);

        operations(mbuf[i]);

        rte_pktmbuf_free(mbuf[i]);
      }
    }

    rte_pause();
  }
}