#include <stdio.h>
#include <stdlib.h>

#include <rte_common.h>  // for rte_exit()
#include <rte_eal.h>     // for rte_eal_init()

int main(int argc, char **argv) {

    if (rte_eal_init(argc, argv) < 0)
        rte_exit(EXIT_FAILURE, "EAL init failed\n");

    printf("Hello, World from DPDK!\n");

    return 0;
}

