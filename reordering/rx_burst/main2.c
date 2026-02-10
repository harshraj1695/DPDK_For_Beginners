#include <stdio.h>
#include <stdint.h>

#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_mbuf.h>
#include <rte_reorder.h>

#define NUM_MBUFS 8192
#define CACHE_SIZE 256
#define BURST_SIZE 32
#define WINDOW_SIZE 128

static uint16_t port_id = 0;

/* ---------------- Port Init ---------------- */

static void
init_port(uint16_t port, struct rte_mempool *mp)
{
    struct rte_eth_conf port_conf = {0};

    if (rte_eth_dev_configure(port, 1, 0, &port_conf) < 0)
        rte_exit(EXIT_FAILURE, "Port configure failed\n");

    if (rte_eth_rx_queue_setup(port, 0, 1024,
                               rte_eth_dev_socket_id(port),
                               NULL, mp) < 0)
        rte_exit(EXIT_FAILURE, "RX queue setup failed\n");

    if (rte_eth_dev_start(port) < 0)
        rte_exit(EXIT_FAILURE, "Port start failed\n");

    printf("Port %u started\n", port);
}

/* ---------------- RX + Reorder Loop ---------------- */

static void
rx_reorder_loop(struct rte_mempool *mp)
{
    struct rte_mbuf *rx[BURST_SIZE];
    struct rte_mbuf *ordered[BURST_SIZE];

    /* Create reorder buffer */
    struct rte_reorder_buffer *rbuf =
        rte_reorder_create("reorder",
                           rte_socket_id(),
                           WINDOW_SIZE);

    if (!rbuf)
        rte_exit(EXIT_FAILURE, "Reorder create failed\n");

    uint32_t seq_counter = 0;

    printf("Starting RX + reorder loop\n");

    while (1) {

        /* ---- RX burst ---- */
        uint16_t nb_rx =
            rte_eth_rx_burst(port_id, 0, rx, BURST_SIZE);

        if (nb_rx == 0)
            continue;

        /* ---- Insert into reorder ---- */
        for (uint16_t i = 0; i < nb_rx; i++) {

            struct rte_mbuf *m = rx[i];

            /* Demo: generate sequence number */
            uint32_t seq = seq_counter++;

            /* Store sequence for reorder */
            *rte_reorder_seqn(m) = seq;

            if (rte_reorder_insert(rbuf, m) < 0) {
                /* Packet dropped by reorder */
                rte_pktmbuf_free(m);
            }
        }

        /* ---- Drain ordered packets ---- */
        uint16_t nb_out =
            rte_reorder_drain(rbuf, ordered, BURST_SIZE);

        for (uint16_t i = 0; i < nb_out; i++) {

            struct rte_mbuf *m = ordered[i];

            uint32_t seq = *rte_reorder_seqn(m);

            printf("Ordered packet seq = %u\n", seq);

            /* Process / forward packet here */

            rte_pktmbuf_free(m);
        }
    }
}

/* ---------------- Main ---------------- */

int
main(int argc, char **argv)
{
    if (rte_eal_init(argc, argv) < 0)
        rte_exit(EXIT_FAILURE, "EAL init failed\n");

    struct rte_mempool *mp =
        rte_pktmbuf_pool_create("MBUF_POOL",
                                NUM_MBUFS,
                                CACHE_SIZE,
                                0,
                                RTE_MBUF_DEFAULT_BUF_SIZE,
                                rte_socket_id());

    if (!mp)
        rte_exit(EXIT_FAILURE, "Mempool create failed\n");

    init_port(port_id, mp);

    rx_reorder_loop(mp);

    return 0;
}
