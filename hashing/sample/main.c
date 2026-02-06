#include <stdio.h>
#include <stdint.h>
#include <rte_eal.h>
#include <rte_hash.h>
#include <rte_hash_crc.h>


#define HASH_ENTRIES 1024


int main(int argc, char **argv)
{
    if (rte_eal_init(argc, argv) < 0)
        rte_exit(EXIT_FAILURE, "EAL init failed\n");

    struct rte_hash_parameters params = {
        .name = "custom_hash",
        .entries = HASH_ENTRIES,
        .key_len = sizeof(int),
        .hash_func = rte_hash_crc,
        .hash_func_init_val = 12345,
        .socket_id = rte_socket_id(),
    };

    struct rte_hash *hash = rte_hash_create(&params);

    if (!hash)
        rte_exit(EXIT_FAILURE, "Hash create failed\n");

    int key = 50;
    int value = 500;

    rte_hash_add_key_data(hash, &key, (void *)(uintptr_t)value);

    void *lookup;

    if (rte_hash_lookup_data(hash, &key, &lookup) >= 0)
        printf("Found value = %d\n", (int)(uintptr_t)lookup);
    else
        printf("Lookup failed\n");

    return 0;
}
