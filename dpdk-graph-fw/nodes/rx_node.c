#include <stdio.h>
#include <rte_graph.h>
#include <rte_graph_worker.h>
#include <rte_ethdev.h>

#include "config.h"

enum {
    RX_NEXT_PARSER
};

static uint16_t
rx_node_process(struct rte_graph *graph,
                struct rte_node *node,
                void **objs,
                uint16_t nb_objs)
{
    struct rte_mbuf *pkts[32];

    uint16_t nb_rx;

    nb_rx = rte_eth_rx_burst(0, 0, pkts, 32);

   rx_count += nb_rx;

if (nb_rx) {
    if (rx_count % 1000 < nb_rx) {
        printf("[RX] total=%lu (last burst=%u)\n", rx_count, nb_rx);
    }

    rte_node_enqueue(graph, node,
                     RX_NEXT_PARSER,
                     (void **)pkts,
                     nb_rx);
}

    return nb_rx;
}

static struct rte_node_register rx_node = {
    .name = "rx_node",
    .flags = RTE_NODE_SOURCE_F,
    .process = rx_node_process,
    .nb_edges = 1,
    .next_nodes = {
        [RX_NEXT_PARSER] = "parser_node",
    },
};

RTE_NODE_REGISTER(rx_node);