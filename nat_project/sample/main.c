#include <rte_cycles.h>
#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_lcore.h>
#include <rte_mbuf.h>
#include <stdio.h>
#include "source.h"

#define BURST_SIZE 32
#define port_id 0

int main(int argc, char *argv[]) {
    rte_eal_init(argc, argv);

    uint64_t hz = rte_get_timer_hz();
    uint64_t start_tsc = 0;   /* set once on first packet, never touched again until reset */
    uint64_t last_rx_tsc = 0; /* set on every packet received, to track idle time */
    int started = 0;

    struct rte_mempool *mp = create_mempool();
    init_port(port_id, mp);

    // rte hash init
    struct rte_hash *ht = ip_hash_create();

    struct rte_mbuf *bufs[BURST_SIZE];

    while (1) {
        uint16_t nb_rx = rte_eth_rx_burst(port_id, 0, bufs, BURST_SIZE);

        if (nb_rx > 0) {
            if (!started) {
                /* first packet — start timer */
                start_tsc = rte_get_timer_cycles();
                started = 1;
                printf("First packet received. Timer started.\n");
            }
            hash_ips(ht, bufs, nb_rx); // process packets and store IPs in hash table

            /* always update last_rx on any packet */
            last_rx_tsc = rte_get_timer_cycles();

            /* elapsed keeps going from start_tsc, not reset on each packet */
            uint64_t elapsed = (rte_get_timer_cycles() - start_tsc) / hz;
            printf("Received %u packets | Elapsed: %lu sec\n", nb_rx, elapsed);
            uint16_t nb_tx = rte_eth_tx_burst(port_id, 0, bufs, nb_rx);
            if (nb_tx < nb_rx) {
                for (int i = nb_tx; i < nb_rx; i++)
                    rte_pktmbuf_free(bufs[i]);
            }

        } else {
            /* no packet — check idle time against last_rx, not start_tsc */
            if (started) {
                uint64_t idle = (rte_get_timer_cycles() - last_rx_tsc) / hz;
                if (idle >= 5) {
                    printf("No packets for 5 seconds. Timer reset.\n");
                    start_tsc = 0;
                    last_rx_tsc = 0;
                    started = 0;
                    hash_ips_dump(ht); // print all stored IPs before reset
                }
            }
        }
    }

    return 0;
}