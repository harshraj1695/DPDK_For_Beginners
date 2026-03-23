#include <stdio.h>
#include <stdint.h>
#include <rte_eal.h>
#include <rte_graph.h>
#include <rte_graph_worker.h>
#include <rte_mbuf.h>
#include <rte_mempool.h>
#include <rte_cycles.h>

#define NUM_PKTS 100000
#define BURST 32

/* ---------------- NORMAL PIPELINE ---------------- */

static inline void parse(struct rte_mbuf *m)
{
    uint8_t *data = rte_pktmbuf_mtod(m, uint8_t *);
    data[0] ^= 1;
}

static inline void classify(struct rte_mbuf *m)
{
    uint8_t *data = rte_pktmbuf_mtod(m, uint8_t *);
    data[1] ^= 2;
}

static inline void forward(struct rte_mbuf *m)
{
    uint8_t *data = rte_pktmbuf_mtod(m, uint8_t *);
    data[2] ^= 3;
}

void run_normal_pipeline(struct rte_mbuf **pkts)
{
    for (int i = 0; i < NUM_PKTS; i++) {
        parse(pkts[i]);
        classify(pkts[i]);
        forward(pkts[i]);
    }
}

/* ---------------- GRAPH NODES ---------------- */

enum {
    START_NEXT_PARSE,
};

static uint16_t
start_node_process(struct rte_graph *graph, struct rte_node *node,
                   void **objs, uint16_t nb_objs)
{
    rte_node_enqueue(graph, node, START_NEXT_PARSE, objs, nb_objs);
    return nb_objs;
}

enum {
    PARSE_NEXT_CLASSIFY,
};

static uint16_t
parse_node_process(struct rte_graph *graph, struct rte_node *node,
                   void **objs, uint16_t nb_objs)
{
    for (int i = 0; i < nb_objs; i++)
        parse(objs[i]);

    rte_node_enqueue(graph, node, PARSE_NEXT_CLASSIFY, objs, nb_objs);
    return nb_objs;
}

enum {
    CLASSIFY_NEXT_FORWARD,
};

static uint16_t
classify_node_process(struct rte_graph *graph, struct rte_node *node,
                      void **objs, uint16_t nb_objs)
{
    for (int i = 0; i < nb_objs; i++)
        classify(objs[i]);

    rte_node_enqueue(graph, node, CLASSIFY_NEXT_FORWARD, objs, nb_objs);
    return nb_objs;
}

static uint16_t
forward_node_process(struct rte_graph *graph, struct rte_node *node,
                     void **objs, uint16_t nb_objs)
{
    for (int i = 0; i < nb_objs; i++)
        forward(objs[i]);

    return nb_objs;
}

/* ---------------- NODE REGISTRATION ---------------- */

static struct rte_node_register start_node = {
    .name = "start_node",
    .flags = RTE_NODE_SOURCE_F,   // IMPORTANT
    .process = start_node_process,
    .nb_edges = 1,
    .next_nodes = { "parse_node" },
};
RTE_NODE_REGISTER(start_node);

static struct rte_node_register parse_node = {
    .name = "parse_node",
    .process = parse_node_process,
    .nb_edges = 1,
    .next_nodes = { "classify_node" },
};
RTE_NODE_REGISTER(parse_node);

static struct rte_node_register classify_node = {
    .name = "classify_node",
    .process = classify_node_process,
    .nb_edges = 1,
    .next_nodes = { "forward_node" },
};
RTE_NODE_REGISTER(classify_node);

static struct rte_node_register forward_node = {
    .name = "forward_node",
    .process = forward_node_process,
    .nb_edges = 0,
};
RTE_NODE_REGISTER(forward_node);

/* ---------------- MAIN ---------------- */

int main(int argc, char **argv)
{
    rte_eal_init(argc, argv);

    struct rte_mempool *mp =
        rte_pktmbuf_pool_create("MBUF_POOL",
                                NUM_PKTS,
                                256,
                                0,
                                RTE_MBUF_DEFAULT_BUF_SIZE,
                                rte_socket_id());

    if (!mp) {
        printf("mempool creation failed\n");
        return -1;
    }

    struct rte_mbuf *pkts[NUM_PKTS];

    for (int i = 0; i < NUM_PKTS; i++)
        pkts[i] = rte_pktmbuf_alloc(mp);

    /* -------- NORMAL PIPELINE TEST -------- */

    uint64_t start = rte_rdtsc();

    run_normal_pipeline(pkts);

    uint64_t end = rte_rdtsc();

    printf("Normal pipeline cycles: %lu\n", end - start);

    /* -------- GRAPH CREATION -------- */

    const char *patterns[] = {
        "start_node",
        "parse_node",
        "classify_node",
        "forward_node"
    };

    struct rte_graph_param gconf = {
        .socket_id = rte_socket_id(),
        .nb_node_patterns = 4,
        .node_patterns = patterns,
    };

    rte_graph_t graph_id = rte_graph_create("demo_graph", &gconf);

    if (graph_id == RTE_GRAPH_ID_INVALID) {
        printf("Graph creation failed\n");
        return -1;
    }

    struct rte_graph *graph = rte_graph_lookup("demo_graph");

    rte_node_t start_node_id = rte_node_from_name("start_node");

    struct rte_node *start_node_ptr =
        rte_graph_node_get(graph_id, start_node_id);

    /* -------- GRAPH PIPELINE TEST -------- */

    start = rte_rdtsc();

    for (int i = 0; i < NUM_PKTS; i += BURST) {

        rte_node_enqueue(graph,
                         start_node_ptr,
                         0,
                         (void **)&pkts[i],
                         BURST);

        rte_graph_walk(graph);
    }

    end = rte_rdtsc(); 

    printf("Graph pipeline cycles: %lu\n", end - start);

    return 0;
}