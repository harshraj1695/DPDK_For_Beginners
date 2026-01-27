#include "pipeline.h"

// ------------------------------------------------------------
// RX NODE
// Reads packets from NIC (port 0) and sends them to L2 node
// ------------------------------------------------------------
uint16_t rx_node_func(struct rte_graph *graph,
                      struct rte_node *node,
                      void **objs,
                      uint16_t nb_objs __rte_unused)
{
    struct rte_mbuf *pkts[32];

    // Receive packets from NIC
    uint16_t n = rte_eth_rx_burst(0, 0, pkts, 32);

    // Copy packets into objs[]
    for (uint16_t i = 0; i < n; i++)
        objs[i] = pkts[i];

    // Send packets to L2 parser node (edge 0)
    if (n > 0)
        rte_node_enqueue(graph, node, 0, objs, n);

    return n;
}

// Register RX node
static struct rte_node_register rx_node = {
    .name = RX_NODE_NAME,
    .process = rx_node_func,
    .flags = RTE_NODE_SOURCE_F,         // RX is a source node
    .nb_edges = 1,                      // Only 1 next node
    .next_nodes = { L2_NODE_NAME },
};
RTE_NODE_REGISTER(rx_node);

// ------------------------------------------------------------
// L2 PARSER NODE
// Splits packets into IPv4 and IPv6
// ------------------------------------------------------------
uint16_t l2_node_func(struct rte_graph *graph,
                      struct rte_node *node,
                      void **objs,
                      uint16_t nb_objs)
{
    void *ipv4_pkts[32];
    void *ipv6_pkts[32];
    uint16_t v4 = 0, v6 = 0;

    for (uint16_t i = 0; i < nb_objs; i++) {

        struct rte_mbuf *m = objs[i];

        // Read Ethernet header
        struct rte_ether_hdr *eth =
            rte_pktmbuf_mtod(m, struct rte_ether_hdr *);

        uint16_t etype = rte_be_to_cpu_16(eth->ether_type);

        if (etype == RTE_ETHER_TYPE_IPV4)
            ipv4_pkts[v4++] = m;
        else if (etype == RTE_ETHER_TYPE_IPV6)
            ipv6_pkts[v6++] = m;
        else
            rte_pktmbuf_free(m);     // Drop unknown packets
    }

    // Send IPv4 packets to IPv4 node (edge 0)
    if (v4 > 0)
        rte_node_enqueue(graph, node, 0, ipv4_pkts, v4);

    // Send IPv6 packets to IPv6 node (edge 1)
    if (v6 > 0)
        rte_node_enqueue(graph, node, 1, ipv6_pkts, v6);

    return nb_objs;
}

// Register L2 node
static struct rte_node_register l2_node = {
    .name = L2_NODE_NAME,
    .process = l2_node_func,
    .nb_edges = 2,     // IPv4 and IPv6
    .next_nodes = { IPV4_NODE_NAME, IPV6_NODE_NAME },
};
RTE_NODE_REGISTER(l2_node);

// ------------------------------------------------------------
// IPv4 NODE
// Simply forwards IPv4 packets to TX node
// ------------------------------------------------------------
uint16_t ipv4_node_func(struct rte_graph *graph,
                        struct rte_node *node,
                        void **objs,
                        uint16_t nb_objs)
{
    if (nb_objs > 0)
        rte_node_enqueue(graph, node, 0, objs, nb_objs);

    return nb_objs;
}

static struct rte_node_register ipv4_node = {
    .name = IPV4_NODE_NAME,
    .process = ipv4_node_func,
    .nb_edges = 1,
    .next_nodes = { TX_NODE_NAME },
};
RTE_NODE_REGISTER(ipv4_node);

// ------------------------------------------------------------
// IPv6 NODE
// Simply forwards IPv6 packets to TX node
// ------------------------------------------------------------
uint16_t ipv6_node_func(struct rte_graph *graph,
                        struct rte_node *node,
                        void **objs,
                        uint16_t nb_objs)
{
    if (nb_objs > 0)
        rte_node_enqueue(graph, node, 0, objs, nb_objs);

    return nb_objs;
}

static struct rte_node_register ipv6_node = {
    .name = IPV6_NODE_NAME,
    .process = ipv6_node_func,
    .nb_edges = 1,
    .next_nodes = { TX_NODE_NAME },
};
RTE_NODE_REGISTER(ipv6_node);

// ------------------------------------------------------------
// TX NODE
// Sends packets out of port 0
// ------------------------------------------------------------
uint16_t tx_node_func(struct rte_graph *graph __rte_unused,
                      struct rte_node *node __rte_unused,
                      void **objs,
                      uint16_t nb_objs)
{
    struct rte_mbuf *pkts[32];

    for (uint16_t i = 0; i < nb_objs; i++)
        pkts[i] = objs[i];

    rte_eth_tx_burst(0, 0, pkts, nb_objs);
    return nb_objs;
}

static struct rte_node_register tx_node = {
    .name = TX_NODE_NAME,
    .process = tx_node_func,
    .nb_edges = 0,
};
RTE_NODE_REGISTER(tx_node);
