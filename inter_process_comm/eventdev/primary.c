#include <generic/rte_cycles.h>
#include <rte_eal.h>
#include <rte_eventdev.h>
#include <rte_mempool.h>
#include <rte_mbuf.h>
#include <stdio.h>

#define NUM_MBUFS 1024
#define CACHE_SIZE 32
#define EVENT_DEV_ID 0

int main(int argc, char **argv)
{
    int ret = rte_eal_init(argc, argv);
    if (ret < 0)
        rte_exit(EXIT_FAILURE, "EAL init failed\n");

    printf("Primary process started\n");

    /* ---------- Create mempool ---------- */
    struct rte_mempool *mp = rte_pktmbuf_pool_create(
        "MBUF_POOL",
        NUM_MBUFS,
        CACHE_SIZE,
        0,
        RTE_MBUF_DEFAULT_BUF_SIZE,
        rte_socket_id());

    if (!mp)
        rte_exit(EXIT_FAILURE, "Cannot create mempool\n");

    /* ---------- Configure eventdev ---------- */
    struct rte_event_dev_config dev_conf = {0};
    dev_conf.nb_event_queues = 1;
    dev_conf.nb_event_ports  = 2; // one for primary, one for secondary
    dev_conf.nb_event_queue_flows = 1024;
    dev_conf.nb_events_limit = 4096;
    dev_conf.nb_event_port_dequeue_depth = 32;
    dev_conf.nb_event_port_enqueue_depth = 32;

    rte_event_dev_configure(EVENT_DEV_ID, &dev_conf);

    /* ---------- Setup queue ---------- */
    struct rte_event_queue_conf q_conf = {0};
    q_conf.nb_atomic_flows = 1024;
    q_conf.schedule_type = RTE_SCHED_TYPE_ATOMIC;

    rte_event_queue_setup(EVENT_DEV_ID, 0, &q_conf);

    /* ---------- Setup ports ---------- */
struct rte_event_port_conf p_conf = {
    .new_event_threshold = 1024,
    .dequeue_depth = 32,
    .enqueue_depth = 32,
};


    rte_event_port_setup(EVENT_DEV_ID, 0, &p_conf); // primary port
    rte_event_port_setup(EVENT_DEV_ID, 1, &p_conf); // secondary port

    rte_event_port_link(EVENT_DEV_ID, 0, NULL, NULL, 0);
    rte_event_port_link(EVENT_DEV_ID, 1, NULL, NULL, 0);

    rte_event_dev_start(EVENT_DEV_ID);

    printf("Primary sending events...\n");

    while (1)
    {
        struct rte_mbuf *mbuf = rte_pktmbuf_alloc(mp);
        if (!mbuf)
            continue;

        struct rte_event ev = {0};
        ev.queue_id = 0;
        ev.op = RTE_EVENT_OP_NEW;
        ev.event_type = RTE_EVENT_TYPE_CPU;
        ev.sched_type = RTE_SCHED_TYPE_ATOMIC;
        ev.mbuf = mbuf;

        rte_event_enqueue_burst(EVENT_DEV_ID, 0, &ev, 1);

        // sleep(1);
        rte_delay_us_sleep(100000); // Sleep for 100ms
    }

    return 0;
}
