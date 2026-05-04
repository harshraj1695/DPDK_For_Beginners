#include <stdio.h>
#include <rte_mbuf.h>
#include <rte_mempool.h>
#include <rte_eal.h>
#include <rte_errno.h>
#include "source.h"

struct rte_mempool* create_mempool()
{
    const unsigned int num_mbufs = 2048;
    const unsigned int cache_size = 256;
    const unsigned int priv_size = 0;
    const uint16_t data_room_size = RTE_MBUF_DEFAULT_BUF_SIZE;

    printf("Creating mempool: num_mbufs=%u cache=%u priv=%u data_room=%u socket=%d\n",
           num_mbufs, cache_size, priv_size, data_room_size, rte_socket_id());

    struct rte_mempool *mp = rte_pktmbuf_pool_create(
        "MBUF_POOL",
        num_mbufs,
        cache_size,
        priv_size,
        data_room_size,
        rte_socket_id()
    );

    if (!mp)
        rte_exit(EXIT_FAILURE, "Cannot create mempool (rte_errno=%d: %s)\n",
                 rte_errno, rte_strerror(rte_errno));

    printf("Mempool created: name=%s\n", mp->name);
    return mp;
}
