#include <stdio.h>
#include <rte_eal.h>
#include <rte_lcore.h>
#include <rte_launch.h>

int func2(void *arg) {
    printf("Inside func2: running on lcore %u\n", rte_lcore_id());
    return 0;
}

int func1(void *arg) {
    printf("Inside func1: running on lcore %u\n", rte_lcore_id());

     func2(NULL);     // This will also run on SAME LCORR

    return 0;
}

int main(int argc, char **argv) {
    rte_eal_init(argc, argv);

    unsigned target = 1;   
    unsigned target2=2;

    printf("Launching func1 on lcore %u...\n", target);
    rte_eal_remote_launch(func1, NULL, target);

     printf("Launching func2 on lcore %u...\n", target2);
    rte_eal_remote_launch(func2, NULL, target2);


    rte_eal_mp_wait_lcore();  
    return 0;
}
