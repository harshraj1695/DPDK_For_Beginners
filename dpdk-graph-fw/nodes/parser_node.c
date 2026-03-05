#include <stdio.h>
#include <rte_graph.h>
#include <rte_graph_worker.h>

#include "config.h"

enum {
    PARSER_NEXT_FIREWALL,
    PARSER_NEXT_TX
};

static uint16_t
parser_node_process(struct rte_graph *graph,
                    struct rte_node *node,
                    void **objs,
                    uint16_t nb_objs)
{
    parser_count += nb_objs;

    uint16_t next;

    if (firewall_enabled)
        next = PARSER_NEXT_FIREWALL;
    else
        next = PARSER_NEXT_TX;

    /* print occasionally */
    if (parser_count % 1000 < nb_objs) {
        if (firewall_enabled)
            printf("[Parser] -> FIREWALL\n");
        else
            printf("[Parser] -> TX\n");
    }

    /* ALWAYS forward packets */
    rte_node_enqueue(graph, node, next, objs, nb_objs);

    return nb_objs;
}

static struct rte_node_register parser_node = {
    .name = "parser_node",
    .process = parser_node_process,
    .nb_edges = 2,
    .next_nodes = {
        [PARSER_NEXT_FIREWALL] = "firewall_node",
        [PARSER_NEXT_TX] = "tx_node",
    },
};

RTE_NODE_REGISTER(parser_node);