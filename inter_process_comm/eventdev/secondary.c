#include <rte_eal.h>
#include <rte_eventdev.h>
#include <rte_mbuf.h>
#include <stdio.h>

#define EVENT_DEV_ID 0

int main(int argc, char **argv)
{
    int ret = rte_eal_init(argc, argv);
    if (ret < 0)
        rte_exit(EXIT_FAILURE, "EAL init failed\n");

    printf("Secondary process started\n");

    while (1)
    {
        struct rte_event ev;

        uint16_t nb = rte_event_dequeue_burst(
            EVENT_DEV_ID,
            1, // use port 1 (secondary port)
            &ev,
            1,
            0);

        if (nb > 0)
        {
            printf("Secondary received event!\n");

            rte_pktmbuf_free(ev.mbuf);
        }
    }

    return 0;
}
