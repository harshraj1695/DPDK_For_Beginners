#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <signal.h>

#include <rte_common.h>
#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_mbuf.h>
#include <rte_ip.h>
#include <rte_ether.h>
#include <rte_udp.h>
#include <rte_tcp.h>

#define RX_RING_SIZE 1024
#define TX_RING_SIZE 1024
#define NUM_MBUFS 8192
#define MBUF_CACHE_SIZE 250
#define BURST_SIZE 32

// Stop the packet loop when SIGINT or SIGTERM arrives.
static volatile bool force_quit = false;
static uint16_t nb_ports;
static uint8_t port_id = 0;
static uint32_t next_backend = 0;

struct backend {
    uint32_t ip;   // Stored in network byte order.
    uint16_t port; // Stored in network byte order.
};

// Backends used by the round-robin selector.
static struct backend backends[] = {
    {RTE_IPV4(192,168,0,101), 0},
    {RTE_IPV4(192,168,0,102), 0},
    {RTE_IPV4(192,168,0,103), 0},
};
static const unsigned n_backends = RTE_DIM(backends);

static struct rte_mempool *mbuf_pool;

static void
signal_handler(int signum)
{
    if (signum == SIGINT || signum == SIGTERM)
        force_quit = true;
}

static uint64_t backend_hits[RTE_DIM(backends)] = {0};

static inline int
rewrite_l4_packet(struct rte_mbuf *m)
{
    // Read the Ethernet header and only continue for IPv4 packets.
    struct rte_ether_hdr *eth = rte_pktmbuf_mtod(m, struct rte_ether_hdr *);
    if (eth->ether_type != rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4)) return -1;

    struct rte_ipv4_hdr *ip = (struct rte_ipv4_hdr *)(eth + 1);
    if (ip->next_proto_id != IPPROTO_TCP && ip->next_proto_id != IPPROTO_UDP) return -1;

    // Pick the next backend and rewrite the destination address.
    int selected = next_backend;
    struct backend *bk = &backends[selected];
    next_backend = (next_backend + 1) % n_backends;

    ip->dst_addr = bk->ip;

    if (ip->next_proto_id == IPPROTO_TCP) {
        // Update the TCP destination port and checksum.
        struct rte_tcp_hdr *tcp = (struct rte_tcp_hdr *)((unsigned char *)ip + sizeof(struct rte_ipv4_hdr));
        tcp->dst_port = bk->port;
        tcp->cksum = 0;
        tcp->cksum = rte_ipv4_udptcp_cksum(ip, tcp);
    } else if (ip->next_proto_id == IPPROTO_UDP) {
        // Update the UDP destination port and checksum.
        struct rte_udp_hdr *udp = (struct rte_udp_hdr *)((unsigned char *)ip + sizeof(struct rte_ipv4_hdr));
        udp->dst_port = bk->port;
        udp->dgram_cksum = 0;
        udp->dgram_cksum = rte_ipv4_udptcp_cksum(ip, udp);
    }

    // Recompute the IPv4 header checksum after rewriting.
    ip->hdr_checksum = 0;
    ip->hdr_checksum = rte_ipv4_cksum(ip);

    backend_hits[selected]++;
    return selected;
}

static void
lcore_main(void)
{
    uint16_t port = port_id;
    uint64_t total_rx = 0;
    uint64_t total_tx = 0;
    uint64_t total_drop = 0;
    uint64_t period_rx = 0;
    unsigned print_counter = 0;

    while (!force_quit) {
        // Pull a burst of packets from RX queue 0.
        struct rte_mbuf *pkts[BURST_SIZE];
        const uint16_t nb_rx = rte_eth_rx_burst(port, 0, pkts, BURST_SIZE);
        if (nb_rx == 0)
            continue;

        total_rx += nb_rx;
        period_rx += nb_rx;

        for (uint16_t i = 0; i < nb_rx; i++) {
            // Rewrite supported L4 packets before transmission.
            struct rte_mbuf *m = pkts[i];
            int selected = rewrite_l4_packet(m);
            if (selected >= 0 && (period_rx % 1000) == 0) {
                printf("selected backend=%d dst=%" PRIu32 "\n", selected, rte_be_to_cpu_32(backends[selected].ip));
            }
        }

        // Send the burst back out on TX queue 0.
        const uint16_t nb_tx = rte_eth_tx_burst(port, 0, pkts, nb_rx);
        total_tx += nb_tx;
        total_drop += (nb_rx - nb_tx);

        if (unlikely(nb_tx < nb_rx)) {
            // Free packets that were not transmitted.
            for (uint16_t j = nb_tx; j < nb_rx; j++)
                rte_pktmbuf_free(pkts[j]);
        }

        // Print simple counters every few loop iterations.
        if (++print_counter == 1000) {
            print_counter = 0;
            printf("stats: total_rx=%" PRIu64 " total_tx=%" PRIu64 " drop=%" PRIu64 "\n",
                   total_rx, total_tx, total_drop);
            for (unsigned i = 0; i < n_backends; i++) {
                printf("    backend %u hits = %" PRIu64 "\n", i, backend_hits[i]);
            }
        }
    }
}

int
main(int argc, char *argv[])
{
    int ret = rte_eal_init(argc, argv);
    if (ret < 0)
        rte_exit(EXIT_FAILURE, "EAL init failed\n");

    argc -= ret;
    argv += ret;

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    nb_ports = rte_eth_dev_count_avail();
    if (nb_ports < 1)
        rte_exit(EXIT_FAILURE, "No Ethernet ports - bye\n");

    uint16_t port;

    mbuf_pool = rte_pktmbuf_pool_create("MBUF_POOL", NUM_MBUFS * nb_ports,
        MBUF_CACHE_SIZE, 0, RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());
    if (mbuf_pool == NULL)
        rte_exit(EXIT_FAILURE, "Cannot create mbuf pool\n");

    // Initialize backend destination ports in network byte order.
    for (unsigned i = 0; i < n_backends; i++) {
        backends[i].port = rte_cpu_to_be_16(10000);
    }

    // Use a basic single-queue port configuration.
    struct rte_eth_conf port_conf = {0};

    RTE_ETH_FOREACH_DEV(port) {
        ret = rte_eth_dev_configure(port, 1, 1, &port_conf);
        if (ret < 0)
            rte_exit(EXIT_FAILURE, "Cannot configure device: err=%d, port=%u\n", ret, port);

        ret = rte_eth_rx_queue_setup(port, 0, RX_RING_SIZE,
            rte_eth_dev_socket_id(port), NULL, mbuf_pool);
        if (ret < 0)
            rte_exit(EXIT_FAILURE, "rte_eth_rx_queue_setup:err=%d, port=%u\n", ret, port);

        ret = rte_eth_tx_queue_setup(port, 0, TX_RING_SIZE,
            rte_eth_dev_socket_id(port), NULL);
        if (ret < 0)
            rte_exit(EXIT_FAILURE, "rte_eth_tx_queue_setup:err=%d, port=%u\n", ret, port);

        ret = rte_eth_dev_start(port);
        if (ret < 0)
            rte_exit(EXIT_FAILURE, "rte_eth_dev_start: err=%d, port=%u\n", ret, port);

        rte_eth_promiscuous_enable(port);
        printf("Port %u started\n", port);

        port_id = port; // Keep the last initialized port as the active port.
    }

    printf("Starting main loop on lcore %u\n", rte_lcore_id());
    lcore_main();

    RTE_ETH_FOREACH_DEV(port) {
        rte_eth_dev_stop(port);
        rte_eth_dev_close(port);
    }

    printf("Bye\n");
    return 0;
}
