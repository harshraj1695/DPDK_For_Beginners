#include <rte_eal.h>
#include <rte_launch.h>
#include <rte_lcore.h>
#include <rte_ring.h>
#include <rte_malloc.h>
#include <rte_ring_core.h>


int handler (void* ring){
    struct rte_ring* ring1=(struct rte_ring*)ring;
    if(!ring1){
        rte_exit(EXIT_FAILURE, "Ring is NULL\n");
    }
    printf("In handler function\n");
    printf("lcore id %d\n", rte_lcore_id());
    int *data=rte_malloc(NULL, sizeof(int),0);
    *data=20;
    rte_ring_enqueue(ring1,data);
    printf("Data enqueued from handler\n");
    return 0;
}
int handler2(void *arg){
    struct rte_ring*ring2=(struct rte_ring*)arg;
    if(!ring2){
        rte_exit(EXIT_FAILURE, "Ring is NULL\n");
    }
    printf("In handler2 function\n");
    printf("lcore id %d\n", rte_lcore_id());
    void *ptr=NULL;
    while(rte_ring_dequeue(ring2,&ptr)<0){
        rte_pause();
    }
    int *val=ptr;
    printf("Data dequeued from handler2: %d\n",*val);
    return 0;
}
int main(int argc, char** argv){
    int rt=rte_eal_init(argc, argv);
    if(rt < 0){
        rte_exit(EXIT_FAILURE, "EAL init failed\n");
    }
   struct rte_ring* ring=rte_ring_create("ring1",1024, rte_socket_id(),0);
    if(!ring){
        rte_exit(EXIT_FAILURE, "Ring creation failed\n");
    }
    printf("Ring created successfully\n");

    rte_eal_remote_launch(handler,ring,1);
    rte_eal_remote_launch(handler2,ring,2);

    rte_eal_wait_lcore(0);
    rte_eal_wait_lcore(1);

    rte_ring_free(ring);
    return 0;
}