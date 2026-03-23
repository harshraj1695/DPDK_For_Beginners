const net = require('net');
const WebSocket = require('ws');

const wss = new WebSocket.Server({ port: 3001 });

console.log("WebSocket server running on ws://localhost:3001");

wss.on('connection', (ws) => {
    console.log("Frontend connected");

    let client = null;

    function connectDPDK() {
        client = new net.Socket();

        client.connect(9090, '127.0.0.1', () => {
            console.log("Connected to DPDK");
            ws.send("Connected to DPDK\n");
        });

        client.on('data', (data) => {
            ws.send(data.toString());
        });

        client.on('close', () => {
            console.log("DPDK disconnected, retrying...");
            ws.send("DPDK disconnected, retrying...\n");

            setTimeout(connectDPDK, 1000); // 🔥 reconnect
        });

        client.on('error', (err) => {
            console.log("DPDK error:", err.message);
        });
    }

    connectDPDK();

    ws.on('message', (msg) => {
        if (client && client.readyState === 'open') {
            client.write(msg.toString() + '\n');
        } else {
            ws.send("DPDK not connected\n");
        }
    });

    ws.on('close', () => {
        console.log("Frontend disconnected");
        if (client) client.destroy();
    });
});