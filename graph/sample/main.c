// Simple DPDK Graph Example (Beginner Friendly)
// Graph looks like this:
// RX -> IPv4 check -> TX or DROP

#include <stdint.h>
#include <stdio.h>
#include <signal.h>

// DPDK main headers
#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_mbuf.h>
#include <rte_ether.h>

#include <rte_graph.h>
#include <rte_graph_worker.h>

#define NB_MBUFS        8192
#define MBUF_CACHE      256
#define RX_RING_SIZE    1024
#define TX_RING_SIZE    1024
#define BURST_SIZE      RTE_GRAPH_BURST_SIZE

#define RX_NODE_NAME   "rx_node"
#define IPV4_NODE_NAME "ipv4_node"
#define TX_NODE_NAME   "tx_node"
#define DROP_NODE_NAME "drop_node"

static volatile int force_quit;

static void signal_handler(int signum) {
    if (signum == SIGINT || signum == SIGTERM)
        force_quit = 1;
}

// This node reads packets from NIC (port 0)
// This is the source of the graph
static uint16_t rx_node_process(struct rte_graph *graph,
                                struct rte_node *node,
                                void **objs,
                                uint16_t nb_objs __rte_unused)
{
    struct rte_mbuf **pkts = (struct rte_mbuf **)objs;

    // Read packets from NIC
    uint16_t nb_rx = rte_eth_rx_burst(0, 0, pkts, BURST_SIZE);

    // If packets received, send them to IPv4 node
    if (nb_rx > 0)
        rte_node_enqueue(graph, node, 0, objs, nb_rx);

    return nb_rx;
}

// Register RX node so graph can use it
static struct rte_node_register rx_node = {
    .name = RX_NODE_NAME,
    .process = rx_node_process,

    // Mark this node as a source node
    .flags = RTE_NODE_SOURCE_F,

    // This node has 1 output edge
    .nb_edges = 1,

    // Next node after RX is IPv4 node
    .next_nodes = {
        [0] = IPV4_NODE_NAME,
    },
};
RTE_NODE_REGISTER(rx_node);

// If IPv4 -> send to TX
// Else -> send to DROP
static uint16_t ipv4_node_process(struct rte_graph *graph,
                                  struct rte_node *node,
                                  void **objs,
                                  uint16_t nb_objs)
{
    // Print how many packets came here
    printf("Got %u packets\n", nb_objs);

    for (uint16_t i = 0; i < nb_objs; i++) {

        struct rte_mbuf *pkt = (struct rte_mbuf *)objs[i];

        struct rte_ether_hdr *eth =
            rte_pktmbuf_mtod(pkt, struct rte_ether_hdr *);

        // Check if packet is IPv4
        if (eth->ether_type == rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4)) {

            // IPv4 packet -> go to TX
            rte_node_enqueue_x1(graph, node, 0, pkt);

        } else {

            // Not IPv4 -> go to DROP
            rte_node_enqueue_x1(graph, node, 1, pkt);
        }
    }

    return nb_objs;
}

// Register IPv4 node
static struct rte_node_register ipv4_node = {
    .name = IPV4_NODE_NAME,
    .process = ipv4_node_process,

    .nb_edges = 2,        // Two outputs: TX and DROP

    .next_nodes = {
        [0] = TX_NODE_NAME,
        [1] = DROP_NODE_NAME,
    },
};
RTE_NODE_REGISTER(ipv4_node);

// This node sends packets out on NIC TX queue
static uint16_t tx_node_process(struct rte_graph *graph __rte_unused,
                                struct rte_node *node __rte_unused,
                                void **objs,
                                uint16_t nb_objs)
{
    struct rte_mbuf **pkts = (struct rte_mbuf **)objs;

    // Send packets out NIC
    rte_eth_tx_burst(0, 0, pkts, nb_objs);
    return nb_objs;
}

// Register TX node
static struct rte_node_register tx_node = {
    .name = TX_NODE_NAME,
    .process = tx_node_process,
};
RTE_NODE_REGISTER(tx_node);

// This node frees packets we don't want
static uint16_t drop_node_process(struct rte_graph *graph __rte_unused,
                                  struct rte_node *node __rte_unused,
                                  void **objs,
                                  uint16_t nb_objs)
{
    for (uint16_t i = 0; i < nb_objs; i++)
        rte_pktmbuf_free((struct rte_mbuf *)objs[i]);

    return nb_objs;
}

// Register DROP node
static struct rte_node_register drop_node = {
    .name = DROP_NODE_NAME,
    .process = drop_node_process,
};
RTE_NODE_REGISTER(drop_node);

int main(int argc, char **argv)
{
    int ret;
    struct rte_mempool *mbuf_pool;
    struct rte_graph *graph;
    rte_graph_t graph_id;

    // Start DPDK
    ret = rte_eal_init(argc, argv);
    if (ret < 0)
        rte_exit(EXIT_FAILURE, "EAL init failed\n");

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    // Create mempool for packets
    mbuf_pool = rte_pktmbuf_pool_create("MBUF_POOL",
                                        NB_MBUFS,
                                        MBUF_CACHE,
                                        0,
                                        RTE_MBUF_DEFAULT_BUF_SIZE,
                                        rte_socket_id());

    if (!mbuf_pool)
        rte_exit(EXIT_FAILURE, "Mbuf pool creation failed\n");

    // Setup NIC (port 0) with 1 RX and 1 TX queue
    struct rte_eth_conf port_conf = {0};

    rte_eth_dev_configure(0, 1, 1, &port_conf);

    rte_eth_rx_queue_setup(0, 0, RX_RING_SIZE,
                           rte_eth_dev_socket_id(0),
                           NULL, mbuf_pool);

    rte_eth_tx_queue_setup(0, 0, TX_RING_SIZE,
                           rte_eth_dev_socket_id(0),
                           NULL);

    rte_eth_dev_start(0);

    // Create the graph starting from RX node
    struct rte_graph_param graph_param = {
        .nb_node_patterns = 1,
        .node_patterns = (const char *[]) { RX_NODE_NAME },
    };

    graph_id = rte_graph_create("ipv4_graph", &graph_param);
    if (graph_id == RTE_GRAPH_ID_INVALID)
        rte_exit(EXIT_FAILURE, "Graph creation failed\n");

    graph = rte_graph_lookup("ipv4_graph");
    if (!graph)
        rte_exit(EXIT_FAILURE, "Graph lookup failed\n");

    printf("DPDK Graph running (Ctrl+C to quit)\n");

    // Keep running the graph until Ctrl+C
    while (!force_quit) {
        rte_graph_walk(graph);
    }

    rte_graph_destroy(graph_id);
    rte_eth_dev_stop(0);
    rte_eth_dev_close(0);

    printf("Exited cleanly\n");
    return 0;
}
