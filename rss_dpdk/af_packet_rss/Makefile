APP = main
SRC = main.c

CC = gcc
CFLAGS = -O2 -g -march=native -Wall

DPDK_CFLAGS := $(shell pkg-config --cflags libdpdk 2>/dev/null)
DPDK_LIBS   := $(shell pkg-config --libs libdpdk 2>/dev/null)

ifneq ($(DPDK_LIBS),)
  CFLAGS += $(DPDK_CFLAGS)
  LIBS   = $(DPDK_LIBS)
else
  CFLAGS += -I/usr/local/include -include rte_config.h
  LDFLAGS = -L/usr/local/lib/x86_64-linux-gnu
  LIBS   = -lrte_eal -lrte_ethdev -lrte_mempool -lrte_mbuf -lrte_ring \
           -lrte_net -lrte_kvargs -lrte_bus_vdev
endif

all: $(APP)

$(APP): $(SRC)
	$(CC) $(SRC) -o $(APP) $(CFLAGS) $(LDFLAGS) $(LIBS)

clean:
	rm -f $(APP)

.PHONY: all clean
