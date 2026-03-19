#include <signal.h>
#include <stdint.h>
#include <stdio.h>

#include <rte_eal.h>
#include <rte_graph.h>
#include <rte_graph_worker.h>

#define HELLO_NODE_NAME "hello_node"
#define DUMMY_NODE_NAME "dummy_node"
#define DUMMY_NODE_NAME2 "dummy_node2"

static volatile int force_quit = 0;
static int count = 0;
// -----------------------
// HELLO NODE (source)
// -----------------------
static uint16_t hello_node_process(struct rte_graph *graph,
                                   struct rte_node *node, void **objs,
                                   uint16_t nb_objs) {
  if (count == 0)
    printf("Hello from graph node!\n");
  count++;
  // Must enqueue SOMETHING to the next node, even empty
  rte_node_enqueue(graph, node, 0, objs, 0);
  return 0; // No real objects
}

static struct rte_node_register hello_node = {
    .name = HELLO_NODE_NAME,
    .process = hello_node_process,
    .flags = RTE_NODE_SOURCE_F,

    .nb_edges = 1, // MUST HAVE AT LEAST ONE EDGE
    .next_nodes =
        {
            [0] = DUMMY_NODE_NAME,  // Dummy next node
            [1] = DUMMY_NODE_NAME2, // second dummy
        },
};
RTE_NODE_REGISTER(hello_node);

// DUMMY NODE (does nothing)
static uint16_t dummy_node_process(struct rte_graph *graph,
                                   struct rte_node *node, void **objs,
                                   uint16_t nb_objs) {
  // Does nothing
  if (count == 10)
    printf("Going into dummy node 1\n");
  count++;
  return nb_objs;
}

static struct rte_node_register dummy_node = {
    .name = DUMMY_NODE_NAME,
    .process = dummy_node_process,
    .nb_edges = 0,
};
RTE_NODE_REGISTER(dummy_node);

// second dummy NODE
static uint16_t dummy_node_process2(struct rte_graph *graph,
                                    struct rte_node *node, void **objs,
                                    uint16_t nb_objs) {
  // Does nothing
  if (count == 10)
    printf("Going into dummy node 2\n");
  count++;
  return nb_objs;
}

static struct rte_node_register dummy_node2 = {
    .name = DUMMY_NODE_NAME2,
    .process = dummy_node_process,
    .nb_edges = 0,
};
RTE_NODE_REGISTER(dummy_node2);

static void signal_handler(int sig) {
  if (sig == SIGINT || sig == SIGTERM)
    force_quit = 1;
}

int main(int argc, char **argv) {
  rte_graph_t graph_id;
  struct rte_graph *graph;

  int ret = rte_eal_init(argc, argv);
  if (ret < 0)
    rte_exit(EXIT_FAILURE, "EAL init failed\n");

  signal(SIGINT, signal_handler);

  struct rte_graph_param gparam = {
      .nb_node_patterns = 1,
      .node_patterns = (const char *[]){HELLO_NODE_NAME},
  };

  graph_id = rte_graph_create("simple_graph", &gparam);
  if (graph_id == RTE_GRAPH_ID_INVALID)
    rte_exit(EXIT_FAILURE, "Graph creation failed\n");

  graph = rte_graph_lookup("simple_graph");
  if (!graph)
    rte_exit(EXIT_FAILURE, "Graph lookup failed\n");

  printf("Running minimal DPDK graph (Ctrl+C to exit)\n");

  while (!force_quit && count < 10) {
    rte_graph_walk(graph);
  }

  rte_graph_destroy(graph_id);
  return 0;
}
