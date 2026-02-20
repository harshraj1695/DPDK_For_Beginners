/*
 * DPDK Telemetry – Advanced Stats Example
 *
 * This app exposes richer telemetry:
 *  - /adv/summary      : high-level app stats (uptime, lcore count, loops)
 *  - /adv/lcore_stats  : per-lcore counters (loops per lcore)
 *
 * It does NOT change the existing sample app.
 *
 * Run:
 *   ./advanced_stats_main -l 0-2
 *
 * Then query with either:
 *   dpdk-telemetry.py        (interactive)
 *   or python3 adv_client.py "/adv/summary"
 */

#include <rte_eal.h>
#include <rte_lcore.h>
#include <rte_telemetry.h>
#include <rte_cycles.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <inttypes.h>

struct lcore_stat {
    uint64_t loops;
};

static volatile uint64_t g_total_loops;
static struct lcore_stat g_lcore_stats[RTE_MAX_LCORE];
static uint64_t g_start_tsc;

static int
worker_loop(__rte_unused void *arg)
{
    const unsigned lcore_id = rte_lcore_id();

    for (;;) {
        g_lcore_stats[lcore_id].loops++;
        g_total_loops++;
        /* Small pause so numbers grow but not too fast */
        rte_delay_us_block(1000); /* 1 ms */
    }

    return 0;
}

/* /adv/summary: dictionary with app-wide stats */
static int
adv_summary_handler(const char *cmd __rte_unused,
                    const char *params __rte_unused,
                    struct rte_tel_data *d)
{
    uint64_t now = rte_rdtsc();
    uint64_t hz = rte_get_tsc_hz();
    uint64_t uptime_sec = hz ? (now - g_start_tsc) / hz : 0;

    rte_tel_data_start_dict(d);
    rte_tel_data_add_dict_string(d, "app", "advanced-telemetry");
    rte_tel_data_add_dict_uint(d, "uptime_sec", uptime_sec);
    rte_tel_data_add_dict_uint(d, "lcore_count", rte_lcore_count());
    rte_tel_data_add_dict_uint(d, "total_loops", g_total_loops);

    return 0;
}

/* /adv/lcore_stats: array of per-lcore loop counters */
static int
adv_lcore_stats_handler(const char *cmd __rte_unused,
                        const char *params __rte_unused,
                        struct rte_tel_data *d)
{
    unsigned lcore_id;

    rte_tel_data_start_array(d, RTE_TEL_UINT_VAL);

    RTE_LCORE_FOREACH(lcore_id) {
        rte_tel_data_add_array_uint(d, g_lcore_stats[lcore_id].loops);
    }

    return 0;
}

int
main(int argc, char **argv)
{
    int ret;
    unsigned lcore_id;

    ret = rte_eal_init(argc, argv);
    if (ret < 0)
        rte_exit(EXIT_FAILURE, "EAL init failed\n");

    printf("EAL initialized (advanced_stats)\n");

    g_start_tsc = rte_rdtsc();

    /* Launch worker on all non-main lcores */
    RTE_LCORE_FOREACH_WORKER(lcore_id) {
        rte_eal_remote_launch(worker_loop, NULL, lcore_id);
    }

    /* Small delay to ensure telemetry is ready */
    sleep(1);

    /* Register telemetry commands */
    ret = rte_telemetry_register_cmd("/adv/summary", adv_summary_handler,
                                     "Advanced app summary stats");
    printf("Register /adv/summary -> %d\n", ret);

    ret = rte_telemetry_register_cmd("/adv/lcore_stats", adv_lcore_stats_handler,
                                     "Per-lcore loop counters");
    printf("Register /adv/lcore_stats -> %d\n", ret);

    printf("PID %d running advanced_stats...\n", getpid());
    printf("Use dpdk-telemetry.py or adv_client.py to query /adv/summary and /adv/lcore_stats\n");

    /* Main lcore just sleeps; workers update stats */
    for (;;)
        sleep(1);

    return 0;
}

