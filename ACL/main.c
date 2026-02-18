#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stddef.h>

#include <rte_eal.h>
#include <rte_acl.h>
#include <rte_ip.h>

#define NUM_FIELDS 5
#define MAX_RULES  32
#define NUM_CATEGORIES 1

/* Rule structure */
RTE_ACL_RULE_DEF(acl_rule, NUM_FIELDS);

struct pkt_key {
    uint32_t src_ip;
    uint32_t dst_ip;
    uint16_t src_port;
    uint16_t dst_port;
    uint8_t  proto;
};

enum {
    SRC_FIELD,
    DST_FIELD,
    SPORT_FIELD,
    DPORT_FIELD,
    PROTO_FIELD
};

static struct rte_acl_field_def field_defs[NUM_FIELDS] = {

    {  // SRC IP
        .type = RTE_ACL_FIELD_TYPE_MASK,
        .size = sizeof(uint32_t),
        .field_index = SRC_FIELD,
        .input_index = 0,
        .offset = offsetof(struct pkt_key, src_ip),
    },

    {   // DST IP
        .type = RTE_ACL_FIELD_TYPE_MASK,
        .size = sizeof(uint32_t),
        .field_index = DST_FIELD,
        .input_index = 0,
        .offset = offsetof(struct pkt_key, dst_ip),
    },

    {   // SRC PORT 
        .type = RTE_ACL_FIELD_TYPE_RANGE,
        .size = sizeof(uint16_t),
        .field_index = SPORT_FIELD,
        .input_index = 1,
        .offset = offsetof(struct pkt_key, src_port),
    },

    {   // DST PORT 
        .type = RTE_ACL_FIELD_TYPE_RANGE,
        .size = sizeof(uint16_t),
        .field_index = DPORT_FIELD,
        .input_index = 1,
        .offset = offsetof(struct pkt_key, dst_port),
    },

    {   // PROTOCOL
        .type = RTE_ACL_FIELD_TYPE_BITMASK,
        .size = sizeof(uint8_t),
        .field_index = PROTO_FIELD,
        .input_index = 2,
        .offset = offsetof(struct pkt_key, proto),
    }
};

int main(int argc, char **argv)
{
    if (rte_eal_init(argc, argv) < 0) {
        printf("EAL init failed\n");
        return -1;
    }

    // Create ACL context 
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
// Add a rule to ACL
    struct acl_rule rule;
    memset(&rule, 0, sizeof(rule));

    rule.data.priority = 100;
    rule.data.category_mask = 1;
    rule.data.userdata = 777;   // result if matched

    // SRC IP = 140.82.112.21 /
    rule.field[SRC_FIELD].value.u32 =
        rte_cpu_to_be_32(RTE_IPV4(140,82,112,21));
    rule.field[SRC_FIELD].mask_range.u32 = 32;

    // DST IP = 172.17.166.200 
    rule.field[DST_FIELD].value.u32 =
        rte_cpu_to_be_32(RTE_IPV4(172,17,166,200));
    rule.field[DST_FIELD].mask_range.u32 = 32;

    // SRC PORT = 443 
    rule.field[SPORT_FIELD].value.u16 =
        rte_cpu_to_be_16(443);
    rule.field[SPORT_FIELD].mask_range.u16 = 443;

    // DST PORT = 42960 
    rule.field[DPORT_FIELD].value.u16 =
        rte_cpu_to_be_16(42960);
    rule.field[DPORT_FIELD].mask_range.u16 = 42960;

    // PROTO TCP 
    rule.field[PROTO_FIELD].value.u8 = IPPROTO_TCP;
    rule.field[PROTO_FIELD].mask_range.u8 = 0xFF;

    if (rte_acl_add_rules(ctx, (struct rte_acl_rule *)&rule, 1) < 0) {
        printf("Add rules failed\n");
        return -1;
    }

    //  BUILD ACL 
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

    pkt.src_ip   = rte_cpu_to_be_32(RTE_IPV4(140,82,112,21));
    pkt.dst_ip   = rte_cpu_to_be_32(RTE_IPV4(172,17,166,200));
    pkt.src_port = rte_cpu_to_be_16(443);
    pkt.dst_port = rte_cpu_to_be_16(42960);
    pkt.proto    = IPPROTO_TCP;

    const uint8_t *data[1] = {(const uint8_t *)&pkt};
    uint32_t result[1];
while(1){
    rte_acl_classify(ctx, data, result, 1, NUM_CATEGORIES);

    printf("ACL result userdata = %u\n", result[0]);

    if (result[0] == 777)
        printf("MATCHED: packet allowed\n");
    else
        printf("NOT MATCHED: packet blocked\n");
}
    return 0;
}
