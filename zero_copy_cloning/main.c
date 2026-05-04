#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include <rte_eal.h>
#include <rte_mbuf.h>
#include <rte_mempool.h>

#define NUM_MBUFS 8192
#define MBUF_CACHE_SIZE 250
#define BURST_SIZE 32

int main(int argc, char **argv)
{
    int ret;

    ret = rte_eal_init(argc, argv);
    if (ret < 0)
        rte_panic("EAL init failed\n");

    printf("DPDK initialized\n");

    // Create mempool
    struct rte_mempool *mbuf_pool = rte_pktmbuf_pool_create(
        "MBUF_POOL",
        NUM_MBUFS,
        MBUF_CACHE_SIZE,
        0,
        RTE_MBUF_DEFAULT_BUF_SIZE,
        rte_socket_id()
    );

    if (mbuf_pool == NULL)
        rte_panic("Cannot create mbuf pool\n");

    printf("Mempool created\n");

    // Allocate original mbuf
    struct rte_mbuf *m1 = rte_pktmbuf_alloc(mbuf_pool);
    if (!m1)
        rte_panic("mbuf alloc failed\n");

    // Get data pointer
    char *data = rte_pktmbuf_mtod(m1, char *);

    // Fill packet data
    strcpy(data, "Hello DPDK Zero Copy!");

    // Set lengths
    m1->data_len = strlen(data) + 1;
    m1->pkt_len  = m1->data_len;

    printf("Original packet data: %s\n", data);

    // ZERO-COPY CLONE
    struct rte_mbuf *m2 = rte_pktmbuf_clone(m1, mbuf_pool);
    if (!m2)
        rte_panic("clone failed\n");

    char *data_clone = rte_pktmbuf_mtod(m2, char *);

    printf("Cloned packet data: %s\n", data_clone);  

    // Show both point to SAME buffer
    printf("\nBuffer addresses:\n");
    printf("Original: %p\n", data);
    printf("Clone   : %p\n", data_clone);

    if (data == data_clone)
        printf("Zero-copy confirmed (same buffer)\n");
    else
        printf("Copy happened (unexpected)\n");

    // Show refcount
    printf("\nRefcnt after clone: %d\n",
           rte_mbuf_refcnt_read(m1));

    // Modify through clone (affects original!)
    strcpy(data_clone, "MODIFIED!");

    printf("\nAfter modifying clone:\n");
    printf("Original sees: %s\n", data);
    printf("Clone sees   : %s\n", data_clone);

    // Free both
    rte_pktmbuf_free(m1);
    printf("\nAfter freeing original, refcnt = %d\n",
           rte_mbuf_refcnt_read(m2));

    rte_pktmbuf_free(m2);

    printf("Both freed successfully\n");

    return 0;
}