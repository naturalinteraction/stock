#!/usr/bin/env python3
"""MCP bridge for stock viewer.

Exposes a set-ticker tool over MCP (JSON-RPC over TCP/IP) that forwards
requests to the already-running bin/stock REST server at localhost:8080.
"""

import json
import socket
import urllib.request
import urllib.error

REST_BASE = "http://localhost:8080"
MCP_PORT = 8081


def send(sock, msg):
    out = json.dumps(msg)
    sock.send((out + "\n").encode())


def rest_post(path, body):
    req = urllib.request.Request(
        f"{REST_BASE}{path}",
        data=json.dumps(body).encode(),
        headers={"Content-Type": "application/json"},
        method="POST",
    )
    try:
        with urllib.request.urlopen(req, timeout=5) as resp:
            return json.loads(resp.read().decode())
    except urllib.error.URLError as e:
        return {"error": f"Cannot reach stock server: {e.reason}"}
    except Exception as e:
        return {"error": str(e)}


def rest_get(path):
    try:
        with urllib.request.urlopen(f"{REST_BASE}{path}", timeout=5) as resp:
            return json.loads(resp.read().decode())
    except urllib.error.URLError as e:
        return {"error": f"Cannot reach stock server: {e.reason}"}
    except Exception as e:
        return {"error": str(e)}


def handle_initialize(req, sock):
    send(sock, {
        "jsonrpc": "2.0",
        "id": req["id"],
        "result": {
            "protocolVersion": "2024-11-05",
            "capabilities": {"tools": {}},
            "serverInfo": {"name": "stock-mcp-bridge", "version": "1.0.0"},
        },
    })


def handle_tools_list(req, sock):
    send(sock, {
        "jsonrpc": "2.0",
        "id": req["id"],
        "result": {
            "tools": [
                {
                    "name": "set-ticker",
                    "description": "Change the stock ticker displayed in the chart viewer. The stock application (bin/stock) must already be running.",
                    "inputSchema": {
                        "type": "object",
                        "properties": {
                            "ticker": {
                                "type": "string",
                                "description": "Stock ticker symbol, e.g. AAPL, VWCE.DE, MSFT",
                            }
                        },
                        "required": ["ticker"],
                    },
                },
                {
                    "name": "get-tickers",
                    "description": "List available/known tickers from the stock viewer.",
                    "inputSchema": {"type": "object", "properties": {}},
                },
            ]
        },
    })


def handle_tools_call(req, sock):
    name = req["params"]["name"]
    args = req["params"].get("arguments", {})

    if name == "set-ticker":
        ticker = args.get("ticker", "")
        if not ticker:
            text = "Error: ticker is required"
        else:
            result = rest_post("/set-ticker", {"ticker": ticker})
            if "error" in result:
                text = f"Error: {result['error']}"
            else:
                text = f"Ticker set to {ticker}"
                if "message" in result:
                    text = result["message"]

    elif name == "get-tickers":
        result = rest_get("/tickers")
        if "error" in result:
            text = f"Error: {result['error']}"
        else:
            text = json.dumps(result, indent=2)

    else:
        text = f"Unknown tool: {name}"

    send(sock, {
        "jsonrpc": "2.0",
        "id": req["id"],
        "result": {
            "content": [{"type": "text", "text": text}],
        },
    })


def main():
    server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    server_socket.bind(('127.0.0.1', MCP_PORT))
    server_socket.listen(1)
    print(f'MCP bridge listening on port {MCP_PORT}...')

    handlers = {
        "initialize": handle_initialize,
        "notifications/initialized": lambda req, sock: None,
        "tools/list": handle_tools_list,
        "tools/call": handle_tools_call,
    }

    try:
        while True:
            conn, addr = server_socket.accept()
            print(f'Client connected from {addr}')
            buffer = ""

            try:
                while True:
                    data = conn.recv(4096).decode()
                    if not data:
                        break

                    buffer += data
                    lines = buffer.split('\n')
                    buffer = lines[-1]  # Keep incomplete line in buffer

                    for line in lines[:-1]:
                        line = line.strip()
                        if not line:
                            continue
                        try:
                            req = json.loads(line)
                        except json.JSONDecodeError:
                            continue

                        method = req.get("method", "")
                        handler = handlers.get(method)
                        if handler:
                            handler(req, conn)
                        elif "id" in req:
                            send(conn, {
                                "jsonrpc": "2.0",
                                "id": req["id"],
                                "error": {"code": -32601, "message": f"Method not found: {method}"},
                            })
            finally:
                conn.close()
    except KeyboardInterrupt:
        print('Shutting down...')
    finally:
        server_socket.close()


if __name__ == "__main__":
    main()
