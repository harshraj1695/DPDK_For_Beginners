CC=gcc

CFLAGS=-O3 -Iinclude $(shell pkg-config --cflags libdpdk)
LDFLAGS=$(shell pkg-config --libs libdpdk)

SRC=main.c \
nodes/rx_node.c \
nodes/parser_node.c \
nodes/firewall_node.c \
nodes/tx_node.c \
control/cli.c

OBJ=$(SRC:.c=.o)

all: graphfw

graphfw: $(OBJ)
	$(CC) -o graphfw $(OBJ) $(LDFLAGS)

clean:
	rm -f $(OBJ) graphfw