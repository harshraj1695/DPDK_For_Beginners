APP = append_prependops

# Use pkg-config to get correct flags for your DPDK installation
PKGCONF = $(shell pkg-config --cflags --libs libdpdk)

all: $(APP)

$(APP): append_prependops.c
	$(CC) append_prependops.c -o $(APP) $(PKGCONF)

clean:
	rm -f $(APP)

