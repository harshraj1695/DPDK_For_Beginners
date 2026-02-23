#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>

#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_mbuf.h>
#include <rte_lcore.h>

#define RX_RING 1024
#define NUM_MBUFS 8191
#define MBUF_CACHE 256
#define BURST 32

static uint16_t port_id = 0;
static struct rte_mempool *mbuf_pool;
static uint16_t used_rx_queues = 1;

// RX loop executed on each lcore
static int lcore_rx(void *arg)
{
    uint16_t queue_id = (uintptr_t)arg;
    struct rte_mbuf *bufs[BURST];

    printf("Lcore %u polling queue %u\n",
           rte_lcore_id(), queue_id);

    while (1) {
        uint16_t nb = rte_eth_rx_burst(
            port_id,
            queue_id,
            bufs,
            BURST);

        if (nb == 0)
            continue;

        for (int i = 0; i < nb; i++) {
            printf("Packet received on queue %u (lcore %u)\n",
                   queue_id, rte_lcore_id());
            rte_pktmbuf_free(bufs[i]);
        }
    }
    return 0;
}

int main(int argc, char **argv)
{
    int ret = rte_eal_init(argc, argv);
    if (ret < 0)
        rte_exit(EXIT_FAILURE, "EAL init failed\n");

    uint16_t nb_ports = rte_eth_dev_count_avail();
    if (nb_ports == 0)
        rte_exit(EXIT_FAILURE, "No Ethernet ports found\n");

    printf("Detected %u port(s)\n", nb_ports);

    // Create mbuf pool
    mbuf_pool = rte_pktmbuf_pool_create(
        "MBUF_POOL",
        NUM_MBUFS,
        MBUF_CACHE,
        0,
        RTE_MBUF_DEFAULT_BUF_SIZE,
        rte_socket_id());

    if (!mbuf_pool)
        rte_exit(EXIT_FAILURE, "Cannot create mbuf pool\n");

    // Query device capabilities
    struct rte_eth_dev_info dev_info;
    rte_eth_dev_info_get(port_id, &dev_info);

    printf("Driver: %s\n", dev_info.driver_name);
    printf("Max RX queues supported: %u\n",
           dev_info.max_rx_queues);

    // Decide queue count based on lcores and NIC support
    uint16_t desired_queues = rte_lcore_count() - 1;
    if (desired_queues == 0)
        desired_queues = 1;

    used_rx_queues =
        desired_queues < dev_info.max_rx_queues ?
        desired_queues : dev_info.max_rx_queues;

    if (used_rx_queues == 0)
        used_rx_queues = 1;

    printf("Using %u RX queue(s)\n", used_rx_queues);

    // Configure port
    struct rte_eth_conf port_conf = {0};

    // Enable RSS only if multiple queues are used
    if (used_rx_queues > 1) {
        printf("Enabling RSS\n");

        port_conf.rxmode.mq_mode = RTE_ETH_MQ_RX_RSS;

        port_conf.rx_adv_conf.rss_conf.rss_hf =
              RTE_ETH_RSS_IP
            | RTE_ETH_RSS_TCP
            | RTE_ETH_RSS_UDP;
    } else {
        printf("Single queue mode (RSS disabled)\n");
        port_conf.rxmode.mq_mode = RTE_ETH_MQ_RX_NONE;
    }

    // Apply configuration
    ret = rte_eth_dev_configure(
            port_id,
            used_rx_queues,
            0,
            &port_conf);

    if (ret < 0)
        rte_exit(EXIT_FAILURE, "Port configure failed\n");

    // Setup RX queues
    for (uint16_t q = 0; q < used_rx_queues; q++) {

        ret = rte_eth_rx_queue_setup(
                port_id,
                q,
                RX_RING,
                rte_eth_dev_socket_id(port_id),
                NULL,
                mbuf_pool);

        if (ret < 0)
            rte_exit(EXIT_FAILURE,
                     "RX queue setup failed\n");
    }

    // Start device
    ret = rte_eth_dev_start(port_id);
    if (ret < 0)
        rte_exit(EXIT_FAILURE, "Port start failed\n");

    printf("Port started successfully\n");

    // Launch workers
    uint16_t q = 0;
    unsigned lcore_id;

    RTE_LCORE_FOREACH_WORKER(lcore_id) {

        if (q >= used_rx_queues)
            break;

        rte_eal_remote_launch(
            lcore_rx,
            (void *)(uintptr_t)q,
            lcore_id);

        q++;
    }

    // Run remaining queue on master core if needed
    if (q < used_rx_queues)
        lcore_rx((void *)(uintptr_t)q);

    rte_eal_mp_wait_lcore();
    return 0;
}