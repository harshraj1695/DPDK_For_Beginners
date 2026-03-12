#include <stdio.h>
#include <unistd.h>
#include <stdint.h>

#include <rte_eal.h>
#include <rte_lcore.h>
#include <rte_power_cpufreq.h>
#include <rte_cycles.h>

/* thresholds used to decide scaling */
#define HIGH_LOAD_THRESHOLD 8000000
#define LOW_LOAD_THRESHOLD  2000000


static int
power_worker(__rte_unused void *arg)
{
    unsigned lcore_id = rte_lcore_id();   // get current logical core ID
    uint64_t prev_tsc, cur_tsc, diff;

    printf("Worker running on lcore %u\n", lcore_id);

    /*
        rte_power_init()

        Initializes DPDK power management for this lcore.

        Internally it:
        - Detects CPU power management driver
        - Opens cpufreq sysfs interface
        - Prepares function pointers for freq scaling
    */
    if (rte_power_init(lcore_id) < 0) {
        printf("Power init failed\n");
        return -1;
    }

    prev_tsc = rte_rdtsc();  // read timestamp counter

    while (1) {

        /* simulate workload */
        for (volatile int i = 0; i < 10000000; i++);

        cur_tsc = rte_rdtsc();
        diff = cur_tsc - prev_tsc;
        prev_tsc = cur_tsc;

        if (diff > HIGH_LOAD_THRESHOLD) {

            /*
                rte_power_freq_max()

                Sets CPU frequency to the maximum available level.

                Used when:
                - packet rate is high
                - CPU load increases
                - low latency is required
            */
            rte_power_freq_max(lcore_id);

            /*
                rte_power_get_freq()

                Returns the current frequency index.

                NOTE:
                This is NOT the MHz value.
                It is the index inside the CPU frequency table.
            */
            printf("High load → increasing frequency (index %u)\n",
                   rte_power_get_freq(lcore_id));
        }

        else if (diff < LOW_LOAD_THRESHOLD) {

            /*
                rte_power_freq_min()

                Sets CPU frequency to the lowest available level.

                Used when:
                - traffic is low
                - CPU utilization is small
                - energy saving is desired
            */
            rte_power_freq_min(lcore_id);

            printf("Low load → decreasing frequency (index %u)\n",
                   rte_power_get_freq(lcore_id));
        }

        else {

            /*
                rte_power_get_freq()

                Just read current CPU frequency index
                without modifying the scaling state.
            */
            printf("Moderate load → keeping frequency (index %u)\n",
                   rte_power_get_freq(lcore_id));
        }

        sleep(1);
    }

    /*
        rte_power_exit()

        Cleans up power management for this lcore.

        Internally it:
        - resets scaling governor
        - releases cpufreq interface
        - clears environment state
    */
    rte_power_exit(lcore_id);

    return 0;
}


int main(int argc, char **argv)
{
    int ret;

    /* initialize DPDK environment abstraction layer */
    ret = rte_eal_init(argc, argv);
    if (ret < 0)
        rte_exit(EXIT_FAILURE, "EAL init failed\n");

    printf("Starting automatic power management demo\n");

    /*
        rte_eal_remote_launch()

        Launch the worker function on another lcore.
        Here we start our power management loop on lcore 1.
    */
    rte_eal_remote_launch(power_worker, NULL, 1);

    /* wait until all lcores finish */
    rte_eal_mp_wait_lcore();

    return 0;
}