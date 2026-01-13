APP = main

# Use pkg-config to get correct flags for your DPDK installation
PKGCONF = $(shell pkg-config --cflags --libs libdpdk)

all: $(APP)

$(APP): main.c
	$(CC) main.c -o $(APP) $(PKGCONF)

clean:
	rm -f $(APP)

