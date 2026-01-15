# APP = sample

# # Use pkg-config to get correct flags for your DPDK installation
# PKGCONF = $(shell pkg-config --cflags --libs libdpdk)

# all: $(APP)

# $(APP): sample.c
# 	$(CC) sample.c -o $(APP) $(PKGCONF)

# clean:
# 	rm -f $(APP)


APP = dpdk_tun_echo
SRCS = dpdk_tun_echo.c

CFLAGS += -O3 -I/usr/local/include -include rte_config.h -march=native
LDFLAGS += -L/usr/local/lib/x86_64-linux-gnu \
           -lrte_eal -lrte_mbuf -lrte_mempool -lrte_ethdev -lrte_kvargs -lrte_bus_vdev

all:
	cc $(SRCS) -o $(APP) $(CFLAGS) $(LDFLAGS)

clean:
	rm -f $(APP)



