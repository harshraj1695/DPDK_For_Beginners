#include <stdio.h>
#include <string.h>

#include <rte_eal.h>
#include <rte_mempool.h>
#include <rte_mbuf.h>

int main(int argc, char **argv)
{
     // 1. Initialize DPDK EAL
    if (rte_eal_init(argc, argv) < 0)
        rte_exit(EXIT_FAILURE, "EAL init failed\n");

     // 2. Create a mempool to hold mbufs
    struct rte_mempool *mp = rte_pktmbuf_pool_create(
        "MY_POOL",              // pool name
        1024,                   // number of mbufs
        256,                    // per-lcore cache size
        0,                      // private size (0 = none)
        RTE_MBUF_DEFAULT_BUF_SIZE, // data buffer size per mbuf
        rte_socket_id()         // NUMA socket
    );
    printf("size of data room i.e buffer size per mbuf: %d\n", RTE_MBUF_DEFAULT_BUF_SIZE);
    if (!mp)
        rte_exit(EXIT_FAILURE, "Cannot create mempool\n");

    printf("Mempool created successfully!\n");

     // 3. Allocate one mbuf from the mempool
    struct rte_mbuf *m = rte_pktmbuf_alloc(mp);
    if (!m)
        rte_exit(EXIT_FAILURE, "Failed to allocate mbuf\n");

    printf("Mbuf allocated!\n");

     // 4. Add data to mbuf (append)
    const char *msg = "Hello from DPDK mbuf!";
    uint16_t len = strlen(msg) + 1;

    char *data = rte_pktmbuf_append(m, len);
    if (!data)
        rte_exit(EXIT_FAILURE, "Append failed - not enough tailroom\n");

    memcpy(data, msg, len);

    printf("Data written to mbuf: %s\n", (char *)data);

    
     //5. Read data from mbuf (mtod)
    char *ptr = rte_pktmbuf_mtod(m, char *);
    printf("Data read from mbuf: %s\n", ptr);

     // 6. Free the mbuf back to mempool
    rte_pktmbuf_free(m);

    printf("Mbuf freed!\n");

    return 0;
}
