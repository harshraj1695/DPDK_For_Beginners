#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <unistd.h>
#include <arpa/inet.h>

#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_mbuf.h>
#include <rte_ip.h>
#include <rte_udp.h>
#include <rte_ether.h>
#include <rte_vxlan.h>

#define NUM_MBUFS 8191
#define MBUF_CACHE_SIZE 250
#define BURST_SIZE 32

#define VXLAN_PORT 4789
#define VXLAN_VNI  100

// debug flags
#define DEBUG_RX 1
#define DEBUG_TX 1
#define DEBUG_VXLAN 1

// helper macro for IPv4
#define MAKE_IPV4(a,b,c,d) \
    ((uint32_t)(((a) & 0xff) << 24 | ((b) & 0xff) << 16 | ((c) & 0xff) << 8 | ((d) & 0xff)))

static const struct rte_eth_conf port_conf_default = {
    .rxmode = { }
};

struct rte_mempool *mbuf_pool;

// vxlan encapsulation
static void vxlan_encap(struct rte_mbuf *m) {
    struct rte_ether_hdr *eth;
    struct rte_ipv4_hdr *ip;
    struct rte_udp_hdr *udp;
    struct rte_vxlan_hdr *vxlan;

    uint16_t pkt_len = rte_pktmbuf_pkt_len(m);

    char *ptr = rte_pktmbuf_prepend(m,
        sizeof(struct rte_ether_hdr) +
        sizeof(struct rte_ipv4_hdr) +
        sizeof(struct rte_udp_hdr) +
        sizeof(struct rte_vxlan_hdr));

    eth = (struct rte_ether_hdr *)ptr;
    ip  = (struct rte_ipv4_hdr *)(eth + 1);
    udp = (struct rte_udp_hdr *)(ip + 1);
    vxlan = (struct rte_vxlan_hdr *)(udp + 1);

    struct rte_ether_addr src = {{0x08,0x00,0x27,0x12,0x34,0x56}};
    struct rte_ether_addr dst = {{0x08,0x00,0x27,0xab,0xcd,0xef}};

    rte_ether_addr_copy(&dst, &eth->dst_addr);
    rte_ether_addr_copy(&src, &eth->src_addr);
    eth->ether_type = rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4);

    ip->version_ihl = 0x45;
    ip->type_of_service = 0;
    ip->total_length = rte_cpu_to_be_16(sizeof(struct rte_ipv4_hdr) +
                                       sizeof(struct rte_udp_hdr) +
                                       sizeof(struct rte_vxlan_hdr) +
                                       pkt_len);
    ip->packet_id = 0;
    ip->fragment_offset = 0;
    ip->time_to_live = 64;
    ip->next_proto_id = IPPROTO_UDP;
    ip->src_addr = rte_cpu_to_be_32(MAKE_IPV4(10,0,0,1));
    ip->dst_addr = rte_cpu_to_be_32(MAKE_IPV4(10,0,0,2));
    ip->hdr_checksum = rte_ipv4_cksum(ip);

    udp->src_port = rte_cpu_to_be_16(12345);
    udp->dst_port = rte_cpu_to_be_16(VXLAN_PORT);
    udp->dgram_len = rte_cpu_to_be_16(sizeof(struct rte_udp_hdr) +
                                      sizeof(struct rte_vxlan_hdr) +
                                      pkt_len);
    udp->dgram_cksum = 0;

    vxlan->vx_flags = rte_cpu_to_be_32(0x08000000);
    vxlan->vx_vni = rte_cpu_to_be_32(VXLAN_VNI << 8);

    if (DEBUG_VXLAN)
        printf("Encapsulated packet into VXLAN VNI=%d len=%u\n", VXLAN_VNI, pkt_len);
}

// vxlan decapsulation
static int vxlan_decap(struct rte_mbuf *m) {
    struct rte_ether_hdr *eth;
    struct rte_ipv4_hdr *ip;
    struct rte_udp_hdr *udp;

    eth = rte_pktmbuf_mtod(m, struct rte_ether_hdr *);

    if (eth->ether_type != rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4))
        return -1;

    ip = (struct rte_ipv4_hdr *)(eth + 1);

    if (ip->next_proto_id != IPPROTO_UDP)
        return -1;

    udp = (struct rte_udp_hdr *)(ip + 1);

    if (udp->dst_port != rte_cpu_to_be_16(VXLAN_PORT))
        return -1;

    rte_pktmbuf_adj(m,
        sizeof(struct rte_ether_hdr) +
        sizeof(struct rte_ipv4_hdr) +
        sizeof(struct rte_udp_hdr) +
        sizeof(struct rte_vxlan_hdr));

    if (DEBUG_VXLAN)
        printf("Decapsulated VXLAN packet\n");

    return 0;
}

// port initialization
static inline int port_init(uint16_t port) {
    struct rte_eth_conf port_conf = port_conf_default;
    int retval;

    retval = rte_eth_dev_configure(port, 1, 1, &port_conf);
    if (retval != 0) return retval;

    retval = rte_eth_rx_queue_setup(port, 0, 128,
            rte_eth_dev_socket_id(port), NULL, mbuf_pool);
    if (retval < 0) return retval;

    retval = rte_eth_tx_queue_setup(port, 0, 512,
            rte_eth_dev_socket_id(port), NULL);
    if (retval < 0) return retval;

    retval = rte_eth_dev_start(port);
    if (retval < 0) return retval;

    rte_eth_promiscuous_enable(port);

    printf("Initialized port %u\n", port);
    return 0;
}

int main(int argc, char *argv[]) {
    int ret = rte_eal_init(argc, argv);
    if (ret < 0)
        rte_exit(EXIT_FAILURE, "EAL init failed\n");

    mbuf_pool = rte_pktmbuf_pool_create("MBUF_POOL",
        NUM_MBUFS * 2, MBUF_CACHE_SIZE, 0,
        RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());

    if (mbuf_pool == NULL)
        rte_exit(EXIT_FAILURE, "Cannot create mbuf pool\n");

    if (port_init(0) != 0 || port_init(1) != 0)
        rte_exit(EXIT_FAILURE, "Cannot init ports\n");

    struct rte_mbuf *bufs[BURST_SIZE];

    printf("VXLAN forwarder started\n");

    while (1) {
        uint16_t nb_rx = rte_eth_rx_burst(0, 0, bufs, BURST_SIZE);

        if (DEBUG_RX && nb_rx)
            printf("Received %u packets\n", nb_rx);

        for (int i = 0; i < nb_rx; i++) {
            struct rte_mbuf *m = bufs[i];

            if (vxlan_decap(m) == 0) {
                if (DEBUG_TX)
                    printf("Forwarding decapsulated packet\n");
            } else {
                if (DEBUG_TX)
                    printf("Encapsulating packet\n");

                vxlan_encap(m);
            }

            rte_eth_tx_burst(1, 0, &m, 1);
        }
    }

    return 0;
}