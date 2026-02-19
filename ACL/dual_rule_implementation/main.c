#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stddef.h>
#include <unistd.h>

#include <rte_eal.h>
#include <rte_acl.h>
#include <rte_ip.h>

#define NUM_FIELDS 5
#define MAX_RULES  32
#define NUM_CATEGORIES 1

#define ACTION_ALLOW 777
#define ACTION_DROP  0

RTE_ACL_RULE_DEF(acl_rule, NUM_FIELDS);

struct pkt_key {
    uint8_t  proto;      // must be first
    uint32_t src_ip;
    uint32_t dst_ip;
    uint16_t src_port;
    uint16_t dst_port;
};

enum {
    PROTO_FIELD,
    SRC_FIELD,
    DST_FIELD,
    SPORT_FIELD,
    DPORT_FIELD
};

static struct rte_acl_field_def field_defs[NUM_FIELDS] = {

    {
        .type = RTE_ACL_FIELD_TYPE_BITMASK,
        .size = sizeof(uint8_t),
        .field_index = PROTO_FIELD,
        .input_index = 0,
        .offset = offsetof(struct pkt_key, proto),
    },

    {
        .type = RTE_ACL_FIELD_TYPE_MASK,
        .size = sizeof(uint32_t),
        .field_index = SRC_FIELD,
        .input_index = 1,
        .offset = offsetof(struct pkt_key, src_ip),
    },

    {
        .type = RTE_ACL_FIELD_TYPE_MASK,
        .size = sizeof(uint32_t),
        .field_index = DST_FIELD,
        .input_index = 2,
        .offset = offsetof(struct pkt_key, dst_ip),
    },

    {
        .type = RTE_ACL_FIELD_TYPE_RANGE,
        .size = sizeof(uint16_t),
        .field_index = SPORT_FIELD,
        .input_index = 3,
        .offset = offsetof(struct pkt_key, src_port),
    },

    {
        .type = RTE_ACL_FIELD_TYPE_RANGE,
        .size = sizeof(uint16_t),
        .field_index = DPORT_FIELD,
        .input_index = 4,
        .offset = offsetof(struct pkt_key, dst_port),
    }
};

int main(int argc, char **argv)
{
    if (rte_eal_init(argc, argv) < 0) {
        printf("EAL init failed\n");
        return -1;
    }

    struct rte_acl_param prm = {
        .name = "acl_ctx",
        .socket_id = rte_socket_id(),
        .rule_size = RTE_ACL_RULE_SZ(NUM_FIELDS),
        .max_rule_num = MAX_RULES
    };

    struct rte_acl_ctx *ctx = rte_acl_create(&prm);
    if (!ctx) {
        printf("ACL create failed\n");
        return -1;
    }

    struct acl_rule rules[2];
    memset(rules, 0, sizeof(rules));

    // Rule 0: allow specific TCP flow
    rules[0].data.priority = 100;
    rules[0].data.category_mask = 1;
    rules[0].data.userdata = ACTION_ALLOW;

    rules[0].field[PROTO_FIELD].value.u8 = IPPROTO_TCP;
    rules[0].field[PROTO_FIELD].mask_range.u8 = 0xFF;

    rules[0].field[SRC_FIELD].value.u32 = RTE_IPV4(140,82,112,21);
    rules[0].field[SRC_FIELD].mask_range.u32 = 32;

    rules[0].field[DST_FIELD].value.u32 = RTE_IPV4(172,17,166,200);
    rules[0].field[DST_FIELD].mask_range.u32 = 32;

    rules[0].field[SPORT_FIELD].value.u16 = 443;
    rules[0].field[SPORT_FIELD].mask_range.u16 = 443;

    rules[0].field[DPORT_FIELD].value.u16 = 42960;
    rules[0].field[DPORT_FIELD].mask_range.u16 = 42960;

    // Rule 1: default DROP (wildcard rule)
    rules[1].data.priority = 1;
    rules[1].data.category_mask = 1;
    rules[1].data.userdata = ACTION_DROP;

    for (int i = 0; i < NUM_FIELDS; i++) {
        rules[1].field[i].value.u64 = 0;
        rules[1].field[i].mask_range.u64 = 0;
    }

    if (rte_acl_add_rules(ctx, (struct rte_acl_rule *)rules, 2) < 0) {
        printf("Add rules failed\n");
        return -1;
    }

    struct rte_acl_config cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.num_categories = NUM_CATEGORIES;
    cfg.num_fields = NUM_FIELDS;
    memcpy(cfg.defs, field_defs, sizeof(field_defs));

    if (rte_acl_build(ctx, &cfg) != 0) {
        printf("ACL build failed\n");
        return -1;
    }

    printf("ACL built successfully\n");

    struct pkt_key pkt;

    pkt.proto    = IPPROTO_TCP;
    pkt.src_ip   = rte_cpu_to_be_32(RTE_IPV4(140,82,112,21));
    pkt.dst_ip   = rte_cpu_to_be_32(RTE_IPV4(172,17,166,200));
    pkt.src_port = rte_cpu_to_be_16(443);
    pkt.dst_port = rte_cpu_to_be_16(42960);

    const uint8_t *data[1] = {(const uint8_t *)&pkt};
    uint32_t result[1];

    while (1) {

        rte_acl_classify(ctx, data, result, 1, NUM_CATEGORIES);

        printf("ACL result userdata = %u\n", result[0]);

        if (result[0] == ACTION_ALLOW)
            printf("MATCHED: packet allowed\n");
        else
            printf("DEFAULT DROP applied\n");

        sleep(1);
    }

    return 0;
}
