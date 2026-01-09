APP = sample

# Use pkg-config to get correct flags for your DPDK installation
PKGCONF = $(shell pkg-config --cflags --libs libdpdk)

all: $(APP)

$(APP): sample.c
	$(CC) sample.c -o $(APP) $(PKGCONF)

clean:
	rm -f $(APP)

