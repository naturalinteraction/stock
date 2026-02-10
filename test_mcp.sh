#!/bin/bash

# Test script for MCP server
# This demonstrates how Claude can send commands to the stock app

echo "Starting stock app with MCP server..."
(
    # MCP Protocol requests sent to the stock app via stdin

    # Step 1: Initialize
    echo '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{}}'

    # Wait a moment for initialization
    sleep 1

    # Step 2: List available tools
    echo '{"jsonrpc":"2.0","id":2,"method":"tools/list"}'

    # Wait a moment
    sleep 1

    # Step 3: Call set_ticker tool with VHYL.AS
    echo '{"jsonrpc":"2.0","id":3,"method":"tools/call","params":{"name":"set_ticker","arguments":{"ticker":"VHYL.AS"}}}'

    # Keep stdin open so app doesn't exit
    sleep 15
) | ./bin/stock
