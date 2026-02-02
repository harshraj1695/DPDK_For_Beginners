APP = main

SRCS = main.c

CFLAGS = -I/usr/local/include -O2 -g -march=native
LDFLAGS = -L/usr/local/lib/x86_64-linux-gnu

LDLIBS = -lrte_graph -lrte_node -lrte_dispatcher \
         -lrte_eal -lrte_ethdev -lrte_mbuf \
         -lrte_mempool -lrte_ring -lrte_net

all:
	$(CC) $(SRCS) -o $(APP) $(CFLAGS) $(LDFLAGS) $(LDLIBS)
