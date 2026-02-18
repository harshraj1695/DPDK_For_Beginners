#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <rte_acl.h>
#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_ether.h>
#include <rte_ip.h>
#include <rte_mbuf.h>
#include <rte_tcp.h>
#include <rte_udp.h>

#define NUM_FIELDS 5
#define MAX_RULES 32
#define NUM_CATEGORIES 1
#define NB_MBUF 8192
#define BURST 32

/* ACL rule structure */
RTE_ACL_RULE_DEF(acl_rule, NUM_FIELDS);

/* Packet key used by ACL */
struct pkt_key {
  uint32_t src_ip;
  uint32_t dst_ip;
  uint16_t src_port;
  uint16_t dst_port;
  uint8_t proto;
};

/* Field indexes */
enum { SRC_FIELD, DST_FIELD, SPORT_FIELD, DPORT_FIELD, PROTO_FIELD };

/* ACL field definitions */
static struct rte_acl_field_def field_defs[NUM_FIELDS] = {

    {
        /* SRC IP */
        .type = RTE_ACL_FIELD_TYPE_MASK,
        .size = sizeof(uint32_t),
        .field_index = SRC_FIELD,
        .input_index = 0,
        .offset = offsetof(struct pkt_key, src_ip),
    },

    {
        /* DST IP */
        .type = RTE_ACL_FIELD_TYPE_MASK,
        .size = sizeof(uint32_t),
        .field_index = DST_FIELD,
        .input_index = 0,
        .offset = offsetof(struct pkt_key, dst_ip),
    },

    {
        /* SRC PORT */
        .type = RTE_ACL_FIELD_TYPE_RANGE,
        .size = sizeof(uint16_t),
        .field_index = SPORT_FIELD,
        .input_index = 1,
        .offset = offsetof(struct pkt_key, src_port),
    },

    {
        /* DST PORT */
        .type = RTE_ACL_FIELD_TYPE_RANGE,
        .size = sizeof(uint16_t),
        .field_index = DPORT_FIELD,
        .input_index = 1,
        .offset = offsetof(struct pkt_key, dst_port),
    },

    {
        /* PROTO */
        .type = RTE_ACL_FIELD_TYPE_BITMASK,
        .size = sizeof(uint8_t),
        .field_index = PROTO_FIELD,
        .input_index = 2,
        .offset = offsetof(struct pkt_key, proto),
    }};

int main(int argc, char **argv) {
  /* ----------- EAL INIT ----------- */
  if (rte_eal_init(argc, argv) < 0)
    rte_exit(EXIT_FAILURE, "EAL init failed\n");

  uint16_t nb_ports = rte_eth_dev_count_avail();
  printf("Available ports: %u\n", nb_ports);

  if (nb_ports == 0)
    rte_exit(EXIT_FAILURE, "No Ethernet ports available\n");

  uint16_t port_id = 0;

  struct rte_eth_dev_info info;
  rte_eth_dev_info_get(port_id, &info);
  printf("Using port %u driver=%s\n", port_id, info.driver_name);

  /* ----------- MEMPOOL ----------- */
  struct rte_mempool *mp = rte_pktmbuf_pool_create(
      "mbuf_pool", NB_MBUF, 256, 0, RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());

  if (!mp)
    rte_exit(EXIT_FAILURE, "mempool failed\n");

  /* ----------- PORT CONFIG ----------- */
  struct rte_eth_conf port_conf;
  memset(&port_conf, 0, sizeof(port_conf));

  port_conf.rxmode.mq_mode = RTE_ETH_MQ_RX_NONE;
  port_conf.txmode.mq_mode = RTE_ETH_MQ_TX_NONE;

  if (rte_eth_dev_configure(port_id, 1, 1, &port_conf) < 0)
    rte_exit(EXIT_FAILURE, "port configure failed\n");

  if (rte_eth_rx_queue_setup(port_id, 0, 1024, rte_eth_dev_socket_id(port_id),
                             NULL, mp) < 0)
    rte_exit(EXIT_FAILURE, "rx setup failed\n");

  if (rte_eth_tx_queue_setup(port_id, 0, 1024, rte_eth_dev_socket_id(port_id),
                             NULL) < 0)
    rte_exit(EXIT_FAILURE, "tx setup failed\n");

  if (rte_eth_dev_start(port_id) < 0)
    rte_exit(EXIT_FAILURE, "port start failed\n");

  rte_eth_promiscuous_enable(port_id);

  printf("Port started successfully\n");

  /* ----------- ACL SETUP ----------- */
  struct rte_acl_param prm = {.name = "acl_ctx",
                              .socket_id = rte_socket_id(),
                              .rule_size = RTE_ACL_RULE_SZ(NUM_FIELDS),
                              .max_rule_num = MAX_RULES};

  struct rte_acl_ctx *ctx = rte_acl_create(&prm);
  if (!ctx)
    rte_exit(EXIT_FAILURE, "ACL create failed\n");

  /* RULE: ICMP to 10.0.0.2 */
  struct acl_rule rule;
  memset(&rule, 0, sizeof(rule));

  rule.data.priority = 100;
  rule.data.category_mask = 1;
  rule.data.userdata = 42;

  /* SRC any */
  rule.field[SRC_FIELD].value.u32 = 0;
  rule.field[SRC_FIELD].mask_range.u32 = 0;

  /* DST = 239.255.255.250 */
  rule.field[DST_FIELD].value.u32 = RTE_IPV4(239, 255, 255, 250);
  rule.field[DST_FIELD].mask_range.u32 = 32;

  /* SRC PORT any */
  rule.field[SPORT_FIELD].value.u16 = 0;
  rule.field[SPORT_FIELD].mask_range.u16 = 65535;

  /* DST PORT = 1900 */
  rule.field[DPORT_FIELD].value.u16 = 1900;
  rule.field[DPORT_FIELD].mask_range.u16 = 1900;

  /* PROTO = UDP */
  rule.field[PROTO_FIELD].value.u8 = IPPROTO_UDP;
  rule.field[PROTO_FIELD].mask_range.u8 = 0xFF;

  if (rte_acl_add_rules(ctx, (struct rte_acl_rule *)&rule, 1) < 0)
    rte_exit(EXIT_FAILURE, "Rule add failed\n");

  struct rte_acl_config cfg;
  memset(&cfg, 0, sizeof(cfg));
  cfg.num_categories = 1;
  cfg.num_fields = NUM_FIELDS;
  memcpy(cfg.defs, field_defs, sizeof(field_defs));

  if (rte_acl_build(ctx, &cfg) != 0)
    rte_exit(EXIT_FAILURE, "ACL build failed\n");

  printf("ACL ready\n");

  /* ----------- RX LOOP ----------- */
  struct rte_mbuf *bufs[BURST];

  while (1) {

    uint16_t nb = rte_eth_rx_burst(port_id, 0, bufs, BURST);
    if (!nb)
      continue;

    for (int i = 0; i < nb; i++) {

      struct rte_mbuf *m = bufs[i];

      struct rte_ether_hdr *eth = rte_pktmbuf_mtod(m, struct rte_ether_hdr *);

      if (eth->ether_type != rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4)) {
        rte_pktmbuf_free(m);
        continue;
      }

      struct rte_ipv4_hdr *ip = (struct rte_ipv4_hdr *)(eth + 1);

      struct pkt_key key;
      memset(&key, 0, sizeof(key));

      key.src_ip = ip->src_addr;
      key.dst_ip = ip->dst_addr;
      key.proto = ip->next_proto_id;

      uint8_t *l4 = (uint8_t *)ip + (ip->ihl * 4);

      if (key.proto == IPPROTO_TCP) {
        struct rte_tcp_hdr *tcp = (struct rte_tcp_hdr *)l4;
        key.src_port = tcp->src_port;
        key.dst_port = tcp->dst_port;
      } else if (key.proto == IPPROTO_UDP) {
        struct rte_udp_hdr *udp = (struct rte_udp_hdr *)l4;
        key.src_port = udp->src_port;
        key.dst_port = udp->dst_port;
      }

      uint32_t s = rte_be_to_cpu_32(key.src_ip);
      uint32_t d = rte_be_to_cpu_32(key.dst_ip);

      printf("RX proto=%u src=%u.%u.%u.%u dst=%u.%u.%u.%u\n", key.proto,
             (s >> 24) & 255, (s >> 16) & 255, (s >> 8) & 255, s & 255,
             (d >> 24) & 255, (d >> 16) & 255, (d >> 8) & 255, d & 255);

      const uint8_t *data[1] = {(const uint8_t *)&key};
      uint32_t result[1];

      rte_acl_classify(ctx, data, result, 1, 1);

      if (result[0])
        printf("ACL MATCH\n");
      else
        printf("ACL NO MATCH\n");

      rte_pktmbuf_free(m);
    }
  }
}
