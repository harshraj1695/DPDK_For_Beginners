#include <stdio.h>
#include <rte_ethdev.h>
#include <rte_mbuf.h>

#define BURST_SIZE 32

void print_mbuf_info(struct rte_mbuf *m); // fro mk_buff_info.c
void parse_packet(struct rte_mbuf *m); // from packet_parsing.c

void run_rx_tx_loop(uint16_t port_id)
{
    struct rte_mbuf *bufs[BURST_SIZE];

    while (1) {
        uint16_t nb_rx = rte_eth_rx_burst(port_id, 0, bufs, BURST_SIZE);

        if (nb_rx == 0)
            continue;

        printf("Received %u packets\n", nb_rx);

        for (int i = 0; i < nb_rx; i++) {
            struct rte_mbuf *m = bufs[i];

            // print_mbuf_info(m);

            uint8_t *data = rte_pktmbuf_mtod(m, uint8_t *);
            uint16_t len = rte_pktmbuf_data_len(m);
           
            parse_packet(m);
            printf("Packet %d: len=%u: ", i, len);
           
            printf("\n");
        }

        uint16_t nb_tx = rte_eth_tx_burst(port_id, 0, bufs, nb_rx);

        if (nb_tx < nb_rx) {
            for (int i = nb_tx; i < nb_rx; i++)
                rte_pktmbuf_free(bufs[i]);
        }
    }
}
