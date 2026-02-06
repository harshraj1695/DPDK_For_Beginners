#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include <rte_eal.h>
// #include <rte_mp.h>
#include <rte_ring.h>

#define RING_NAME "DATA_RING"

static int secondary_ready = 0;


// MP callback for ready reply
static int ready_cb(const struct rte_mp_msg *msg, const void *peer)
{
    printf("Primary: Secondary is ready\n");
    secondary_ready = 1;
    return 0;
}

int main(int argc, char **argv)
{
    rte_eal_init(argc, argv);

    if (rte_eal_process_type() != RTE_PROC_PRIMARY)
        rte_exit(EXIT_FAILURE, "Run as primary\n");

    // Create ring 
    struct rte_ring *ring =
        rte_ring_create(RING_NAME, 32, rte_socket_id(), 0);

    if (!ring)
        rte_exit(EXIT_FAILURE, "Ring create failed\n");

        // Register MP handler
    rte_mp_action_register("ready_reply", ready_cb);

    // Send ready check to secondary
    struct rte_mp_msg msg = {0};
    strcpy(msg.name, "ready_check");

    printf("Primary: Checking if secondary is ready...\n");
while (!secondary_ready) {
    printf("Primary: Sending ready check...\n");
    rte_mp_sendmsg(&msg);
    sleep(1);
}


// Wait for secondary to reply
while (!secondary_ready)
        sleep(1);

    // Send integers through ring to secondary
    for (int i = 0; i < 5; i++) {
        rte_ring_enqueue(ring, (void *)(long)i);
        printf("Primary sent %d\n", i);
        sleep(1);
    }

    return 0;
}
