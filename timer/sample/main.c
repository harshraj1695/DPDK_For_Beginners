#include <stdio.h>
#include <rte_eal.h>
#include <rte_cycles.h>
#include <rte_lcore.h>

int main(int argc, char *argv[])
{
    rte_eal_init(argc, argv);

    uint64_t hz = rte_get_timer_hz();
    uint64_t start_tsc = 0;
    int started = 0;
    int n;

    printf("Enter any number to start the timer.\n");
    printf("  0 -> restart timer\n");
    printf("  9 -> print current second\n");

    while (1) {
        scanf("%d", &n);

        if (!started) {
            start_tsc = rte_get_timer_cycles();
            started = 1;
            printf("Timer started.\n");
            continue;
        }

        if (n == 0) {
            start_tsc = rte_get_timer_cycles();
            printf("Timer restarted.\n");
        } else if (n == 9) {
            uint64_t elapsed = (rte_get_timer_cycles() - start_tsc) / hz;
            printf("Current second: %lu\n", elapsed);
        }
    }

    return 0;
}