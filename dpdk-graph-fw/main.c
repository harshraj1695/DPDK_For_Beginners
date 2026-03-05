#include <stdio.h>
#include <pthread.h>

#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_mbuf.h>
#include <rte_graph.h>
#include <rte_graph_worker.h>

#include "config.h"

#define NUM_MBUFS 8192
#define MBUF_CACHE_SIZE 250

void run_cli();

static struct rte_mempool *mbuf_pool;

void *cli_thread(void *arg)
{
    run_cli();
    return NULL;
}

static void
port_init(uint16_t port)
{

    struct rte_eth_conf port_conf = {0};

    rte_eth_dev_configure(port, 1, 1, &port_conf);

    rte_eth_rx_queue_setup(port, 0, 1024,
        rte_eth_dev_socket_id(port),
        NULL,
        mbuf_pool);

    rte_eth_tx_queue_setup(port, 0, 1024,
        rte_eth_dev_socket_id(port),
        NULL);

    rte_eth_dev_start(port);
}

int main(int argc, char **argv)
{
    rte_graph_t graph_id;

    struct rte_graph *graph;

    pthread_t cli;

    rte_eal_init(argc, argv);

    mbuf_pool = rte_pktmbuf_pool_create(
            "MBUF_POOL",
            NUM_MBUFS,
            MBUF_CACHE_SIZE,
            0,
            RTE_MBUF_DEFAULT_BUF_SIZE,
            rte_socket_id());

    port_init(0);

    const char *nodes[] = {
        "rx_node"
    };

    struct rte_graph_param param = {
        .nb_node_patterns = 1,
        .node_patterns = nodes,
    };

    graph_id = rte_graph_create("packet_graph", &param);

    graph = rte_graph_lookup("packet_graph");

    pthread_create(&cli, NULL, cli_thread, NULL);

    while (app_running)
        rte_graph_walk(graph);

    printf("Application exiting\n");

    return 0;
}