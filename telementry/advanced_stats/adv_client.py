#!/usr/bin/env python3
import socket
import sys

SOCKET_PATH = "/var/run/dpdk/rte/dpdk_telemetry.v2"

# Command from argv, default to /adv/summary
cmd = sys.argv[1] if len(sys.argv) > 1 else "/adv/summary"
print(f"Sending command: '{cmd}'")

try:
    sock = socket.socket(socket.AF_UNIX, socket.SOCK_SEQPACKET)
    sock.connect(SOCKET_PATH)
except Exception as e:
    print(f"Connection failed: {e}")
    sys.exit(1)

try:
    hello = sock.recv(4096)
    print("Handshake:", hello.decode())
except Exception as e:
    print(f"Handshake receive failed: {e}")
    sock.close()
    sys.exit(1)

try:
    sent = sock.send(cmd.encode())
    print(f"Sent {sent} bytes: {repr(cmd)}")
except Exception as e:
    print(f"Send failed: {e}")
    sock.close()
    sys.exit(1)

try:
    resp = sock.recv(16384)
    print("Response:", resp.decode())
except Exception as e:
    print(f"Receive failed: {e}")

sock.close()

