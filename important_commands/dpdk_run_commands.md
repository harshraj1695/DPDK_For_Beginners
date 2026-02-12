# DPDK Run and Debug Commands Guide

## Running DPDK Application with AF_PACKET Interface with huge page 

``` bash
sudo ./main --huge-dir=/dev/hugepages -l 0-3 --vdev=net_af_packet0,iface=eth0
```

------------------------------------------------------------------------

## Packet Capture using tcpdump

``` bash
sudo tcpdump -i eth0 -nn -vvv
```

------------------------------------------------------------------------

## Makefile Debug Configuration

``` makefile
CFLAGS += $(shell pkg-config --cflags libdpdk)
CFLAGS += -g -O0 -fno-omit-frame-pointer -Wall -Wextra -DDEBUG
LDFLAGS += $(shell pkg-config --libs libdpdk)
```

------------------------------------------------------------------------

## Running Application with GDB

``` bash
sudo gdb --args ./main --no-huge -l 0-3 --vdev=net_af_packet0,iface=eth0
```

------------------------------------------------------------------------

## Running setup_veth_bridge Script

### Make Script Executable

``` bash
chmod +x setup_veth_bridge.sh
```

### Run Script

``` bash
sudo ./setup_veth_bridge.sh
```

------------------------------------------------------------------------

## Running DPDK Application with Different Virtual Interfaces

### AF_PACKET Interface

``` bash
sudo ./your_dpdk_app --no-huge -l 0-1 --vdev=net_af_packet0,iface=eth0
```

### XDP Interface

``` bash
sudo ./your_dpdk_app --no-huge -l 0-1 --vdev=net_xdp0,iface=eth0
```

### TAP Interface

``` bash
sudo ./your_dpdk_app --no-huge -l 0-1 --vdev=net_tap0,iface=tap0
```

------------------------------------------------------------------------

## Running Event Device (Software Event Device)

### Using Service Corelist

``` bash
sudo ./main -l 0-2 --vdev=event_sw0 --service-corelist=1
```

### Using -S Flag

``` bash
sudo ./main -l 0-2 --vdev=event_sw0 -S 1
```

------------------------------------------------------------------------

## Running Multi-process DPDK Applications

### Primary Process

``` bash
sudo ./primary -l 0-1 --proc-type=primary
```

### Secondary Process

``` bash
sudo ./secondary -l 0-1 --proc-type=secondary
```

------------------------------------------------------------------------

## Running testpmd Application

### Basic Interactive Mode

``` bash
sudo ./build/app/dpdk-testpmd -l 0-3 -- -i
```

### testpmd with AF_PACKET Interface

``` bash
sudo ./build/app/dpdk-testpmd -l 0-3 --vdev=net_af_packet0,iface=eth0 -- -i
```
