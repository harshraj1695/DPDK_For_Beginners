#include <stdio.h>
#include <stdint.h>
#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_mempool.h>
#include <rte_mbuf.h>

#define BURST_SIZE 32

int main(int argc, char **argv)
{
    /* 1. Initialize EAL */
    int ret = rte_eal_init(argc, argv);
    if (ret < 0)
        rte_exit(EXIT_FAILURE, "EAL init failed\n");

    printf("EAL Initialized.\n");

    /* 2. Count ports */
    uint16_t nb_ports = rte_eth_dev_count_avail();
    printf("Ports available: %u\n", nb_ports);

    if (nb_ports == 0)
        rte_exit(EXIT_FAILURE, "No ports found!\n");

    uint16_t port_id = 0;

    /* 3. Create mempool */
    struct rte_mempool *mbuf_pool =
        rte_pktmbuf_pool_create("MBUF_POOL",
            8192, 256, 0,
            RTE_MBUF_DEFAULT_BUF_SIZE,
            rte_socket_id());

    if (!mbuf_pool)
        rte_exit(EXIT_FAILURE, "Cannot create mbuf pool\n");

    /* 4. Configure the port */
    struct rte_eth_conf port_conf = {0};
    rte_eth_dev_configure(port_id, 1, 1, &port_conf);

    /* 5. Setup RX queue */
    rte_eth_rx_queue_setup(port_id, 0, 1024,
                            rte_eth_dev_socket_id(port_id),
                            NULL, mbuf_pool);

    /* 6. Setup TX queue */
    rte_eth_tx_queue_setup(port_id, 0, 1024,
                            rte_eth_dev_socket_id(port_id),
                            NULL);

    /* 7. Start port */
    rte_eth_dev_start(port_id);
    printf("Port %u started.\n", port_id);

    /* 8. Main RX/TX loop */
    struct rte_mbuf *bufs[BURST_SIZE];

    while (1) {
        uint16_t nb_rx = rte_eth_rx_burst(port_id, 0, bufs, BURST_SIZE);

        if (nb_rx == 0)
            continue;

        printf("Received %u packets\n", nb_rx);

        /* Print packet contents */
        for (int i = 0; i < nb_rx; i++) {
            struct rte_mbuf *m = bufs[i];
            uint8_t *data = rte_pktmbuf_mtod(m, uint8_t *);
            uint16_t len = rte_pktmbuf_data_len(m);

            printf("Packet %d: len=%u : ", i, len);
            for (int j = 0; j < len; j++)
                printf("%02x ", data[j]);
            printf("\n");
        }

        /* Echo packets back (send RX → TX) */
        uint16_t nb_tx = rte_eth_tx_burst(port_id, 0, bufs, nb_rx);

        /* Free unsent packets */
        if (nb_tx < nb_rx) {
            for (int i = nb_tx; i < nb_rx; i++)
                rte_pktmbuf_free(bufs[i]);
        }
    }

    /* never reached */
    return 0;
}
