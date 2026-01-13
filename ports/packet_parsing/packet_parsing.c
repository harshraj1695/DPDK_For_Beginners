#include <stdio.h>
#include <string.h>

#include <rte_ether.h>
#include <rte_icmp.h>
#include <rte_ip.h>
#include <rte_mbuf.h>
#include <rte_tcp.h>
#include <rte_udp.h>

void print_ipv6_addr(uint8_t a[16]) {
  for (int i = 0; i < 16; i += 2) {
    printf("%02x%02x", a[i], a[i + 1]);

    if (i < 14)
      printf(":");
  }
}

void parse_packet(struct rte_mbuf *m) {
  uint8_t *pkt = rte_pktmbuf_mtod(m, uint8_t *);
  uint16_t offset = 0;

  printf("\n========== PACKET PARSING ==========\n");

  /* -------------------------------------------------- */
  /* L2: ETHERNET HEADER                                */
  /* -------------------------------------------------- */
  struct rte_ether_hdr *eth = (struct rte_ether_hdr *)pkt;

  printf("L2: Ethernet\n");
  printf("   SRC MAC: %02x:%02x:%02x:%02x:%02x:%02x\n",
         eth->src_addr.addr_bytes[0], eth->src_addr.addr_bytes[1],
         eth->src_addr.addr_bytes[2], eth->src_addr.addr_bytes[3],
         eth->src_addr.addr_bytes[4], eth->src_addr.addr_bytes[5]);

  printf("   DST MAC: %02x:%02x:%02x:%02x:%02x:%02x\n",
         eth->dst_addr.addr_bytes[0], eth->dst_addr.addr_bytes[1],
         eth->dst_addr.addr_bytes[2], eth->dst_addr.addr_bytes[3],
         eth->dst_addr.addr_bytes[4], eth->dst_addr.addr_bytes[5]);

  uint16_t eth_type = rte_be_to_cpu_16(eth->ether_type);
  offset = sizeof(struct rte_ether_hdr);

  /* -------------------------------------------------- */
  /* VLAN Detection                                     */
  /* -------------------------------------------------- */
  if (eth_type == RTE_ETHER_TYPE_VLAN) {
    struct rte_vlan_hdr *vlan =
        rte_pktmbuf_mtod_offset(m, struct rte_vlan_hdr *, offset);

    printf("L2: VLAN Tag - TCI=%u\n", rte_be_to_cpu_16(vlan->vlan_tci));

    eth_type = rte_be_to_cpu_16(vlan->eth_proto);
    offset += sizeof(struct rte_vlan_hdr);
  }

  /* -------------------------------------------------- */
  /* L3: IPv4                                           */
  /* -------------------------------------------------- */
  if (eth_type == RTE_ETHER_TYPE_IPV4) {

    struct rte_ipv4_hdr *ip =
        rte_pktmbuf_mtod_offset(m, struct rte_ipv4_hdr *, offset);

    printf("L3: IPv4\n");

    printf("   SRC IP: %d.%d.%d.%d\n", (ip->src_addr) & 0xFF,
           (ip->src_addr >> 8) & 0xFF, (ip->src_addr >> 16) & 0xFF,
           (ip->src_addr >> 24) & 0xFF);

    printf("   DST IP: %d.%d.%d.%d\n", (ip->dst_addr) & 0xFF,
           (ip->dst_addr >> 8) & 0xFF, (ip->dst_addr >> 16) & 0xFF,
           (ip->dst_addr >> 24) & 0xFF);

    uint8_t proto = ip->next_proto_id;
    uint16_t ihl = (ip->version_ihl & 0x0F) * 4;
    offset += ihl;

    /* ---------------- UDP ---------------- */
    if (proto == IPPROTO_UDP) {
      struct rte_udp_hdr *udp =
          rte_pktmbuf_mtod_offset(m, struct rte_udp_hdr *, offset);

      printf("L4: UDP\n");
      printf("   SRC PORT: %u\n", rte_be_to_cpu_16(udp->src_port));
      printf("   DST PORT: %u\n", rte_be_to_cpu_16(udp->dst_port));

      offset += sizeof(struct rte_udp_hdr);
    }

    /* ---------------- TCP ---------------- */
    else if (proto == IPPROTO_TCP) {

      struct rte_tcp_hdr *tcp =
          rte_pktmbuf_mtod_offset(m, struct rte_tcp_hdr *, offset);

      printf("L4: TCP\n");
      printf("   SRC PORT: %u\n", rte_be_to_cpu_16(tcp->src_port));
      printf("   DST PORT: %u\n", rte_be_to_cpu_16(tcp->dst_port));
      printf("   SEQ NUM:  %u\n", rte_be_to_cpu_32(tcp->sent_seq));
      printf("   ACK NUM:  %u\n", rte_be_to_cpu_32(tcp->recv_ack));
      printf("   FLAGS:    0x%x\n", tcp->tcp_flags);

      uint16_t tcp_hlen = ((tcp->data_off >> 4) & 0xF) * 4;
      offset += tcp_hlen;
    }

    /* ---------------- ICMP ---------------- */
    else if (proto == IPPROTO_ICMP) {

      struct rte_icmp_hdr *icmp =
          rte_pktmbuf_mtod_offset(m, struct rte_icmp_hdr *, offset);

      printf("L4: ICMP\n");
      printf("   TYPE: %u\n", icmp->icmp_type);
      printf("   CODE: %u\n", icmp->icmp_code);

      offset += sizeof(struct rte_icmp_hdr);
    }
  }

  /* -------------------------------------------------- */
  /* L3: IPv6                                           */
  /* -------------------------------------------------- */
  else if (eth_type == RTE_ETHER_TYPE_IPV6) {

    struct rte_ipv6_hdr *ip6 =
        rte_pktmbuf_mtod_offset(m, struct rte_ipv6_hdr *, offset);

    printf("L3: IPv6\n");

    printf("SRC IPv6: ");
    print_ipv6_addr(ip6->src_addr.a);

    printf("\nDST IPv6: ");
    print_ipv6_addr(ip6->dst_addr.a);
    printf("\n");

    uint8_t proto = ip6->proto;
    offset += sizeof(struct rte_ipv6_hdr);

    if (proto == IPPROTO_UDP) {
      struct rte_udp_hdr *udp =
          rte_pktmbuf_mtod_offset(m, struct rte_udp_hdr *, offset);

      printf("L4: UDP (IPv6)\n");
      printf("   SRC PORT: %u\n", rte_be_to_cpu_16(udp->src_port));
      printf("   DST PORT: %u\n", rte_be_to_cpu_16(udp->dst_port));

      offset += sizeof(struct rte_udp_hdr);
    } else if (proto == IPPROTO_TCP) {
      struct rte_tcp_hdr *tcp =
          rte_pktmbuf_mtod_offset(m, struct rte_tcp_hdr *, offset);

      printf("L4: TCP (IPv6)\n");
      printf("   SRC PORT: %u\n", rte_be_to_cpu_16(tcp->src_port));
      printf("   DST PORT: %u\n", rte_be_to_cpu_16(tcp->dst_port));

      offset += sizeof(struct rte_tcp_hdr);
    }
  }
}