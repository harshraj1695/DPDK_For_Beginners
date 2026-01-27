#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <signal.h>
#include <stdbool.h>

#include <rte_common.h>
#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_graph.h>
#include <rte_graph_worker.h>
#include <rte_mbuf.h>
#include <rte_ether.h>

/* Use this specific header for ethdev graph nodes */
#include <rte_node_ethdev_api.h>

#define NB_MBUF 8192
#define MEMPOOL_CACHE_SIZE 256
#define RX_DESC_DEFAULT 1024
#define TX_DESC_DEFAULT 1024

static volatile bool force_quit;

/* --- Custom Classifier Node --- */
enum pkt_cls_next_nodes {
    PKT_CLS_NEXT_IP4_LOOKUP,
    PKT_CLS_NEXT_IP6_LOOKUP,
    PKT_CLS_NEXT_PKT_DROP,
    PKT_CLS_NEXT_MAX,
};

static uint16_t
pkt_cls_node_process(struct rte_graph *graph, struct rte_node *node,
                     void **objs, uint16_t nb_objs)
{
    uint16_t i;
    for (i = 0; i < nb_objs; i++) {
        struct rte_mbuf *mbuf = (struct rte_mbuf *)objs[i];
        struct rte_ether_hdr *eth_hdr = rte_pktmbuf_mtod(mbuf, struct rte_ether_hdr *);
        uint16_t eth_type = rte_be_to_cpu_16(eth_hdr->ether_type);

        uint16_t next_index;
        if (eth_type == RTE_ETHER_TYPE_IPV4)
            next_index = PKT_CLS_NEXT_IP4_LOOKUP;
        else if (eth_type == RTE_ETHER_TYPE_IPV6)
            next_index = PKT_CLS_NEXT_IP6_LOOKUP;
        else
            next_index = PKT_CLS_NEXT_PKT_DROP;

        rte_node_enqueue_x1(graph, node, next_index, objs[i]);
    }
    return nb_objs;
}

static struct rte_node_register pkt_cls_node_base = {
    .name = "pkt_cls",
    .process = pkt_cls_node_process,
    .nb_edges = PKT_CLS_NEXT_MAX,
    .next_nodes = {
        [PKT_CLS_NEXT_IP4_LOOKUP] = "ip4_lookup",
        [PKT_CLS_NEXT_IP6_LOOKUP] = "ip6_lookup",
        [PKT_CLS_NEXT_PKT_DROP] = "pkt_drop",
    },
};
RTE_NODE_REGISTER(pkt_cls_node_base);

static void signal_handler(int signum) {
    if (signum == SIGINT || signum == SIGTERM) {
        printf("\nExiting...\n");
        force_quit = true;
    }
}

int main(int argc, char **argv) {
    struct rte_mempool *mbuf_pool;
    uint16_t port_id = 0;
    int ret;

    ret = rte_eal_init(argc, argv);
    if (ret < 0) rte_exit(EXIT_FAILURE, "EAL Init failed\n");

    force_quit = false;
    signal(SIGINT, signal_handler);

    mbuf_pool = rte_pktmbuf_pool_create("MBUF_POOL", NB_MBUF, MEMPOOL_CACHE_SIZE, 0,
                                        RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());

    /* Ethdev Setup */
    struct rte_eth_conf port_conf = {0};
    rte_eth_dev_configure(port_id, 1, 1, &port_conf);
    rte_eth_rx_queue_setup(port_id, 0, RX_DESC_DEFAULT, rte_socket_id(), NULL, mbuf_pool);
    rte_eth_tx_queue_setup(port_id, 0, TX_DESC_DEFAULT, rte_socket_id(), NULL);
    rte_eth_dev_start(port_id);

    /* 1. Register ethdev ports to the graph system */
    struct rte_node_ethdev_config eth_node_conf[1];
    eth_node_conf[0].port_id = port_id;
    eth_node_conf[0].num_rx_queues = 1;
    eth_node_conf[0].num_tx_queues = 1;

    ret = rte_node_ethdev_config(eth_node_conf, 1);
    if (ret < 0) rte_exit(EXIT_FAILURE, "Ethdev node config failed\n");

    /* 2. Create the Graph */
    const char *node_patterns[] = {
        "ethdev_rx-0-0", "pkt_cls", "ip4_lookup", 
        "ip6_lookup", "ethdev_tx-0", "pkt_drop"
    };

    struct rte_graph_param graph_conf = {
        .node_patterns = node_patterns,
        .nb_node_patterns = RTE_DIM(node_patterns),
        .socket_id = rte_socket_id(),
    };

    rte_graph_t graph_id = rte_graph_create("my_graph", &graph_conf);
    if (graph_id == RTE_GRAPH_ID_INVALID) rte_exit(EXIT_FAILURE, "Graph creation failed\n");

    /* 3. Convert ID to Pointer for rte_graph_walk */
    struct rte_graph *graph = rte_graph_lookup_by_name("my_graph");
    if (!graph) rte_exit(EXIT_FAILURE, "Graph lookup failed\n");

    

    printf("Processing packets. Press Ctrl+C to stop.\n");

    while (!force_quit) {
        rte_graph_walk(graph);
    }

    rte_eth_dev_stop(port_id);
    rte_eal_cleanup();
    return 0;
}