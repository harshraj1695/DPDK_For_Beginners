#include "source.h"

#include <rte_cycles.h>
#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_lcore.h>
#include <rte_mbuf.h>
#include <rte_timer.h>
#include <stdio.h>

#define BURST_SIZE      32
#define port_id         0
#define TABLE_TIMEOUT_SEC 10

#define NAT_POOL_START "203.0.113.10"
#define NAT_POOL_RANGE  5

int main(int argc, char *argv[]) {
    rte_eal_init(argc, argv);

    // must init before any rte_timer usage
    rte_timer_subsystem_init();

    uint64_t hz = rte_get_timer_hz();

    struct rte_mempool *mp = create_mempool();
    init_port(port_id, mp);

    // IP tracking hash — only for non-NAT IPs
    struct rte_hash *ht = ip_hash_create();

    // load outbound NAT pool
    struct nat_pool *pool;
    int option = 0;
    printf("Enter 1 to load outbound pool from nat.txt, 0 for default config\n");
    (void)scanf("%d", &option);

    if (option == 0)
        pool = nat_pool_create(NAT_POOL_START, NAT_POOL_RANGE, hz);
    else
        pool = nat_load_from_file("nat.txt", hz);

    if (pool == NULL)
        rte_exit(EXIT_FAILURE, "Failed to create NAT pool\n");

    // load inbound static mappings from file
    nat_inbound_load_from_file(pool, "nat_inbound.txt");

    struct rte_mbuf *bufs[BURST_SIZE];

    while (1) {
        // drive both timers every iteration
        rte_timer_manage();

        uint16_t nb_rx = rte_eth_rx_burst(port_id, 0, bufs, BURST_SIZE);

        if (nb_rx > 0) {
            // arm table_timer on first packet
            if (!pool->started) {
                pool->started = 1;
                printf("First packet. Table timer armed (%ds timeout).\n",
                       TABLE_TIMEOUT_SEC);
            }

            // reset table_timer on EVERY packet — any traffic resets the 10s countdown
            // only fires if no packet arrives for 10 consecutive seconds
            nat_table_timer_arm(pool, hz, TABLE_TIMEOUT_SEC);

            // store non-NAT IPs for monitoring
            // hash_ips(ht, pool->outbound_map, bufs, nb_rx);

            // apply NAT — pure packet processing, no timer logic inside
            nat_pool_apply(pool, bufs, nb_rx);

            printf("Received %u packets\n", nb_rx);

            // tx — skip NULL bufs dropped by pool exhaustion or inbound drop
            for (int i = 0; i < nb_rx; i++) {
                if (bufs[i] == NULL)
                    continue;
                rte_eth_tx_burst(port_id, 0, &bufs[i], 1);
            }
        }
        // no else needed — table_timer fires on its own via rte_timer_manage()
    }

    return 0;
}
