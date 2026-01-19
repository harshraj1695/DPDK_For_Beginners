#!/bin/bash
set -e

echo "========================================"
echo "   Hyper-V Raw TX Enable Script"
echo "   (veth ↔ bridge ↔ eth0 architecture)"
echo "========================================"

############################################
# 1. USER CONFIG (CHANGE IF NEEDED)
############################################

# --- Change ONLY if your interface is NOT eth0 ---
NIC="eth0"              # your real Hyper-V NIC

# --- Change ONLY if your IP/Gateway is different ---
VM_IP="172.23.52.244"   # your VM's current IP
VM_MASK="20"            # your subnet mask (/20)
VM_GW="172.23.48.1"     # your gateway

# --- veth interface names ---
VETH0="veth0"
VETH1="veth1"

# --- Linux bridge name ---
BR="br0"

############################################
echo "[1] Creating veth pair: $VETH0 <-> $VETH1"
############################################
sudo ip link add $VETH0 type veth peer name $VETH1 || true

############################################
echo "[2] Creating Linux bridge: $BR"
############################################
sudo ip link add name $BR type bridge || true

############################################
echo "[3] Enabling IP forwarding"
############################################
sudo sysctl -w net.ipv4.ip_forward=1

############################################
echo "[4] Adding $NIC and $VETH0 to bridge"
############################################
sudo ip link set $NIC master $BR
sudo ip link set $VETH0 master $BR

############################################
echo "[5] Bringing interfaces UP"
############################################
sudo ip link set $BR up
sudo ip link set $NIC up
sudo ip link set $VETH0 up
sudo ip link set $VETH1 up

############################################
echo "[6] Moving IP from $NIC → $BR"
echo "    (This may temporarily interrupt network)"
############################################

# Remove IP from NIC
sudo ip addr flush dev $NIC

# Assign IP to bridge
sudo ip addr add $VM_IP/$VM_MASK dev $BR

# Restore default route
sudo ip route add default via $VM_GW dev $BR

############################################
echo "[7] Final state verification"
############################################
echo "--- ip addr show $BR ---"
ip addr show $BR

echo "--- bridge link ---"
bridge link

############################################
echo "========================================"
echo " Setup complete!"
echo " You can now transmit via: $VETH1"
echo ""
echo " Example: DPDK TX that now works:"
echo "   sudo ./simple --no-huge -l 0-1 --vdev=net_af_packet0,iface=$VETH1"
echo ""
echo "To watch packets on real NIC:"
echo "   sudo tcpdump -i $NIC -nn"
echo "========================================"
