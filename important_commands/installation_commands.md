# DPDK Installation Guide

## STEP 1 --- Download DPDK

``` bash
cd ~/Downloads/
wget https://fast.dpdk.org/rel/dpdk-25.11.tar.xz
tar xJf dpdk-25.11.tar.xz
cd dpdk-25.11
```

------------------------------------------------------------------------

## STEP 2 --- Install Build Tools

``` bash
sudo apt update
sudo apt install -y meson ninja-build python3-pyelftools
```

------------------------------------------------------------------------

## STEP 3 --- Install Required Libraries

``` bash
sudo apt install -y     libnuma-dev     libfdt-dev     libbsd-dev     libjansson-dev     libpcap-dev     libarchive-dev
```

------------------------------------------------------------------------

## STEP 4 --- Configure Build with Meson

``` bash
meson setup build
```

------------------------------------------------------------------------

## STEP 5 --- Build DPDK

``` bash
cd build
ninja -j$(nproc)
```

------------------------------------------------------------------------

## STEP 6 --- Install DPDK System-wide

``` bash
sudo ninja install
sudo ldconfig
```

------------------------------------------------------------------------

## STEP 7 --- Setup pkg-config Path

``` bash
echo 'export PKG_CONFIG_PATH=/usr/local/lib/pkgconfig' >> ~/.bashrc
source ~/.bashrc
```

------------------------------------------------------------------------

## STEP 8 --- Verify DPDK Installation

``` bash
pkg-config --libs libdpdk
pkg-config --cflags libdpdk
```

------------------------------------------------------------------------

## STEP 9 --- Run a Test DPDK Application

``` bash
cd ~/dpdk/hello_world/
cc sample.c -o sample $(pkg-config --cflags --libs libdpdk)
sudo ./sample --no-huge -m 512
```
