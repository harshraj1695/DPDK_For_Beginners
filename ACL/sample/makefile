APP = main
SRC = main.c

CC = gcc

CFLAGS = -O2 -g -march=native -Wall

# Get DPDK flags automatically
DPDK_CFLAGS := $(shell pkg-config --cflags libdpdk)
DPDK_LIBS   := $(shell pkg-config --libs libdpdk)

all: $(APP)

$(APP): $(SRC)
	$(CC) $(SRC) -o $(APP) $(CFLAGS) $(DPDK_CFLAGS) $(DPDK_LIBS)

clean:
	rm -f $(APP)
