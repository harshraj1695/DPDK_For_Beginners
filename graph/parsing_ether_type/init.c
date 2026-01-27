#include "pipeline.h"

// ------------------------------------------------------
// RX NODE (SOURCE)
// ------------------------------------------------------
uint16_t rx_node_func(struct rte_graph *graph, struct rte_node *node,
                      void **objs, uint16_t nb_objs __rte_unused)
{
    struct rte_mbuf *pkts[32];
    uint16_t n = rte_eth_rx_burst(0, 0, pkts, 32);

    for (uint16_t i = 0; i < n; i++)
        objs[i] = pkts[i];

    if (n > 0)
        rte_node_enqueue_next_simple(graph, node, 0, objs, n);

    return n;
}

RTE_NODE_REGISTER_SIMPLE(rx_simple) = {
    .name = RX_NODE_NAME,
    .process = rx_node_func,

    // RX is a source node
    .flags = RTE_NODE_SOURCE_F,

    // RX has 1 next node → L2
    .nb_edges = 1,
    .next_nodes = { L2_NODE_NAME },
};


// ------------------------------------------------------
// L2 PARSER NODE
// ------------------------------------------------------
uint16_t l2_node_func(struct rte_graph *graph, struct rte_node *node,
                      void **objs, uint16_t nb_objs)
{
    void *ipv4_pkts[32];
    void *ipv6_pkts[32];
    uint16_t v4 = 0, v6 = 0;

    for (uint16_t i = 0; i < nb_objs; i++) {
        struct rte_mbuf *m = objs[i];

        struct rte_ether_hdr *eth =
            rte_pktmbuf_mtod(m, struct rte_ether_hdr *);

        uint16_t etype = rte_be_to_cpu_16(eth->ether_type);

        if (etype == RTE_ETHER_TYPE_IPV4)
            ipv4_pkts[v4++] = m;
        else if (etype == RTE_ETHER_TYPE_IPV6)
            ipv6_pkts[v6++] = m;
        else
            rte_pktmbuf_free(m);
    }

    if (v4 > 0)
        rte_node_enqueue_next_simple(graph, node, 0, ipv4_pkts, v4);

    if (v6 > 0)
        rte_node_enqueue_next_simple(graph, node, 1, ipv6_pkts, v6);

    return nb_objs;
}

RTE_NODE_REGISTER_SIMPLE(l2_simple) = {
    .name = L2_NODE_NAME,
    .process = l2_node_func,

    // Two outputs: IPv4, IPv6
    .nb_edges = 2,
    .next_nodes = { IPV4_NODE_NAME, IPV6_NODE_NAME },
};


// ------------------------------------------------------
// IPv4 NODE
// ------------------------------------------------------
uint16_t ipv4_node_func(struct rte_graph *graph, struct rte_node *node,
                        void **objs, uint16_t nb_objs)
{
    if (nb_objs > 0)
        rte_node_enqueue_next_simple(graph, node, 0, objs, nb_objs);

    return nb_objs;
}

RTE_NODE_REGISTER_SIMPLE(ipv4_simple) = {
    .name = IPV4_NODE_NAME,
    .process = ipv4_node_func,

    .nb_edges = 1,
    .next_nodes = { TX_NODE_NAME },
};


// ------------------------------------------------------
// IPv6 NODE
// ------------------------------------------------------
uint16_t ipv6_node_func(struct rte_graph *graph, struct rte_node *node,
                        void **objs, uint16_t nb_objs)
{
    if (nb_objs > 0)
        rte_node_enqueue_next_simple(graph, node, 0, objs, nb_objs);

    return nb_objs;
}

RTE_NODE_REGISTER_SIMPLE(ipv6_simple) = {
    .name = IPV6_NODE_NAME,
    .process = ipv6_node_func,

    .nb_edges = 1,
    .next_nodes = { TX_NODE_NAME },
};


// ------------------------------------------------------
// TX NODE
// ------------------------------------------------------
uint16_t tx_node_func(struct rte_graph *graph __rte_unused,
                      struct rte_node *node __rte_unused,
                      void **objs,
                      uint16_t nb_objs)
{
    struct rte_mbuf *mp[32];

    for (uint16_t i = 0; i < nb_objs; i++)
        mp[i] = objs[i];

    rte_eth_tx_burst(0, 0, mp, nb_objs);
    return nb_objs;
}

RTE_NODE_REGISTER_SIMPLE(tx_simple) = {
    .name = TX_NODE_NAME,
    .process = tx_node_func,

    // TX has no next nodes
    .nb_edges = 0,
    .next_nodes = { NULL },
};
