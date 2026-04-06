#include "source.h"

#include <stdio.h>
#include <signal.h>

#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_mbuf.h>
#include <rte_timer.h>

#define BURST_SIZE 32
#define PORT_ID 0

// flag to stop application
static volatile int force_quit = 0;


// handle Ctrl+C
static void signal_handler(int sig)
{
    if (sig == SIGINT || sig == SIGTERM) {
        printf("\nExiting...\n");
        force_quit = 1;
    }
}


int main(int argc, char *argv[])
{
    // initialize DPDK EAL
    if (rte_eal_init(argc, argv) < 0) {
        printf("EAL init failed\n");
        return -1;
    }

    // register signal handler
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    // init timer subsystem (required for rte_timer)
    rte_timer_subsystem_init();

    // create packet buffer pool
    struct rte_mempool *mp = create_mempool();
    if (!mp) {
        printf("Mempool creation failed\n");
        return -1;
    }

    // initialize NIC port
    init_port(PORT_ID, mp);
     
    // initialize NAT with public IP
    uint32_t public_ip = RTE_IPV4(1, 2, 3, 4); // change this
    if (nat_init(rte_cpu_to_be_32(public_ip)) < 0) {
        printf("NAT init failed\n");
        return -1;
    }

    printf("NAT started...\n");

    struct rte_mbuf *bufs[BURST_SIZE];

    // main packet loop
    while (!force_quit) {

        // receive packets
        uint16_t nb_rx = rte_eth_rx_burst(PORT_ID, 0, bufs, BURST_SIZE);

        if (nb_rx == 0) {
            // still run timers even if no packets
            nat_timer_manage();
            continue;
        }

        uint16_t tx_count = 0;

        // process each packet
        for (uint16_t i = 0; i < nb_rx; i++) {

            // process NAT
            // return 0 = forward, -1 = drop
            if (nat_process_packet(bufs[i]) == 0) {
                bufs[tx_count++] = bufs[i];
            } else {
                rte_pktmbuf_free(bufs[i]); // drop packet
            }
        }

        // transmit processed packets
        uint16_t sent = rte_eth_tx_burst(PORT_ID, 0, bufs, tx_count);

        // free unsent packets
        if (sent < tx_count) {
            for (uint16_t i = sent; i < tx_count; i++) {
                rte_pktmbuf_free(bufs[i]);
            }
        }

        // run NAT timers
        nat_timer_manage();
    }

    // cleanup on exit
    printf("Cleaning up...\n");

    rte_eth_dev_stop(PORT_ID);
    rte_eth_dev_close(PORT_ID);

    return 0;
}