APP = main

SRCS = main.c 

CFLAGS += -O3 -I.
CFLAGS += $(shell pkg-config --cflags libdpdk)
CFLAGS += -g -O0 -fno-omit-frame-pointer -Wall -Wextra -DDEBUG
LDFLAGS += $(shell pkg-config --libs libdpdk)
CFLAGS += -O0 -g


all:
	$(CC) $(SRCS) -o $(APP) $(CFLAGS) $(LDFLAGS)

clean:
	rm -f $(APP)
