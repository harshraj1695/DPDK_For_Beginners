#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <rte_common.h>
#include <rte_eal.h>
#include <rte_lcore.h>
#include <rte_power_cpufreq.h>


// NOTE: This example demonstrates how to use DPDK's power management API to adjust CPU frequencies. It is not a graph application and does not involve packet processing.
// if you are running it on hyperv vm you will Errors
int main(int argc, char **argv) {
  int ret;
  unsigned lcore_id;

  ret = rte_eal_init(argc, argv);
  if (ret < 0)
    rte_exit(EXIT_FAILURE, "EAL init failed\n");

    // Find a worker lcore to run the power management code
  RTE_LCORE_FOREACH_WORKER(lcore_id) {
    break;
  }

  printf("Using worker lcore %u\n", lcore_id);

  // Initialize power management for the selected lcore
  if (rte_power_init(lcore_id) < 0)
    rte_exit(EXIT_FAILURE, "Power init failed\n");

  printf("Power management initialized\n");

  uint32_t freq = rte_power_get_freq(lcore_id);
  printf("Current CPU frequency index: %u\n", freq);

  printf("Increasing CPU frequency\n");

  for (int i = 0; i < 3; i++) {
    rte_power_freq_up(lcore_id);
    printf("Current CPU frequency index: %u\n", rte_power_get_freq(lcore_id));
    sleep(1);
  }

  printf("Decreasing CPU frequency\n");

  for (int i = 0; i < 3; i++) {
    rte_power_freq_down(lcore_id);
    printf("Current CPU frequency index: %u\n", rte_power_get_freq(lcore_id));
    sleep(1);
  }

  printf("Setting MAX frequency\n");
  rte_power_freq_max(lcore_id);
  printf("Current CPU frequency index: %u\n", rte_power_get_freq(lcore_id));

  sleep(2);

  printf("Setting MIN frequency\n");
  rte_power_freq_min(lcore_id);
  printf("Current CPU frequency index: %u\n", rte_power_get_freq(lcore_id));

  sleep(2);

  rte_power_exit(lcore_id);

  printf("Power management exited\n");

  return 0;
}