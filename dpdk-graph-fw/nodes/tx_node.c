#include <stdio.h>
#include <rte_graph.h>
#include <rte_graph_worker.h>
#include <rte_ethdev.h>

#include "config.h"

static uint16_t
tx_node_process(struct rte_graph *graph,
                struct rte_node *node,
                void **objs,
                uint16_t nb_objs)
{

    tx_count += nb_objs;

if (tx_count % 1000 < nb_objs)
    printf("[TX] total sent=%lu\n", tx_count);

    rte_eth_tx_burst(0, 0,
                     (struct rte_mbuf **)objs,
                     nb_objs);

    return nb_objs;
}

static struct rte_node_register tx_node = {
    .name = "tx_node",
    .process = tx_node_process,
    .nb_edges = 0,
};

RTE_NODE_REGISTER(tx_node);