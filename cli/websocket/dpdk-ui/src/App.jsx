import { useEffect, useState, useRef } from "react";

export default function App() {
  const [ws, setWs] = useState(null);
  const [input, setInput] = useState("");
  const [output, setOutput] = useState("");
  const terminalRef = useRef(null);

  useEffect(() => {
    const socket = new WebSocket("ws://localhost:3001");

    socket.onopen = () => {
      appendOutput("Connected to backend...\n");
    };

    socket.onmessage = (event) => {
      appendOutput(event.data);
    };

    socket.onerror = () => {
      appendOutput("\n[WebSocket error]\n");
    };

    socket.onclose = () => {
      appendOutput("\n[Connection closed]\n");
    };

    setWs(socket);

    return () => socket.close();
  }, []);

  const appendOutput = (text) => {
    setOutput((prev) => prev + text);
  };

  const sendCommand = () => {
    if (!ws || ws.readyState !== WebSocket.OPEN) return;

    ws.send(input + "\n");
    appendOutput(`> ${input}\n`);
    setInput("");
  };

  // auto-scroll
  useEffect(() => {
    if (terminalRef.current) {
      terminalRef.current.scrollTop =
        terminalRef.current.scrollHeight;
    }
  }, [output]);

  return (
    <div style={{ padding: 20, fontFamily: "monospace" }}>
      <h2>DPDK CLI Dashboard</h2>

      <div
        ref={terminalRef}
        style={{
          height: 400,
          overflowY: "auto",
          background: "black",
          color: "#00ff00",
          padding: 10,
          borderRadius: 5,
          whiteSpace: "pre-wrap",
        }}
      >
        {output}
      </div>

      <div style={{ marginTop: 10 }}>
        <input
          value={input}
          onChange={(e) => setInput(e.target.value)}
          onKeyDown={(e) => e.key === "Enter" && sendCommand()}
          placeholder="Enter command..."
          style={{
            width: "70%",
            padding: 8,
            marginRight: 10,
          }}
        />

        <button onClick={sendCommand}>Send</button>
      </div>
    </div>
  );
}