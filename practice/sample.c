#include <rte_eal.h>
#include <rte_interrupts.h>
#include <rte_ring.h>
#include <rte_malloc.h>


int handler(void *arg){\
    int *val=(int *)arg;
    printf("handler called\n");
    printf("lcore id %d\n", rte_lcore_id());
    printf("value is %d\n",*val);
    return 0;
}
int main(int argc, char **argv){
    int ret=rte_eal_init(argc, argv);
    if(ret < 0)
        rte_exit(EXIT_FAILURE, "EAL init failed\n");

    int *a= rte_malloc(NULL, sizeof(int),0);
    *a=10;
    int rt=rte_eal_remote_launch(handler,a,1);
    printf("Eal init done\n");
   return 0;   
}