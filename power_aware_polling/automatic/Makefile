APP = power_autodemo
SRC = main.c

CFLAGS += -O3 -Wall
CFLAGS += $(shell pkg-config --cflags libdpdk)

LDFLAGS += $(shell pkg-config --libs libdpdk)

all:
	gcc $(SRC) $(CFLAGS) -o $(APP) $(LDFLAGS)

clean:
	rm -f $(APP)