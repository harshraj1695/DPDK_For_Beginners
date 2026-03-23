#include <stdio.h>
#include <stdint.h>
#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_mempool.h>
#include <rte_mbuf.h>
#include <rte_ether.h>

#define BURST_SIZE 32

int main(int argc, char **argv)
{
    /* 1. Initialize EAL */
    int ret = rte_eal_init(argc, argv);
    if (ret < 0)
        rte_exit(EXIT_FAILURE, "EAL init failed\n");

    /* 2. Count ports */
    uint16_t nb_ports = rte_eth_dev_count_avail();

    if (nb_ports < 2)
        rte_exit(EXIT_FAILURE, "Need at least 2 ports (AF_PACKET)\n");

    uint16_t rx_port = 0;
    uint16_t tx_port = 1;

    /* 3. Create mempool */
    struct rte_mempool *mbuf_pool =
        rte_pktmbuf_pool_create("MBUF_POOL",
            8192, 256, 0,
            RTE_MBUF_DEFAULT_BUF_SIZE,
            rte_socket_id());

    if (!mbuf_pool)
        rte_exit(EXIT_FAILURE, "Cannot create mbuf pool\n");

    /* 4. Configure RX port */
    struct rte_eth_conf port_conf = {0};

    if (rte_eth_dev_configure(rx_port, 1, 1, &port_conf) < 0)
        rte_exit(EXIT_FAILURE, "RX port configure failed\n");

    if (rte_eth_rx_queue_setup(rx_port, 0, 1024,
                                rte_eth_dev_socket_id(rx_port),
                                NULL, mbuf_pool) < 0)
        rte_exit(EXIT_FAILURE, "RX queue setup failed\n");

    if (rte_eth_tx_queue_setup(rx_port, 0, 1024,
                                rte_eth_dev_socket_id(rx_port),
                                NULL) < 0)
        rte_exit(EXIT_FAILURE, "RX TX queue setup failed\n");

    if (rte_eth_dev_start(rx_port) < 0)
        rte_exit(EXIT_FAILURE, "RX port start failed\n");

    rte_eth_promiscuous_enable(rx_port);

    /* 5. Configure TX port */

    if (rte_eth_dev_configure(tx_port, 1, 1, &port_conf) < 0)
        rte_exit(EXIT_FAILURE, "TX port configure failed\n");

    if (rte_eth_rx_queue_setup(tx_port, 0, 1024,
                                rte_eth_dev_socket_id(tx_port),
                                NULL, mbuf_pool) < 0)
        rte_exit(EXIT_FAILURE, "TX RX queue setup failed\n");

    if (rte_eth_tx_queue_setup(tx_port, 0, 1024,
                                rte_eth_dev_socket_id(tx_port),
                                NULL) < 0)
        rte_exit(EXIT_FAILURE, "TX queue setup failed\n");

    if (rte_eth_dev_start(tx_port) < 0)
        rte_exit(EXIT_FAILURE, "TX port start failed\n");

    rte_eth_promiscuous_enable(tx_port);

    /* 6. Main RX/TX loop */
    struct rte_mbuf *bufs[BURST_SIZE];

    while (1) {
        uint16_t nb_rx = rte_eth_rx_burst(rx_port, 0, bufs, BURST_SIZE);

        if (nb_rx == 0)
            continue;

        for (uint16_t i = 0; i < nb_rx; i++) {
            struct rte_mbuf *m = bufs[i];

            struct rte_ether_hdr *eth =
                rte_pktmbuf_mtod(m, struct rte_ether_hdr *);

            /* MAC swap */
            struct rte_ether_addr temp;
            rte_ether_addr_copy(&eth->src_addr, &temp);
            rte_ether_addr_copy(&eth->dst_addr, &eth->src_addr);
            rte_ether_addr_copy(&temp, &eth->dst_addr);

            /* Transmit */
            uint16_t nb_tx = rte_eth_tx_burst(tx_port, 0, &m, 1);
            if (nb_tx == 0)
                rte_pktmbuf_free(m);
        }
    }

    return 0;
}
