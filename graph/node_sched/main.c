#include <stdio.h>
#include <stdint.h>
#include <signal.h>

#include <rte_eal.h>
#include <rte_graph.h>
#include <rte_graph_worker.h>

#define HELLO_NODE_NAME "hello_node"
#define DUMMY_NODE_NAME "dummy_node"
#define DUMMY_NODE_NAME2 "dummy_node2"

static volatile int force_quit = 0;
static int count = 0;

// HELLO NODE (source)
static uint16_t
hello_node_process(struct rte_graph *graph,
                   struct rte_node *node,
                   void **objs,
                   uint16_t nb_objs)
{
    count++;

    // two different dummy objects
    //static int dummy1, dummy2;

    if (count % 2 == 0) {
        printf("Hello -> dummy1\n");
        //objs[0] = &dummy1;
        rte_node_enqueue(graph, node, 0, objs, 1);
    } else {
        printf("Hello -> dummy2\n");
        //objs[0] = &dummy2;
        rte_node_enqueue(graph, node, 1, objs, 1);
    }

    return 0;     // MUST return 1 or scheduler won't run children
}

static struct rte_node_register hello_node = {
    .name = HELLO_NODE_NAME,
    .process = hello_node_process,
    .flags = RTE_NODE_SOURCE_F,

    .nb_edges = 2,
    .next_nodes = {
        [0] = DUMMY_NODE_NAME,
        [1] = DUMMY_NODE_NAME2,
    },
};
RTE_NODE_REGISTER(hello_node);

// DUMMY NODE 1
static uint16_t
dummy_node_process(struct rte_graph *graph,
                   struct rte_node *node,
                   void **objs,
                   uint16_t nb_objs)
{
    printf(">>> dummy 1 executed\n");
    return 0;   // no more output
}

static struct rte_node_register dummy_node = {
    .name = DUMMY_NODE_NAME,
    .process = dummy_node_process,
    .nb_edges = 0,
};
RTE_NODE_REGISTER(dummy_node);

// DUMMY NODE 2
static uint16_t
dummy_node_process2(struct rte_graph *graph,
                   struct rte_node *node,
                   void **objs,
                   uint16_t nb_objs)
{
    printf(">>> dummy 2 executed\n");
    return 0;
}

static struct rte_node_register dummy_node2 = {
    .name = DUMMY_NODE_NAME2,
    .process = dummy_node_process2,
    .nb_edges = 0,
};
RTE_NODE_REGISTER(dummy_node2);


// MAIN
static void signal_handler(int sig)
{
    if (sig == SIGINT || sig == SIGTERM)
        force_quit = 1;
}

int main(int argc, char **argv)
{
    rte_graph_t graph_id;
    struct rte_graph *graph;

    int ret = rte_eal_init(argc, argv);
    if (ret < 0)
        rte_exit(EXIT_FAILURE, "EAL init failed\n");

    signal(SIGINT, signal_handler);

    struct rte_graph_param gparam = {
        .nb_node_patterns = 1,
        .node_patterns = (const char *[]) { HELLO_NODE_NAME },
    };

    graph_id = rte_graph_create("simple_graph", &gparam);
    if (graph_id == RTE_GRAPH_ID_INVALID)
        rte_exit(EXIT_FAILURE, "Graph creation failed\n");

    graph = rte_graph_lookup("simple_graph");
    if (!graph)
        rte_exit(EXIT_FAILURE, "Graph lookup failed\n");

    printf("Running minimal DPDK graph...\n");

    while (!force_quit && count < 10) {
        rte_graph_walk(graph);
    }

    rte_graph_destroy(graph_id);
    return 0;
}
