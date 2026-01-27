#ifndef PIPELINE_H
#define PIPELINE_H

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

// Node names
#define RX_NODE_NAME    "rx_node"
#define L2_NODE_NAME    "l2_node"
#define IPV4_NODE_NAME  "ipv4_node"
#define IPV6_NODE_NAME  "ipv6_node"
#define TX_NODE_NAME    "tx_node"

// Node process function prototypes
uint16_t rx_node_func(struct rte_graph *graph, struct rte_node *node,
                      void **objs, uint16_t nb_objs);

uint16_t l2_node_func(struct rte_graph *graph, struct rte_node *node,
                      void **objs, uint16_t nb_objs);

uint16_t ipv4_node_func(struct rte_graph *graph, struct rte_node *node,
                        void **objs, uint16_t nb_objs);

uint16_t ipv6_node_func(struct rte_graph *graph, struct rte_node *node,
                        void **objs, uint16_t nb_objs);

uint16_t tx_node_func(struct rte_graph *graph, struct rte_node *node,
                      void **objs, uint16_t nb_objs);

// Initialization functions
void init_port(void);
struct rte_graph *init_graph(void);

#endif
