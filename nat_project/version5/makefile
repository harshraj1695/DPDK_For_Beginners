APP := main
SRCS := main.c mempool.c port.c hash.c static_nat.c config.c
OBJS := $(SRCS:.c=.o)
DEPS := $(OBJS:.o=.d)

CC ?= gcc
PKGCONF ?= pkg-config
DPDK_PKG ?= libdpdk

CFLAGS += -O3 -std=gnu11 -Wall -Wextra -Wno-unused-result
CFLAGS += -MMD -MP
CFLAGS += $(shell $(PKGCONF) --cflags $(DPDK_PKG))

LDFLAGS += $(shell $(PKGCONF) --libs $(DPDK_PKG))

.PHONY: all clean

all: $(APP)

$(APP): $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

-include $(DEPS)

clean:
	rm -f $(APP) $(OBJS) $(DEPS)
