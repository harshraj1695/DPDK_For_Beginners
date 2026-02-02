import socket
import json

SOCKET_PATH = "/var/run/dpdk/rte/dpdk_telemetry.v2"

cmd = {
    "action": "get",
    "command": "/harsh_stats"
}

# Create UNIX socket (IMPORTANT: SOCK_SEQPACKET)
sock = socket.socket(socket.AF_UNIX, socket.SOCK_SEQPACKET)
sock.connect(SOCKET_PATH)

# Receive handshake
hello = sock.recv(4096)
print("Handshake:", hello.decode())

# Serialize JSON + newline (VERY IMPORTANT)
msg = json.dumps(cmd) + "\n"
sock.sendall(msg.encode())

# Receive response
resp = sock.recv(16384)
print("Response:", resp.decode())

sock.close()
