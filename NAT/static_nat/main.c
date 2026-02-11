#include <stdio.h>
#include <stdint.h>

#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_mbuf.h>
#include <rte_ether.h>
#include <rte_ip.h>

#define NUM_MBUFS 8192
#define MBUF_CACHE_SIZE 256
#define BURST_SIZE 32

#define PORT_ID 0

#define PRIVATE_IP RTE_IPV4(172,22,116,70)
#define PUBLIC_IP  RTE_IPV4(203,0,113,10)


static struct rte_mempool *mbuf_pool;
static void print_ip(uint32_t ip)
{
    ip = rte_be_to_cpu_32(ip);

    printf("%u.%u.%u.%u",
        (ip >> 24) & 0xff,
        (ip >> 16) & 0xff,
        (ip >> 8) & 0xff,
        ip & 0xff);
}

// Simulate Static NAT by modifying IP addresses
static inline void
simulate_nat(struct rte_mbuf *mbuf)
{
    struct rte_ether_hdr *eth_hdr;
    struct rte_ipv4_hdr *ip_hdr;

    eth_hdr = rte_pktmbuf_mtod(mbuf, struct rte_ether_hdr *);

    if (eth_hdr->ether_type != rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4))
        return;

    ip_hdr = (struct rte_ipv4_hdr *)(eth_hdr + 1);

    printf("\nBefore NAT: SRC=");
    print_ip(ip_hdr->src_addr);
    printf(" DST=");
    print_ip(ip_hdr->dst_addr);
    printf("\n");

    // Outbound simulation
    if (ip_hdr->src_addr == rte_cpu_to_be_32(PRIVATE_IP)) {
        ip_hdr->src_addr = rte_cpu_to_be_32(PUBLIC_IP);
        printf("Outbound Static NAT Applied\n");
    }
// Inbound simulation
    if (ip_hdr->dst_addr == rte_cpu_to_be_32(PUBLIC_IP)) {
        ip_hdr->dst_addr = rte_cpu_to_be_32(PRIVATE_IP);
        printf("Inbound Static NAT Applied\n");
    }

    // checksum update because of IP address change
    ip_hdr->hdr_checksum = 0;
    ip_hdr->hdr_checksum = rte_ipv4_cksum(ip_hdr);

    printf("After NAT:  SRC=");
    print_ip(ip_hdr->src_addr);
    printf(" DST=");
    print_ip(ip_hdr->dst_addr);
    printf("\n");
}

static int
lcore_main(void *arg)
{
    struct rte_mbuf *bufs[BURST_SIZE];

    printf("Single Port NAT Simulation Started\n");

    while (1) {

        uint16_t nb_rx =
            rte_eth_rx_burst(PORT_ID, 0, bufs, BURST_SIZE);

        if (nb_rx == 0)
            continue;

        for (int i = 0; i < nb_rx; i++) {
            simulate_nat(bufs[i]);
        }

// transmit back out the same port
        rte_eth_tx_burst(PORT_ID, 0, bufs, nb_rx);
    }

    return 0;
}

int main(int argc, char **argv)
{
    int ret = rte_eal_init(argc, argv);
    if (ret < 0)
        rte_exit(EXIT_FAILURE, "EAL init failed\n");

    mbuf_pool = rte_pktmbuf_pool_create("MBUF_POOL",
                    NUM_MBUFS,
                    MBUF_CACHE_SIZE,
                    0,
                    RTE_MBUF_DEFAULT_BUF_SIZE,
                    rte_socket_id());

    if (!mbuf_pool)
        rte_exit(EXIT_FAILURE, "Cannot create mbuf pool\n");

    struct rte_eth_conf port_conf = {0};

    rte_eth_dev_configure(PORT_ID, 1, 1, &port_conf);

    rte_eth_rx_queue_setup(PORT_ID, 0, 1024,
        rte_eth_dev_socket_id(PORT_ID),
        NULL, mbuf_pool);

    rte_eth_tx_queue_setup(PORT_ID, 0, 1024,
        rte_eth_dev_socket_id(PORT_ID),
        NULL);

    rte_eth_dev_start(PORT_ID);
    rte_eth_promiscuous_enable(PORT_ID);

    lcore_main(NULL);

    return 0;
}
