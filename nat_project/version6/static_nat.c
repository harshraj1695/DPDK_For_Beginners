#include "source.h"

#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>

#include <rte_byteorder.h>
#include <rte_cycles.h>
#include <rte_ether.h>
#include <rte_hash.h>
#include <rte_ip.h>
#include <rte_jhash.h>
#include <rte_lcore.h>
#include <rte_malloc.h>
#include <rte_errno.h>
#include <rte_tcp.h>
#include <rte_udp.h>

// forward table: private -> public
static struct rte_hash *fwd_hash;

// reverse table: public -> private
static struct rte_hash *rev_hash;

static uint32_t PUBLIC_IP;
static uint16_t current_port = MIN_PORT;

// convert IP to string
static inline const char *ip4_to_str(uint32_t ip_be, char *buf, size_t len) {
  struct in_addr a;
  a.s_addr = ip_be;
  return inet_ntop(AF_INET, &a, buf, len);
}

static inline uint16_t port_to_host(uint16_t port_be) {
  return rte_be_to_cpu_16(port_be);
}

// allocate unique public port
static uint16_t allocate_port(void) {
  current_port++;

  if (current_port >= MAX_PORT)
    current_port = MIN_PORT;

  return rte_cpu_to_be_16(current_port);
}

// delete entry on timeout
static void nat_entry_expire(struct rte_timer *tim, void *arg) {
  struct nat_entry *e = (struct nat_entry *)arg;

  // remove from both tables
  rte_hash_del_key(fwd_hash, &e->fwd_key);
  rte_hash_del_key(rev_hash, &e->rev_key);

  char priv_ip[INET_ADDRSTRLEN], pub_ip[INET_ADDRSTRLEN], rem_ip[INET_ADDRSTRLEN];
  ip4_to_str(e->private_ip, priv_ip, sizeof(priv_ip));
  ip4_to_str(e->public_ip, pub_ip, sizeof(pub_ip));
  ip4_to_str(e->remote_ip, rem_ip, sizeof(rem_ip));
  printf("[nat] expired: %s:%u -> %s:%u (remote %s:%u proto=%u)\n",
         priv_ip, port_to_host(e->private_port),
         pub_ip, port_to_host(e->public_port),
         rem_ip, port_to_host(e->remote_port),
         e->proto);

  rte_free(e);
  (void)tim;
}

// create new NAT mapping
static struct nat_entry *create_entry(uint32_t sip, uint16_t sport,
                                      uint32_t dip, uint16_t dport,
                                      uint8_t proto) {
  struct nat_entry *e = rte_malloc(NULL, sizeof(*e), 0);
  if (!e)
    return NULL;

  // store private side
  e->private_ip = sip;
  e->private_port = sport;

  // store remote server
  e->remote_ip = dip;
  e->remote_port = dport;

  // assign public mapping
  e->public_ip = PUBLIC_IP;
  e->public_port = allocate_port();

  e->proto = proto;

  // forward key
  e->fwd_key.src_ip = sip;
  e->fwd_key.dst_ip = dip;
  e->fwd_key.src_port = sport;
  e->fwd_key.dst_port = dport;
  e->fwd_key.proto = proto;

  // reverse key
  e->rev_key.src_ip = dip;
  e->rev_key.dst_ip = e->public_ip;
  e->rev_key.src_port = dport;
  e->rev_key.dst_port = e->public_port;
  e->rev_key.proto = proto;

  // init timer
  rte_timer_init(&e->timer);

  uint64_t timeout = NAT_TIMEOUT_SEC * rte_get_timer_hz();

  rte_timer_reset(&e->timer, timeout, SINGLE, rte_lcore_id(), nat_entry_expire,
                  e);

  // insert into both tables
  rte_hash_add_key_data(fwd_hash, &e->fwd_key, e);
  rte_hash_add_key_data(rev_hash, &e->rev_key, e);

  char priv_ip[INET_ADDRSTRLEN], pub_ip[INET_ADDRSTRLEN], rem_ip[INET_ADDRSTRLEN];
  ip4_to_str(e->private_ip, priv_ip, sizeof(priv_ip));
  ip4_to_str(e->public_ip, pub_ip, sizeof(pub_ip));
  ip4_to_str(e->remote_ip, rem_ip, sizeof(rem_ip));
  printf("[nat] new: %s:%u -> %s:%u (remote %s:%u proto=%u)\n",
         priv_ip, port_to_host(e->private_port),
         pub_ip, port_to_host(e->public_port),
         rem_ip, port_to_host(e->remote_port),
         e->proto);

  return e;
}

// refresh timer
static inline void refresh(struct nat_entry *e) {
  uint64_t timeout = NAT_TIMEOUT_SEC * rte_get_timer_hz();

  rte_timer_stop(&e->timer);

  rte_timer_reset(&e->timer, timeout, SINGLE, rte_lcore_id(), nat_entry_expire,
                  e);
}

// parse packet
static int parse(struct rte_mbuf *m, uint32_t *sip, uint32_t *dip,
                 uint16_t *sport, uint16_t *dport, uint8_t *proto) {
  struct rte_ether_hdr *eth = rte_pktmbuf_mtod(m, struct rte_ether_hdr *);

  if (eth->ether_type != rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4))
    return -1;

  struct rte_ipv4_hdr *ip = (struct rte_ipv4_hdr *)(eth + 1);

  *sip = ip->src_addr;
  *dip = ip->dst_addr;
  *proto = ip->next_proto_id;

  if (*proto == IPPROTO_TCP) {
    struct rte_tcp_hdr *tcp = (struct rte_tcp_hdr *)(ip + 1);
    *sport = tcp->src_port;
    *dport = tcp->dst_port;
  } else if (*proto == IPPROTO_UDP) {
    struct rte_udp_hdr *udp = (struct rte_udp_hdr *)(ip + 1);
    *sport = udp->src_port;
    *dport = udp->dst_port;
  } else
    return -1;

  return 0;
}

// rewrite outgoing packet (SNAT)
static void rewrite_out(struct rte_mbuf *m, struct nat_entry *e) {
  struct rte_ipv4_hdr *ip =
      (struct rte_ipv4_hdr *)(rte_pktmbuf_mtod(m, char *) +
                              sizeof(struct rte_ether_hdr));

  ip->src_addr = e->public_ip;

  if (e->proto == IPPROTO_TCP) {
    struct rte_tcp_hdr *tcp = (struct rte_tcp_hdr *)(ip + 1);
    tcp->src_port = e->public_port;
    tcp->cksum = rte_ipv4_udptcp_cksum(ip, tcp);
  } else {
    struct rte_udp_hdr *udp = (struct rte_udp_hdr *)(ip + 1);
    udp->src_port = e->public_port;
    udp->dgram_cksum = rte_ipv4_udptcp_cksum(ip, udp);
  }

  ip->hdr_checksum = rte_ipv4_cksum(ip);
}

// rewrite incoming packet (DNAT)
static void rewrite_in(struct rte_mbuf *m, struct nat_entry *e) {
  struct rte_ipv4_hdr *ip =
      (struct rte_ipv4_hdr *)(rte_pktmbuf_mtod(m, char *) +
                              sizeof(struct rte_ether_hdr));

  ip->dst_addr = e->private_ip;

  if (e->proto == IPPROTO_TCP) {
    struct rte_tcp_hdr *tcp = (struct rte_tcp_hdr *)(ip + 1);
    tcp->dst_port = e->private_port;
    tcp->cksum = rte_ipv4_udptcp_cksum(ip, tcp);
  } else {
    struct rte_udp_hdr *udp = (struct rte_udp_hdr *)(ip + 1);
    udp->dst_port = e->private_port;
    udp->dgram_cksum = rte_ipv4_udptcp_cksum(ip, udp);
  }

  ip->hdr_checksum = rte_ipv4_cksum(ip);
}

// main packet handler
int nat_process_packet(struct rte_mbuf *m) {
  uint32_t sip, dip;
  uint16_t sport, dport;
  uint8_t proto;

  if (parse(m, &sip, &dip, &sport, &dport, &proto) < 0)
    return -1;

  struct nat_entry *e = NULL;

  // detect incoming traffic (dest is public IP)
  if (dip == PUBLIC_IP) {
  printf("Inbound traffic\n");
    struct nat_key rev = {sip, dip, sport, dport, proto};

    if (rte_hash_lookup_data(rev_hash, &rev, (void **)&e) < 0)
      return -1;

    refresh(e);
    rewrite_in(m, e);
    return 0;
  }

  // outgoing traffic
  printf("Outbound traffic\n");
  struct nat_key fwd = {sip, dip, sport, dport, proto};
  
  if (rte_hash_lookup_data(fwd_hash, &fwd, (void **)&e) < 0) {
    e = create_entry(sip, sport, dip, dport, proto);
    if (!e)
      return -1;
  } else {
    refresh(e);
  }

  rewrite_out(m, e);
  return 0;
}

// init NAT
int nat_init(uint32_t public_ip) {
  PUBLIC_IP = public_ip;

  char pub_ip_str[INET_ADDRSTRLEN];
  ip4_to_str(PUBLIC_IP, pub_ip_str, sizeof(pub_ip_str));
  printf("[nat] init: public_ip=%s entries=%u key_len=%zu socket=%d timeout=%us\n",
         pub_ip_str, 65536u, sizeof(struct nat_key), rte_socket_id(),
         (unsigned)NAT_TIMEOUT_SEC);

  struct rte_hash_parameters params = {
      .entries = 65536,
      .key_len = sizeof(struct nat_key),
      .hash_func = rte_jhash,
      .socket_id = rte_socket_id(),
  };

  struct rte_hash_parameters fwd_params = params;
  fwd_params.name = "nat_fwd_table";
  fwd_hash = rte_hash_create(&fwd_params);
  if (!fwd_hash) {
    printf("Failed to create %s (rte_errno=%d: %s)\n", fwd_params.name,
           rte_errno, rte_strerror(rte_errno));
    return -1;
  }

  struct rte_hash_parameters rev_params = params;
  rev_params.name = "nat_rev_table";
  rev_hash = rte_hash_create(&rev_params);
  if (!rev_hash) {
    printf("Failed to create %s (rte_errno=%d: %s)\n", rev_params.name,
           rte_errno, rte_strerror(rte_errno));
    return -1;
  }

  printf("[nat] tables created: fwd=%s rev=%s\n", fwd_params.name, rev_params.name);
  return 0;
}

// timer manage
void nat_timer_manage(void) { rte_timer_manage(); }
