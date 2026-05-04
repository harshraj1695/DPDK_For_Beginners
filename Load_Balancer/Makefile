CC = gcc
CFLAGS = -O3 -march=native -I$(RTE_SDK)/include -I$(RTE_SDK)/x86_64-native-linuxapp-gcc/include
LDFLAGS = -L$(RTE_SDK)/x86_64-native-linuxapp-gcc/lib -lrte_eal -lrte_mempool -lrte_ethdev -lrte_mbuf -lrte_ip_frag -lrte_kvargs -lpthread

all: load_balancer

load_balancer: load_balancer.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

clean:
	rm -f load_balancer
