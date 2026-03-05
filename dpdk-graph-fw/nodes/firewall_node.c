#include <rte_graph.h>
#include <rte_graph_worker.h>
#include <rte_mbuf.h>
#include <stdio.h>

#include "config.h"

enum {
    FIREWALL_NEXT_TX
};

static uint16_t
firewall_node_process(struct rte_graph *graph,
                      struct rte_node *node,
                      void **objs,
                      uint16_t nb_objs)
{
    uint16_t i;
    uint16_t out = 0;

    firewall_count += nb_objs;

    /* print occasionally */
    if (firewall_count % 1000 < nb_objs)
        printf("[Firewall] processing %u packets\n", nb_objs);

    /* reuse objs array */
    for (i = 0; i < nb_objs; i++) {

        struct rte_mbuf *m = objs[i];

        /* simple drop rule */
        if (m->pkt_len < 60) {
            rte_pktmbuf_free(m);
            continue;
        }

        objs[out++] = m;
    }

    if (out)
        rte_node_enqueue(graph, node,
                         FIREWALL_NEXT_TX,
                         objs,
                         out);

    return nb_objs;
}

static struct rte_node_register firewall_node = {
    .name = "firewall_node",
    .process = firewall_node_process,
    .nb_edges = 1,
    .next_nodes = {
        [FIREWALL_NEXT_TX] = "tx_node",
    },
};

RTE_NODE_REGISTER(firewall_node);