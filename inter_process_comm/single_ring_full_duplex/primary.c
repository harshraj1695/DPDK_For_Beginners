#include <stdio.h>
#include <unistd.h>
#include <rte_eal.h>
#include <rte_ring.h>
#include <rte_malloc.h>

#include "common.h"

int main(int argc, char **argv)
{
    rte_eal_init(argc, argv);

    struct rte_ring *ring = rte_ring_create(
        RING_NAME,
        RING_SIZE,
        rte_socket_id(),
        0   
    );

    if (!ring)
        rte_exit(EXIT_FAILURE, "Ring create failed\n");

    printf("Primary: ring created\n");

    // Send requests
    for (int i = 0; i < 10; i++) {
        struct msg *m = rte_malloc(NULL, sizeof(*m), 0);
        m->src_id = PROC_A;
        m->dst_id = PROC_B;
        m->type   = MSG_REQ;
        m->value  = i;

        while (rte_ring_enqueue(ring, m) < 0)
            rte_pause();

        printf("Primary: sent %d\n", i);

        //  Wait for response
        for (;;) {
            void *obj;
            if (rte_ring_dequeue(ring, &obj) == 0) {
                struct msg *r = obj;

                if (r->dst_id != PROC_A) {
                    //  not for me  put back 
                    rte_ring_enqueue(ring, r);
                    continue;
                }

                if (r->type == MSG_RESP) {
                    printf("Primary: received %d\n", r->value);
                    rte_free(r);
                    break;
                }
            }
        }
    }

    // Send exit 
    struct msg *exit_msg = rte_malloc(NULL, sizeof(*exit_msg), 0);
    exit_msg->src_id = PROC_A;
    exit_msg->dst_id = PROC_B;
    exit_msg->type   = MSG_EXIT;
    exit_msg->value  = 0;

    rte_ring_enqueue(ring, exit_msg);

    printf("Primary: done\n");
    return 0;
}
