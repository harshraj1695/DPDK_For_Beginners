#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include <rte_eal.h>
// #include <rte_mp.h>
#include <rte_ring.h>

#define RING_NAME "DATA_RING"

static struct rte_ring *ring;

/* MP callback */
static int check_cb(const struct rte_mp_msg *msg, const void *peer)
{
    printf("Secondary: Ready check received\n");

    struct rte_mp_msg reply = {0};
    strcpy(reply.name, "ready_reply");

    rte_mp_sendmsg(&reply);
    return 0;
}

int main(int argc, char **argv)
{
    rte_eal_init(argc, argv);

    if (rte_eal_process_type() != RTE_PROC_SECONDARY)
        rte_exit(EXIT_FAILURE, "Run as secondary\n");

    /* Register MP handler */
    rte_mp_action_register("ready_check", check_cb);

    /* Lookup ring */
    while ((ring = rte_ring_lookup(RING_NAME)) == NULL) {
        printf("Secondary waiting for ring...\n");
        sleep(1);
    }

    printf("Secondary: Ring found\n");

    /* Receive integers */
    while (1) {
        void *data;

        if (rte_ring_dequeue(ring, &data) == 0)
            printf("Secondary received %ld\n", (long)data);

        usleep(200000);
    }

    return 0;
}
