#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>

#include <rte_eal.h>
#include <rte_hash.h>
#include <rte_hash_crc.h>
#include <rte_ether.h>

#define HASH_ENTRIES 1024



// Create MAC table (hash)
struct rte_hash *create_mac_table(void)
{
    struct rte_hash_parameters params = {
        .name = "mac_table",
        .entries = HASH_ENTRIES,
        .key_len = sizeof(struct rte_ether_addr),
        .hash_func = rte_hash_crc,
        .hash_func_init_val = 0,
        .socket_id = rte_socket_id(),
    };

    struct rte_hash *hash = rte_hash_create(&params);

    if (!hash)
        rte_exit(EXIT_FAILURE, "Failed to create MAC table\n");

    return hash;
}

// MAC Learning
void mac_learn(struct rte_hash *mac_hash,
               struct rte_ether_addr *src_mac,
               uint16_t port)
{
    rte_hash_add_key_data(mac_hash, src_mac,
                          (void *)(uintptr_t)port);
}


// MAC Lookup
int mac_lookup(struct rte_hash *mac_hash,
               struct rte_ether_addr *dst_mac,
               uint16_t *port)
{
    void *lookup;

    int ret = rte_hash_lookup_data(mac_hash, dst_mac, &lookup);

    if (ret >= 0) {
        *port = (uint16_t)(uintptr_t)lookup;
        return 0;
    }

    return -1;
}

// Utility to print MAC address
void print_mac(struct rte_ether_addr *addr)
{
    printf("%02X:%02X:%02X:%02X:%02X:%02X",
           addr->addr_bytes[0],
           addr->addr_bytes[1],
           addr->addr_bytes[2],
           addr->addr_bytes[3],
           addr->addr_bytes[4],
           addr->addr_bytes[5]);
}



int main(int argc, char **argv)
{
    if (rte_eal_init(argc, argv) < 0)
        rte_exit(EXIT_FAILURE, "EAL init failed\n");

    struct rte_hash *mac_table = create_mac_table();

    // Sample MAC addresses and ports

    struct rte_ether_addr mac1 = {{0xAA,0xBB,0xCC,0xDD,0xEE,0x01}};
    struct rte_ether_addr mac2 = {{0xAA,0xBB,0xCC,0xDD,0xEE,0x02}};
    struct rte_ether_addr mac3 = {{0xAA,0xBB,0xCC,0xDD,0xEE,0x03}};

    // Learn Source MACs

    mac_learn(mac_table, &mac1, 1);
    mac_learn(mac_table, &mac2, 2);

    printf("Learned MACs\n");

    // Lookup Destination MAC

    uint16_t out_port;

    printf("\nLooking up MAC: ");
    print_mac(&mac1);

    if (mac_lookup(mac_table, &mac1, &out_port) == 0)
        printf(" -> Forward to port %u\n", out_port);
    else
        printf(" -> Flood\n");

    printf("\nLooking up MAC: ");
    print_mac(&mac3);

    if (mac_lookup(mac_table, &mac3, &out_port) == 0)
        printf(" -> Forward to port %u\n", out_port);
    else
        printf(" -> Flood (unknown MAC)\n");

    return 0;
}
