#!/usr/bin/env python3
"""MCP bridge for stock viewer.

Exposes tools over MCP (JSON-RPC over stdio) that:
- Forward ticker changes to bin/stock REST server at localhost:8080
- Accept messages/prompts from the stock app via REST endpoint on localhost:9090
"""

import json
import sys
import urllib.request
import urllib.error
import threading
import select
from http.server import HTTPServer, BaseHTTPRequestHandler
from queue import Queue

REST_BASE = "http://localhost:8080"
MCP_BRIDGE_PORT = 9090

# Queue for messages from the stock app
app_messages = Queue()


def send(msg):
    out = json.dumps(msg)
    sys.stdout.write(out + "\n")
    sys.stdout.flush()


class AppMessageHandler(BaseHTTPRequestHandler):
    """HTTP request handler for messages from the stock app."""

    def do_POST(self):
        if self.path == "/prompt":
            content_length = int(self.headers.get("Content-Length", 0))
            body = self.rfile.read(content_length)
            try:
                data = json.loads(body.decode())
                message = data.get("message", "")
                if message:
                    app_messages.put({"type": "prompt", "message": message})
                    self.send_response(200)
                    self.send_header("Content-Type", "application/json")
                    self.end_headers()
                    self.wfile.write(json.dumps({"status": "received"}).encode())
                else:
                    self.send_response(400)
                    self.send_header("Content-Type", "application/json")
                    self.end_headers()
                    self.wfile.write(json.dumps({"error": "message field required"}).encode())
            except Exception as e:
                self.send_response(500)
                self.send_header("Content-Type", "application/json")
                self.end_headers()
                self.wfile.write(json.dumps({"error": str(e)}).encode())
        else:
            self.send_response(404)
            self.end_headers()

    def log_message(self, format, *args):
        # Suppress HTTP server logs
        pass


def rest_post(path, body):
    req = urllib.request.Request(
        f"{REST_BASE}{path}",
        data=json.dumps(body).encode(),
        headers={"Content-Type": "application/json"},
        method="POST",
    )
    try:
        with urllib.request.urlopen(req, timeout=15) as resp:
            return json.loads(resp.read().decode())
    except urllib.error.URLError as e:
        return {"error": f"Cannot reach stock server: {e.reason}"}
    except Exception as e:
        return {"error": str(e)}


def rest_get(path):
    try:
        with urllib.request.urlopen(f"{REST_BASE}{path}", timeout=15) as resp:
            return json.loads(resp.read().decode())
    except urllib.error.URLError as e:
        return {"error": f"Cannot reach stock server: {e.reason}"}
    except Exception as e:
        return {"error": str(e)}


def handle_initialize(req):
    send({
        "jsonrpc": "2.0",
        "id": req["id"],
        "result": {
            "protocolVersion": "2024-11-05",
            "capabilities": {"tools": {}},
            "serverInfo": {"name": "stock-mcp-bridge", "version": "1.0.0"},
        },
    })


def handle_tools_list(req):
    send({
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
                {
                    "name": "get-app-messages",
                    "description": "Retrieve pending messages/prompts from the stock application. The stock app can send prompts to Claude via REST calls to the MCP bridge.",
                    "inputSchema": {"type": "object", "properties": {}},
                },
            ]
        },
    })


def handle_tools_call(req):
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

    elif name == "get-app-messages":
        messages = []
        while not app_messages.empty():
            try:
                messages.append(app_messages.get_nowait())
            except:
                break
        if messages:
            text = json.dumps(messages, indent=2)
        else:
            text = "No messages from the app"

    else:
        text = f"Unknown tool: {name}"

    send({
        "jsonrpc": "2.0",
        "id": req["id"],
        "result": {
            "content": [{"type": "text", "text": text}],
        },
    })


def start_http_server():
    """Start the HTTP server that listens for messages from the stock app."""
    server = HTTPServer(("localhost", MCP_BRIDGE_PORT), AppMessageHandler)
    print(f'HTTP server listening on port {MCP_BRIDGE_PORT}...', file=sys.stderr)
    server.serve_forever()


def main():
    print('MCP bridge running...', file=sys.stderr)

    # Start HTTP server in a background thread
    http_thread = threading.Thread(target=start_http_server, daemon=True)
    http_thread.start()

    handlers = {
        "initialize": handle_initialize,
        "notifications/initialized": lambda req: None,
        "tools/list": handle_tools_list,
        "tools/call": handle_tools_call,
    }

    # Use select to handle stdin in a non-blocking way
    while True:
        # Check if stdin has data available (with 0.1 second timeout)
        ready, _, _ = select.select([sys.stdin], [], [], 0.1)

        if ready:
            try:
                line = sys.stdin.readline()
                if not line:  # EOF
                    continue
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
                    handler(req)
                elif "id" in req:
                    send({
                        "jsonrpc": "2.0",
                        "id": req["id"],
                        "error": {"code": -32601, "message": f"Method not found: {method}"},
                    })
            except EOFError:
                # Continue running even if stdin closes
                continue
            except Exception as e:
                print(f"Error processing input: {e}", file=sys.stderr)


if __name__ == "__main__":
    main()
