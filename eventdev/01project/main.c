
#include <rte_eal.h>
#include <rte_eventdev.h>
#include <stdint.h>
#include <stdio.h>

#define EVENT_DEV_ID 0
#define NUM_QUEUES 1
#define NUM_PORTS 2

int main(int argc, char **argv)
{
    int ret;

    /* Step 1: Init EAL */
    ret = rte_eal_init(argc, argv);
    if (ret < 0)
        rte_exit(EXIT_FAILURE, "EAL init failed\n");

    printf("EAL initialized\n");

    /* Step 2: Configure Event Device */
    struct rte_event_dev_config dev_conf = {0};
    dev_conf.nb_event_queues = NUM_QUEUES;
    dev_conf.nb_event_ports  = NUM_PORTS;
    dev_conf.nb_event_port_dequeue_depth = 32;
    dev_conf.nb_event_port_enqueue_depth = 32;
    dev_conf.nb_event_queue_flows = 1024;
    dev_conf.nb_events_limit = 4096;

    if (rte_event_dev_configure(EVENT_DEV_ID, &dev_conf) < 0)
        rte_exit(EXIT_FAILURE, "Configure failed\n");

    printf("Event device configured\n");

    /* Step 3: Setup Queue */
    struct rte_event_queue_conf qconf = {0};
    qconf.schedule_type = RTE_SCHED_TYPE_ATOMIC;
    qconf.nb_atomic_flows = 1024;
    qconf.nb_atomic_order_sequences = 1024;
    qconf.priority = RTE_EVENT_DEV_PRIORITY_NORMAL;

    if (rte_event_queue_setup(EVENT_DEV_ID, 0, &qconf) < 0)
        rte_exit(EXIT_FAILURE, "Queue setup failed\n");

    printf("Queue created\n");

    /* Step 4: Setup Ports */

    struct rte_event_port_conf pconf;

    /* Producer port (port 0) */
    rte_event_port_default_conf_get(EVENT_DEV_ID, 0, &pconf);
    pconf.event_port_cfg = RTE_EVENT_PORT_CFG_HINT_PRODUCER;
    if (rte_event_port_setup(EVENT_DEV_ID, 0, &pconf) < 0)
        rte_exit(EXIT_FAILURE, "Producer port failed\n");

    /* Consumer port (port 1) */
    rte_event_port_default_conf_get(EVENT_DEV_ID, 1, &pconf);
    pconf.event_port_cfg = RTE_EVENT_PORT_CFG_HINT_CONSUMER;
    if (rte_event_port_setup(EVENT_DEV_ID, 1, &pconf) < 0)
        rte_exit(EXIT_FAILURE, "Consumer port failed\n");

    printf("Ports created\n");

    /* Link consumer port to queue */
    uint8_t queues[] = {0};
    uint8_t priorities[] = {RTE_EVENT_DEV_PRIORITY_NORMAL};
    rte_event_port_link(EVENT_DEV_ID, 1, queues, priorities, 1);

    /* Step 5: Start device */
    if (rte_event_dev_start(EVENT_DEV_ID) < 0)
        rte_exit(EXIT_FAILURE, "Start failed\n");

    printf("Event device started\n");

    /* Step 6: Create event */
    struct rte_event ev = {0};
    ev.queue_id = 0;
    ev.op = RTE_EVENT_OP_NEW;
    ev.sched_type = RTE_SCHED_TYPE_ATOMIC;
    ev.event_type = RTE_EVENT_TYPE_CPU;
    ev.flow_id = 1;
    ev.u64 = 12345;

    printf("Enqueue event...\n");

    while (rte_event_enqueue_burst(EVENT_DEV_ID, 0, &ev, 1) != 1)
        rte_pause();

    /* Step 7: Dequeue */
    struct rte_event out;

    printf("Waiting for event...\n");

    while (1) {
        if (rte_event_dequeue_burst(EVENT_DEV_ID, 1, &out, 1, 0)) {
            printf("Received event! data=%lu\n", out.u64);
            break;
        }
    }

    /* Cleanup */
    rte_event_dev_stop(EVENT_DEV_ID);
    rte_event_dev_close(EVENT_DEV_ID);

    printf("Done.\n");
    return 0;
}


