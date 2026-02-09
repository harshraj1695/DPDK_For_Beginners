#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

#include <rte_eal.h>
#include <rte_ether.h>
#include <rte_hash.h>
#include <rte_hash_crc.h>

#define HASH_ENTRIES 1024
#define MAX_PORTS 4

// creating mac table
struct rte_hash *create_mac_table(void) {
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
    rte_exit(EXIT_FAILURE, "MAC table creation failed\n");

  return hash;
}

void print_mac(struct rte_ether_addr *addr) {
  printf("%02X:%02X:%02X:%02X:%02X:%02X", addr->addr_bytes[0],
         addr->addr_bytes[1], addr->addr_bytes[2], addr->addr_bytes[3],
         addr->addr_bytes[4], addr->addr_bytes[5]);
}

// mac learn
void mac_learn(struct rte_hash *table, struct rte_ether_addr *src_mac,
               uint16_t port) {
  rte_hash_add_key_data(table, src_mac, (void *)(uintptr_t)port);

  printf("Learned MAC ");
  print_mac(src_mac);
  printf(" -> Port %u\n", port);
}

// mac lookup
int mac_lookup(struct rte_hash *table, struct rte_ether_addr *dst_mac,
               uint16_t *port) {
  void *lookup;

  if (rte_hash_lookup_data(table, dst_mac, &lookup) >= 0) {
    *port = (uint16_t)(uintptr_t)lookup;
    return 0;
  }

  return -1;
}

// flood packet to all ports except incoming port
void flood_packet(uint16_t in_port) {
  printf("Flooding packet to ports: ");

  for (int p = 0; p < MAX_PORTS; p++) {
    if (p != in_port)
      printf("%d ", p);
  }

  printf("\n");
}

// process a packet: learn source MAC, lookup destination MAC, forward or flood


void process_packet(struct rte_hash *table, struct rte_ether_addr *src,
                    struct rte_ether_addr *dst, uint16_t in_port) {
  printf("\nPacket Received on Port %u\n", in_port);

  printf("SRC MAC: ");
  print_mac(src);
  printf("\n");

  printf("DST MAC: ");
  print_mac(dst);
  printf("\n");

  // Step 1 Learn Source
  mac_learn(table, src, in_port);

  // Step 2 Lookup Destination
  uint16_t out_port;

  if (mac_lookup(table, dst, &out_port) == 0) {
    printf("Forward packet to Port %u\n", out_port);
  } else {
    printf("Destination unknown -> ");
    flood_packet(in_port);
  }
}


int main(int argc, char **argv) {
  if (rte_eal_init(argc, argv) < 0)
    rte_exit(EXIT_FAILURE, "EAL init failed\n");

  struct rte_hash *mac_table = create_mac_table();

  struct rte_ether_addr A = {{0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0x01}};
  struct rte_ether_addr B = {{0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0x02}};
  struct rte_ether_addr C = {{0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0x03}};

  // Simulate Packet Flows
  // A -> B
  process_packet(mac_table, &A, &C, 0);

  // B -> A
  process_packet(mac_table, &C, &A, 2);

  // C -> A
  process_packet(mac_table, &A, &C, 0);

  return 0;
}
