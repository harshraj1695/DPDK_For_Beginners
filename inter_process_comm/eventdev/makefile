CC = gcc

CFLAGS += $(shell pkg-config --cflags libdpdk)
LDFLAGS += $(shell pkg-config --libs libdpdk)

all:
	$(CC) primary.c -o primary $(CFLAGS) $(LDFLAGS)
	$(CC) secondary.c -o secondary $(CFLAGS) $(LDFLAGS)
