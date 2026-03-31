#include <stdio.h>
#include <rte_eal.h>
#include <rte_timer.h>
#include <rte_cycles.h>
#include <rte_lcore.h>

static struct rte_timer my_timer;
static uint64_t hz;
static uint64_t start_tsc;

static void timer_cb(struct rte_timer *tim, void *arg)
{
    printf("5 seconds done!\n");
    rte_timer_reset(tim, hz * 5, SINGLE, rte_lcore_id(), timer_cb, arg);
    start_tsc = rte_get_timer_cycles();
}

int main(int argc, char *argv[])
{
    rte_eal_init(argc, argv);
    rte_timer_subsystem_init();
    rte_timer_init(&my_timer);

    hz = rte_get_timer_hz();

    printf("Enter any number to start the timer.\n");
    printf("  0 -> restart timer\n");
    printf("  9 -> print current second\n");

    int n;
    int started = 0;

    while (1) {
        rte_timer_manage();

        scanf("%d", &n);

        if (!started) {
            /* first input — start the timer */
            rte_timer_reset(&my_timer, hz * 5, SINGLE, rte_lcore_id(), timer_cb, NULL);
            start_tsc = rte_get_timer_cycles();
            started = 1;
            printf("Timer started.\n");
            continue;
        }

        if (n == 0) {
            rte_timer_stop(&my_timer);
            rte_timer_reset(&my_timer, hz * 5, SINGLE, rte_lcore_id(), timer_cb, NULL);
            start_tsc = rte_get_timer_cycles();
            printf("Timer restarted.\n");
        } else if (n == 9) {
            uint64_t elapsed = (rte_get_timer_cycles() - start_tsc) / hz;
            printf("Current second: %lu\n", elapsed);
        } else {
            printf("Timer still running.\n");
        }
    }

    return 0;
}