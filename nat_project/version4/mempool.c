#include <stdio.h>
#include <rte_mbuf.h>
#include <rte_mempool.h>
#include <rte_eal.h>
#include "source.h"

struct rte_mempool* create_mempool()
{
    struct rte_mempool *mp = rte_pktmbuf_pool_create(
        "MBUF_POOL",
        8192,
        256,
        0,
        RTE_MBUF_DEFAULT_BUF_SIZE,
        rte_socket_id()
    );

    if (!mp)
        rte_exit(EXIT_FAILURE, "Cannot create mempool\n");

    printf("Mempool created.\n");
    return mp;
}
