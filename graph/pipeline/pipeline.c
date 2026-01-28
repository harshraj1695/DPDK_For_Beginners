#include <stdio.h>
#include <stdint.h>
#include <signal.h>

#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_mbuf.h>
#include <rte_ether.h>

#include <rte_graph.h>
#include <rte_graph_worker.h>

#define RX_NODE_NAME   "rx_node"
#define L2_NODE_NAME   "l2_node"
#define IPV4_NODE_NAME "ipv4_node"
#define IPV6_NODE_NAME "ipv6_node"
#define TX_NODE_NAME   "tx_node"
#define DROP_NODE_NAME "drop_node"

#define BURST 32
#define NB_MBUFS 8192
#define MBUF_CACHE 256
#define RX_RING_SIZE 1024
#define TX_RING_SIZE 1024

static volatile int quit = 0;

/* ---------------- RX NODE (source) ---------------- */
static uint16_t
rx_node_process(struct rte_graph *graph,
                struct rte_node *node,
                void **objs,
                uint16_t nb)
{
    struct rte_mbuf **pkts = (struct rte_mbuf **)objs;
    uint16_t n = rte_eth_rx_burst(0, 0, pkts, BURST);
   
    if (n)
    {
        printf("Rx Node received %u packets\n", n);
        rte_node_enqueue(graph, node, 0, objs, n);
    }
    return n;
}

static struct rte_node_register rx_node = {
    .name = RX_NODE_NAME,
    .process = rx_node_process,
    .flags = RTE_NODE_SOURCE_F,
    .nb_edges = 1,
    .next_nodes = { [0] = L2_NODE_NAME },
};
RTE_NODE_REGISTER(rx_node);


/* ---------------- L2 PARSER NODE ---------------- */
static uint16_t
l2_node_process(struct rte_graph *graph,
                struct rte_node *node,
                void **objs,
                uint16_t nb)
{
    void *ipv4[BURST], *ipv6[BURST], *drop[BURST];
    uint16_t v4 = 0, v6 = 0, d = 0;
    printf("In node process L2 \n");
    for (uint16_t i = 0; i < nb; i++) {
        struct rte_mbuf *m = objs[i];
        struct rte_ether_hdr *eth =
            rte_pktmbuf_mtod(m, struct rte_ether_hdr *);
        printf("Eth type %u\n",eth->ether_type );
        switch (rte_be_to_cpu_16(eth->ether_type)) {
            case RTE_ETHER_TYPE_IPV4: ipv4[v4++] = m; break;
            case RTE_ETHER_TYPE_IPV6: ipv6[v6++] = m; break;
            default: drop[d++] = m; break;
        }
    }

    if (v4) rte_node_enqueue(graph, node, 0, ipv4, v4);
    if (v6) rte_node_enqueue(graph, node, 1, ipv6, v6);
    if (d) rte_node_enqueue(graph, node, 2, drop, d);

    return nb;
}

static struct rte_node_register l2_node = {
    .name = L2_NODE_NAME,
    .process = l2_node_process,
    .nb_edges = 3,
    .next_nodes = {
        [0] = IPV4_NODE_NAME,
        [1] = IPV6_NODE_NAME,
        [2] = DROP_NODE_NAME,
    },
};
RTE_NODE_REGISTER(l2_node);


/* ---------------- IPv4 NODE ---------------- */
static uint16_t
ipv4_node_process(struct rte_graph *graph,
                  struct rte_node *node,
                  void **objs,
                  uint16_t nb)
{
    printf("In node process IPv4 \n");
    rte_node_enqueue(graph, node, 0, objs, nb);
    return nb;
}

static struct rte_node_register ipv4_node = {
    .name = IPV4_NODE_NAME,
    .process = ipv4_node_process,
    .nb_edges = 1,
    .next_nodes = { [0] = TX_NODE_NAME },
};
RTE_NODE_REGISTER(ipv4_node);


/* ---------------- IPv6 NODE ---------------- */
static uint16_t
ipv6_node_process(struct rte_graph *graph,
                  struct rte_node *node,
                  void **objs,
                  uint16_t nb)
{
    printf("In node process IPv6\n");
    rte_node_enqueue(graph, node, 0, objs, nb);
    return nb;
}

static struct rte_node_register ipv6_node = {
    .name = IPV6_NODE_NAME,
    .process = ipv6_node_process,
    .nb_edges = 1,
    .next_nodes = { [0] = TX_NODE_NAME },
};
RTE_NODE_REGISTER(ipv6_node);


/* ---------------- TX NODE ---------------- */
static uint16_t
tx_node_process(struct rte_graph *graph __rte_unused,
                struct rte_node *node __rte_unused,
                void **objs,
                uint16_t nb)
{
    printf("In node process TX \n");
    int n=rte_eth_tx_burst(0, 0, (struct rte_mbuf **)objs, nb);
    if(n<nb){
        for(int i=n;i<nb;i++)
            rte_pktmbuf_free((struct rte_mbuf *)objs[i]);
    }
    return nb;
}

static struct rte_node_register tx_node = {
    .name = TX_NODE_NAME,
    .process = tx_node_process,
};
RTE_NODE_REGISTER(tx_node);


/* ---------------- DROP NODE ---------------- */
static uint16_t
drop_node_process(struct rte_graph *graph __rte_unused,
                  struct rte_node *node __rte_unused,
                  void **objs,
                  uint16_t nb)
{
    for (uint16_t i = 0; i < nb; i++)
        rte_pktmbuf_free(objs[i]);
    return nb;
}

static struct rte_node_register drop_node = {
    .name = DROP_NODE_NAME,
    .process = drop_node_process,
};
RTE_NODE_REGISTER(drop_node);


/* ---------------- Main ---------------- */
static void handler(int sig) { quit = 1; }

int main(int argc, char **argv)
{
    rte_graph_t graph_id;
    struct rte_graph *graph;

    int ret = rte_eal_init(argc, argv);
    if (ret < 0)
        rte_exit(EXIT_FAILURE, "EAL init failed\n");

    signal(SIGINT, handler);

    /* ***** CREATE MEMPOOL ***** */
    struct rte_mempool *pool =
        rte_pktmbuf_pool_create("MBUF_POOL", NB_MBUFS,
                                MBUF_CACHE, 0,
                                RTE_MBUF_DEFAULT_BUF_SIZE,
                                rte_socket_id());
    if (!pool)
        rte_exit(EXIT_FAILURE, "mempool create failed\n");

    /* ***** CONFIGURE PORT 0 ***** */
    struct rte_eth_conf conf = {0};
    rte_eth_dev_configure(0, 1, 1, &conf);
    rte_eth_rx_queue_setup(0, 0, RX_RING_SIZE,
                           rte_eth_dev_socket_id(0),
                           NULL, pool);
    rte_eth_tx_queue_setup(0, 0, TX_RING_SIZE,
                           rte_eth_dev_socket_id(0),
                           NULL);
    rte_eth_dev_start(0);

    /* ***** CREATE GRAPH ***** */
    struct rte_graph_param gp = {
        .nb_node_patterns = 1,
        .node_patterns = (const char *[]){ RX_NODE_NAME },
    };

    graph_id = rte_graph_create("pipeline_graph", &gp);
    if (graph_id == RTE_GRAPH_ID_INVALID)
        rte_exit(EXIT_FAILURE, "graph create failed\n");

    graph = rte_graph_lookup("pipeline_graph");

    printf("Running RX → L2 → IPv4/IPv6 → TX Pipeline...\n");

    while (!quit)
        rte_graph_walk(graph);

    rte_graph_destroy(graph_id);
    rte_eth_dev_stop(0);
    rte_eth_dev_close(0);
}
