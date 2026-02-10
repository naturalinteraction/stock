#!/usr/bin/env python3
"""MCP bridge for stock viewer.

Exposes a set-ticker tool over MCP (JSON-RPC over stdio) that forwards
requests to the already-running bin/stock REST server at localhost:8080.
"""

import json
import sys
import urllib.request
import urllib.error

REST_BASE = "http://localhost:8080"


def send(msg):
    out = json.dumps(msg)
    sys.stdout.write(out + "\n")
    sys.stdout.flush()


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

    else:
        text = f"Unknown tool: {name}"

    send({
        "jsonrpc": "2.0",
        "id": req["id"],
        "result": {
            "content": [{"type": "text", "text": text}],
        },
    })


def main():
    print('MCP bridge running...')
    handlers = {
        "initialize": handle_initialize,
        "notifications/initialized": lambda req: None,
        "tools/list": handle_tools_list,
        "tools/call": handle_tools_call,
    }

    for line in sys.stdin:
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


if __name__ == "__main__":
    main()
