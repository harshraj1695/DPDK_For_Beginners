APP = main
SRCS = main.c

CFLAGS += $(shell pkg-config --cflags libdpdk)
LDFLAGS += $(shell pkg-config --libs libdpdk)

all:
	$(CC) $(SRCS) -o $(APP) $(CFLAGS) $(LDFLAGS)
