#include <rte_ethdev.h>
#include <stdint.h>
#include <stdio.h>

#include <rte_eal.h>
#include <rte_mbuf.h>
#include <rte_mbuf_dyn.h>
#include <rte_mempool.h>
#include <rte_reorder.h>

#define NB_MBUF 1024
#define CACHE 32
#define WINDOW 8
#define BURST 8

int seq_offset = -1;
int rte_reorder_seqn_dynfield_offset = -1;
int port_id = 0;
void init_port(uint16_t port_id, struct rte_mempool *mp) {
  struct rte_eth_conf port_conf = {0};

  if (rte_eth_dev_configure(port_id, 1, 1, &port_conf) < 0)
    rte_exit(EXIT_FAILURE, "Port configure failed\n");

  if (rte_eth_rx_queue_setup(port_id, 0, 1024, rte_eth_dev_socket_id(port_id),
                             NULL, mp) < 0)
    rte_exit(EXIT_FAILURE, "RX queue setup failed\n");

  // if (rte_eth_tx_queue_setup(port_id, 0, 1024,
  //                            rte_eth_dev_socket_id(port_id),
  //                            NULL) < 0)
  //     rte_exit(EXIT_FAILURE, "TX queue setup failed\n");

  if (rte_eth_dev_start(port_id) < 0)
    rte_exit(EXIT_FAILURE, "Port start failed\n");

  printf("Port %u initialized and started.\n", port_id);
}

int main(int argc, char **argv) {
  if (rte_eal_init(argc, argv) < 0)
    rte_panic("EAL failed\n");

  struct rte_mempool *mp =
      rte_pktmbuf_pool_create("POOL", NB_MBUF, CACHE,
                              0, // private area not needed
                              RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());

  if (!mp)
    rte_panic("mempool failed\n");

  // struct rte_mbuf_dynfield seqno_dynfield = {
  //     .name  = "example_seq_no",
  //     .size  = sizeof(uint32_t),
  //     .align = __alignof__(uint32_t),
  // };

  // seq_offset = rte_mbuf_dynfield_register(&seqno_dynfield);
  // rte_reorder_seqn_dynfield_offset = seq_offset;
  // if (seq_offset < 0)
  //     rte_panic("Failed to register dynamic field\n");

  init_port(port_id, mp);
  struct rte_reorder_buffer *buf =
      rte_reorder_create("reorder", rte_socket_id(), WINDOW);

  if (!buf)
    rte_panic("reorder create failed\n");

  printf("reorder created sucess\n");
  // here i have not created a dynamic field, but it is automatically created by
  // the
  //  reorder library when we set the seqn_dynfield_offset
  //   rte_reorder_seqn_dynfield_offset = rte_mbuf_dynfield_lookup("a", NULL);
  // rte_reorder_seqn_dynfield_offset =
  // rte_mbuf_dynfield_lookup("rte_reorder_seqn_dynfield", NULL);
  //     if (rte_reorder_seqn_dynfield_offset < 0){
  //         rte_panic("Failed to register dynamic field\n");
  //     }

  uint32_t seq_list[BURST] = {0, 2, 1, 4, 3, 6, 5, 7};

  struct rte_mbuf *out[BURST];
  struct rte_mbuf *in[BURST];
  int rt = 0;
  uint32_t seq_counter = 0;

  while (1) {
    uint16_t rt = rte_eth_rx_burst(port_id, 0, in, BURST);

    if (rt == 0)
      continue;

    for (uint16_t i = 0; i < rt; i++) {
      struct rte_mbuf *m = in[i];

      uint32_t seq = seq_counter++;

      /* Correct way */
      // *rte_reorder_seqn(m) = seq;

      uint32_t *seqno =
          RTE_MBUF_DYNFIELD(m, rte_reorder_seqn_dynfield_offset, uint32_t *);
      *seqno = seq;

      printf("Insert seq = %u\n", seq);

      if (rte_reorder_insert(buf, m) < 0) {
        rte_pktmbuf_free(m);
      }
    }

    uint16_t nb = rte_reorder_drain(buf, out, BURST);
    for (unsigned j = 0; j < nb; j++) {
      uint32_t *seqno = RTE_MBUF_DYNFIELD(
          out[j], rte_reorder_seqn_dynfield_offset, uint32_t *);

      printf("Drained seq = %u\n", *seqno);

      rte_pktmbuf_free(out[j]);
    }
  }

  rte_reorder_free(buf);

  return 0;
}
