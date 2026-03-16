#include <stdio.h>
#include <stdint.h>
#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_cycles.h>
#include <rte_prefetch.h>
#include <rte_mbuf.h>

#define NUM_MBUFS 8191
#define MBUF_CACHE_SIZE 250
#define BURST_SIZE 32
#define PREFETCH_OFFSET 4

static inline void
process_without_prefetch(struct rte_mbuf **pkts, uint16_t nb)
{
    for (int i = 0; i < nb; i++) {
        uint8_t *data = rte_pktmbuf_mtod(pkts[i], uint8_t *);
        volatile uint8_t x = data[0];
        (void)x;
    }
}

static inline void
process_with_prefetch(struct rte_mbuf **pkts, uint16_t nb)
{
    for (int i = 0; i < nb; i++) {

        if (i + PREFETCH_OFFSET < nb)
            rte_prefetch0(rte_pktmbuf_mtod(pkts[i + PREFETCH_OFFSET], void *));

        uint8_t *data = rte_pktmbuf_mtod(pkts[i], uint8_t *);
        volatile uint8_t x = data[0];
        (void)x;
    }
}

int main(int argc, char **argv)
{
    struct rte_mempool *mbuf_pool;
    struct rte_mbuf *bufs[BURST_SIZE];

    int ret = rte_eal_init(argc, argv);
    if (ret < 0)
        rte_exit(EXIT_FAILURE, "EAL init failed\n");

    uint16_t port_id = 0;

    mbuf_pool = rte_pktmbuf_pool_create("MBUF_POOL",
        NUM_MBUFS * 2,
        MBUF_CACHE_SIZE,
        0,
        RTE_MBUF_DEFAULT_BUF_SIZE,
        rte_socket_id());

    if (mbuf_pool == NULL)
        rte_exit(EXIT_FAILURE, "Cannot create mbuf pool\n");

    struct rte_eth_conf port_conf = {0};

    rte_eth_dev_configure(port_id, 1, 0, &port_conf);
    rte_eth_rx_queue_setup(port_id, 0, 1024,
                           rte_eth_dev_socket_id(port_id),
                           NULL, mbuf_pool);

    rte_eth_dev_start(port_id);

    printf("Starting packet processing test...\n");

    uint64_t t1, t2;

    while (1) {

        uint16_t nb_rx = rte_eth_rx_burst(port_id, 0, bufs, BURST_SIZE);

        if (nb_rx == 0)
            continue;

        t1 = rte_rdtsc();
        process_without_prefetch(bufs, nb_rx);
        t2 = rte_rdtsc();

        printf("No prefetch cycles: %lu\n", t2 - t1);

        t1 = rte_rdtsc();
        process_with_prefetch(bufs, nb_rx);
        t2 = rte_rdtsc();

        printf("With prefetch cycles: %lu\n", t2 - t1);

        for (int i = 0; i < nb_rx; i++)
            rte_pktmbuf_free(bufs[i]);
    }
}