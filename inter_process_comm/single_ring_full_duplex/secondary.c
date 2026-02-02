#include <stdio.h>
#include <unistd.h>
#include <rte_eal.h>
#include <rte_ring.h>
#include <rte_malloc.h>

#include "common.h"

int main(int argc, char **argv)
{
    rte_eal_init(argc, argv);

    struct rte_ring *ring = NULL;
    while (!ring) {
        ring = rte_ring_lookup(RING_NAME);
        sleep(1);
    }

    printf("Secondary: ring found\n");

    for (;;) {
        void *obj;

        if (rte_ring_dequeue(ring, &obj) == 0) {
            struct msg *m = obj;

            if (m->dst_id != PROC_B) {
                //not for me put back
                rte_ring_enqueue(ring, m);
                continue;
            }

            if (m->type == MSG_EXIT) {
                rte_free(m);
                printf("Secondary: exit\n");
                break;
            }

            if (m->type == MSG_REQ) {
                printf("Secondary: received %d\n", m->value);

                struct msg *resp = rte_malloc(NULL, sizeof(*resp), 0);
                resp->src_id = PROC_B;
                resp->dst_id = PROC_A;
                resp->type   = MSG_RESP;
                resp->value  = m->value * 10;

                rte_free(m);

                while (rte_ring_enqueue(ring, resp) < 0)
                    rte_pause();

                printf("Secondary: sent %d\n", resp->value);
            }
        }
    }

    return 0;
}
