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


  struct rte_reorder_buffer *buf =
      rte_reorder_create("reorder", rte_socket_id(), WINDOW);

  if (!buf)
    rte_panic("reorder create failed\n");

    // here i have not created a dynamic field, but it is automatically created by the
    //  reorder library when we set the seqn_dynfield_offset
    //   rte_reorder_seqn_dynfield_offset = rte_mbuf_dynfield_lookup("a", NULL);
    // rte_reorder_seqn_dynfield_offset = rte_mbuf_dynfield_lookup("rte_reorder_seqn_dynfield", NULL);
    //     if (rte_reorder_seqn_dynfield_offset < 0){
    //         rte_panic("Failed to register dynamic field\n");
    //     }

  uint32_t seq_list[BURST] = {0, 2, 1, 4, 3, 6, 5, 7};

  struct rte_mbuf *out[BURST];

  for (int i = 0; i < BURST; i++) {
    struct rte_mbuf *m = rte_pktmbuf_alloc(mp);

    if (!m)
      rte_panic("alloc failed\n");

    uint32_t *seqno =
        RTE_MBUF_DYNFIELD(m, rte_reorder_seqn_dynfield_offset, uint32_t *);
    *seqno = seq_list[i];

    // VERY IMPORTANT: Tell reorder library the seq
    // rte_reorder_min_seqn_set(buf, *seqno);

    // rte_reorder_seqn_set(m, *seqno);

    printf("Insert seq = %u\n", *seqno);

    rte_reorder_insert(buf, m);
  }

  unsigned nb = rte_reorder_drain(buf, out, BURST);

  for (unsigned j = 0; j < nb; j++) {
    uint32_t *seqno =
        RTE_MBUF_DYNFIELD(out[j], rte_reorder_seqn_dynfield_offset, uint32_t *);

    printf("Drained seq = %u\n", *seqno);

    rte_pktmbuf_free(out[j]);
  }

  rte_reorder_free(buf);

  return 0;
}
