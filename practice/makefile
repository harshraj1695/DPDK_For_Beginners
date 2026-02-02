CC = gcc
CFLAGS = $(shell pkg-config --cflags libdpdk)
LDFLAGS = $(shell pkg-config --libs libdpdk)

all:
	$(CC) ring.c -o simple_ring $(CFLAGS) $(LDFLAGS)
