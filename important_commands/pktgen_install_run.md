# DPDK pktgen-dpdk Installation and Usage Guide

##  Prerequisites

Install required packages:

``` bash
sudo apt update
sudo apt install -y build-essential meson ninja-build libnuma-dev git python3-pyelftools
```

------------------------------------------------------------------------

##  Clone pktgen-dpdk

``` bash
git clone https://github.com/pktgen/Pktgen-DPDK.git
cd Pktgen-DPDK
```


## Install libbsd dev package:
``` bash
sudo apt install libbsd-dev
```


------------------------------------------------------------------------


##  Build pktgen

``` bash
meson setup build
ninja -C build
```

After successful build, binary will be located at:

``` bash
build/app/pktgen
```

------------------------------------------------------------------------

##  Configure Hugepages

Check current hugepages:

``` bash
cat /proc/meminfo | grep Huge
```

Allocate hugepages if required:

``` bash
echo 1024 | sudo tee /proc/sys/vm/nr_hugepages
```

------------------------------------------------------------------------

##  Bind NIC to DPDK Driver

Check NIC devices:

``` bash
dpdk-devbind.py -s
```

Bind NIC to vfio-pci:

``` bash
sudo dpdk-devbind.py --bind=vfio-pci <PCI-ADDRESS>
```

------------------------------------------------------------------------

##  Run pktgen

Run using full binary path:

``` bash
sudo ./build/app/pktgen -l 0-3 -n 4 -- -P -m "[1:2].0"
```

------------------------------------------------------------------------

##  Explanation of Run Command

### EAL Flags

-   `-l 0-3` → CPU cores used
-   `-n 4` → Memory channels

### pktgen Flags

-   `-P` → Enable promiscuous mode
-   `-m "[1:2].0"` → Core-to-port mapping

------------------------------------------------------------------------

##  pktgen Interactive Commands

### Start Traffic

``` bash
start all
```

### Stop Traffic

``` bash
stop all
```

### Show Statistics

``` bash
stats
```

### Set Packet Size

``` bash
set 0 size 64
```

### Set Transmission Rate

``` bash
set 0 rate 100
```

### Set Destination MAC

``` bash
set 0 dst mac XX:XX:XX:XX:XX:XX
```

### Set Destination IP

``` bash
set 0 dst ip 192.168.1.2
```

------------------------------------------------------------------------

## Performance Tuning Options

### TX Descriptor Size

``` bash
--txd=2048
```

### RX Descriptor Size

``` bash
--rxd=2048
```

### Burst Size

``` bash
--burst=<value>
```

------------------------------------------------------------------------

## Useful Debug Commands

Check pktgen binary:

``` bash
ls build/app/
```

Check NIC Binding:

``` bash
dpdk-devbind.py -s
```

Check Hugepages:

``` bash
cat /proc/meminfo | grep Huge
```

------------------------------------------------------------------------

##  Optional: Add pktgen to PATH

``` bash
sudo ln -s ~/Pktgen-DPDK/build/app/pktgen /usr/local/bin/pktgen
```

Now you can run:

``` bash
sudo pktgen ...
```

------------------------------------------------------------------------

## Example Full Workflow

``` bash
sudo ./build/app/pktgen -l 0-3 -n 4 -- -P -m "[1:2].0"

# Inside pktgen console:
set 0 dst mac XX:XX:XX:XX:XX:XX
set 0 size 64
set 0 rate 100
start 0
```

------------------------------------------------------------------------

## Notes

-   Ensure NIC supports DPDK drivers.
-   Always configure hugepages before running pktgen.
-   Use correct core-to-port mapping for best performance.
