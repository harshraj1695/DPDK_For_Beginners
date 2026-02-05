/ for binding the nic 
sudo ./usertools/dpdk-devbind.py -b vfio-pci --noiommu 0000:07:00.0

 sudo ./usertools/dpdk-devbind.py --status



 Load VFIO modules**

```bash
sudo modprobe vfio-pci
sudo modprobe vfio_iommu_type1
```

* Ensures VFIO driver is loaded to bind NICs for DPDK.
* Only needs to be done **once per boot**.

---

## **2️⃣ Check and bind your NIC to VFIO**

```bash
# Check current driver
sudo ./dpdk/usertools/dpdk-devbind.py --status

# Bind the DPDK NIC (e.g., 07:00.0) to VFIO
sudo ./dpdk/usertools/dpdk-devbind.py -b vfio-pci 0000:07:00.0
```

* Make sure you **do not touch your SSH NIC** (`enp1s0`).
* This step **rebinds the NIC after reboot**.

---

## **3️⃣ Mount hugepages**

```bash
# Create mount point
sudo mkdir -p /mnt/huge

# Mount hugepages
sudo mount -t hugetlbfs nodev /mnt/huge

# Reserve 1024 hugepages
echo 1024 | sudo tee /sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages
```

* DPDK requires hugepages for memory buffers.
* You may automate this with `/etc/fstab` for persistence:

```
nodev /mnt/huge hugetlbfs defaults 0 0
```

* And write `1024` to `/sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages` on boot.

---

## **4️⃣ Run your DPDK program**

```bash
sudo ./main -l 0-1 -n 1 -- -p 0
```

* `-l 0-1` → CPU cores
* `-n 1` → memory channel
* `-p 0` → DPDK port (the VFIO NIC you bound)

> Make sure your program uses the correct **IP/MAC addresses** matching your virtual network.

---

## **Optional: Capture traffic**

If you want to verify:

```bash
sudo tcpdump -i enp1s0 host 10.0.0.1
```

Or use **dpdk-pdump** for user-space capture:

```bash
sudo ./dpdk/build/dpdk-pdump -l 0-1 -n 1 -- --pdump "port=0,queue=0,rx-dev=/tmp/trace.pcap"
```

* Open `/tmp/trace.pcap` in Wireshark to see the packets.

---


