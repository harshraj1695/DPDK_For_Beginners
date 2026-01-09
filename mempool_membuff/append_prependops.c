#include <stdio.h>
#include <string.h>

#include <rte_eal.h>
#include <rte_mbuf.h>
#include <rte_mempool.h>

int main(int argc, char **argv)
{
    //  * 1. Initialize DPDK EAL
    if (rte_eal_init(argc, argv) < 0)
        rte_exit(EXIT_FAILURE, "EAL init failed\n");

    //  * 2. Create mempool
    struct rte_mempool *mp = rte_pktmbuf_pool_create(
        "POOL",
        1024,
        256,
        0,
        RTE_MBUF_DEFAULT_BUF_SIZE,
        rte_socket_id()
    );
    if (!mp)
        rte_exit(EXIT_FAILURE, "Failed to create mempool\n");

    //  * 3. Allocate one mbuf
    struct rte_mbuf *m = rte_pktmbuf_alloc(mp);
    if (!m)
        rte_exit(EXIT_FAILURE, "Failed to allocate mbuf\n");

    printf("Allocated mbuf!\n");

    //  * 4. Print initial state of mbuf
    printf("\n=== Initial Mbuf State ===\n");
    printf("data_off   = %u\n", m->data_off);
    printf("data_len   = %u\n", m->data_len);
    printf("pkt_len    = %u\n", m->pkt_len);
    printf("headroom   = %u\n", rte_pktmbuf_headroom(m));
    printf("tailroom   = %u\n", rte_pktmbuf_tailroom(m));


    //  * 5. Append some data
    const char *msg = "DPDK-MBUF";
    uint16_t len = strlen(msg) + 1;

    char *data = rte_pktmbuf_append(m, len);
    if (!data)
        rte_exit(EXIT_FAILURE, "Append failed\n");

    memcpy(data, msg, len);

    printf("\n=== After Append(%u bytes) ===\n", len);
    printf("data_off   = %u\n", m->data_off);
    printf("data_len   = %u\n", m->data_len);
    printf("pkt_len    = %u\n", m->pkt_len);
    printf("headroom   = %u\n", rte_pktmbuf_headroom(m));
    printf("tailroom   = %u\n", rte_pktmbuf_tailroom(m));


    //  * 6. Prepend some bytes (add header)
    char *hdr = rte_pktmbuf_prepend(m, 10);
    if (hdr) {
        memset(hdr, 'H', 10);
    }

    printf("\n=== After Prepend(10 bytes) ===\n");
    printf("data_off   = %u\n", m->data_off);
    printf("data_len   = %u\n", m->data_len);
    printf("pkt_len    = %u\n", m->pkt_len);
    printf("headroom   = %u\n", rte_pktmbuf_headroom(m));
    printf("tailroom   = %u\n", rte_pktmbuf_tailroom(m));


    //  * 7. Remove 5 bytes from beginning (adj)
    rte_pktmbuf_adj(m, 5);

    printf("\n=== After Adj(5 bytes removed) ===\n");
    printf("data_off   = %u\n", m->data_off);
    printf("data_len   = %u\n", m->data_len);
    printf("pkt_len    = %u\n", m->pkt_len);
    printf("headroom   = %u\n", rte_pktmbuf_headroom(m));
    printf("tailroom   = %u\n", rte_pktmbuf_tailroom(m));


    //  * 8. Trim tail (remove last bytes)
    rte_pktmbuf_trim(m, 4);

    printf("\n=== After Trim(4 bytes) ===\n");
    printf("data_off   = %u\n", m->data_off);
    printf("data_len   = %u\n", m->data_len);
    printf("pkt_len    = %u\n", m->pkt_len);
    printf("headroom   = %u\n", rte_pktmbuf_headroom(m));
    printf("tailroom   = %u\n", rte_pktmbuf_tailroom(m));

    /* --------------------------------------------------
     * 9. Free mbuf
     * -------------------------------------------------- */
    rte_pktmbuf_free(m);

    return 0;
}
