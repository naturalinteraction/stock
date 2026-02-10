# MCP Server Integration - Prototype

This document describes the prototype MCP (Model Context Protocol) server integrated directly into the stock application.

## Overview

The stock app now includes a built-in MCP server that allows Claude to control which ticker is displayed in real-time via standardized MCP protocol messages.

### Architecture

```
Claude Code / claude.ai
    ↓ (JSON-RPC over stdio)
Stock App (with embedded MCP server)
    ↓ (thread-safe command queue)
Main Render Loop
    ↓ (updates display)
SDL Window (always running in control room)
```

## Files Added/Modified

### New Files
- `src/mcp_server.h` - Header with MCP protocol handler and command queue
- `src/mcp_server.cpp` - Implementation of MCP protocol (JSON-RPC 2.0 over stdin/stdout)

### Modified Files
- `src/main.cpp` - Integration points:
  - Added MCP server include
  - Created global `g_mcpCommandQueue`
  - Started MCP server thread after SDL init
  - Added MCP command processing in event loop
- `Makefile` - Added `src/mcp_server.cpp` to build

## How It Works

### 1. MCP Protocol (JSON-RPC 2.0)

The MCP server listens on stdin for JSON-RPC 2.0 messages and sends responses to stdout. This is the standard MCP transport.

### 2. Supported Methods

#### `initialize`
Initializes the MCP connection. Required first.

**Request:**
```json
{"jsonrpc":"2.0","id":1,"method":"initialize","params":{}}
```

**Response:**
```json
{"jsonrpc":"2.0","id":1,"result":{"protocolVersion":"2024-11-05","capabilities":{"tools":{}},"serverInfo":{"name":"StockChart","version":"1.0"}}}
```

#### `tools/list`
Lists available tools that Claude can invoke.

**Request:**
```json
{"jsonrpc":"2.0","id":2,"method":"tools/list"}
```

**Response:**
```json
{
  "jsonrpc":"2.0",
  "id":2,
  "result":{
    "tools":[
      {
        "name":"set_ticker",
        "description":"Change the displayed stock ticker",
        "inputSchema":{
          "type":"object",
          "properties":{
            "ticker":{"type":"string","description":"Stock ticker symbol (e.g., AAPL, VWCE.DE)"},
            "days":{"type":"integer","description":"Number of days to display (optional)"}
          },
          "required":["ticker"]
        }
      }
    ]
  }
}
```

#### `tools/call`
Calls a tool with arguments.

**Request (set_ticker):**
```json
{"jsonrpc":"2.0","id":3,"method":"tools/call","params":{"name":"set_ticker","arguments":{"ticker":"AAPL"}}}
```

**Response:**
```json
{"jsonrpc":"2.0","id":3,"result":{"success":true,"message":"Ticker changed to AAPL"}}
```

### 3. Thread Safety

The `MCPCommandQueue` is a thread-safe queue that:
- **MCP thread** (background): Parses JSON-RPC messages and queues commands
- **Main thread** (render loop): Polls the queue and processes commands

Commands are processed each frame before SDL event handling.

### 4. Command Processing

When Claude sends `set_ticker AAPL`:
1. MCP server receives JSON-RPC message
2. Creates `MCPCommand` struct and pushes to thread-safe queue
3. Main loop (next frame) pops the command
4. Ticker is changed to AAPL (if in config)
5. New data is fetched from Yahoo Finance
6. Chart re-renders with new ticker

## Testing

Run the test script to see it in action:
```bash
chmod +x test_mcp.sh
./test_mcp.sh
```

This will:
1. Start the stock app
2. Send MCP messages to initialize, list tools, and switch to AAPL
3. Display the chart with AAPL

## Future Enhancements

### 1. Additional Tools
- `get_data` - Return raw price data as JSON (for agents that need to analyze without rendering)
- `set_days` - Change display duration
- `get_current_ticker` - Query current state

### 2. WebSocket Transport
For production, replace stdin/stdout with WebSocket for better compatibility with remote connections and persistent sessions.

### 3. Robust JSON Parsing
Currently uses simple string parsing. For production:
- Integrate nlohmann/json library (header-only, single include)
- Use proper JSON parsing/generation

### 4. Error Handling
- Currently errors are basic. Could add retry logic for network failures
- Add validation for ticker symbols before fetching

### 5. Configuration
- Allow MCP server to query/modify app settings
- Persist ticker history via MCP

## Current Limitations

1. **Tickers must be pre-configured** - Claude can only switch to tickers in the `config.json` list. Adding new tickers at runtime would require additional tools.

2. **Simple JSON parser** - Handles common cases but isn't robust. For production, use a proper JSON library.

3. **Single app instance** - App must be running; MCP doesn't start it. Claude needs the app already displayed in the control room.

4. **Stdio-based** - Works for local control. For remote scenarios, upgrade to WebSocket.

## Integration with Claude Code / claude.ai

### Option 1: Local Control
Run the app locally with:
```bash
./bin/stock &
```

Then configure Claude Code's MCP to connect to the app's stdio. Update settings to register the app as an MCP server.

### Option 2: Remote Control (Future)
When upgraded to WebSocket, Claude can connect to:
```
ws://control-room-ip:9000
```

And control the display from anywhere.

## Example Claude Conversation

**User:** "Show me Apple stock"

**Claude:** [Calls MCP tool `set_ticker` with `{"ticker":"AAPL"}`]

*Stock app chart updates in real-time to show AAPL*

**User:** "Now show Tesla"

**Claude:** [Calls MCP tool `set_ticker` with `{"ticker":"TSLA"}`]

*Stock app chart updates to show TSLA*

---

This is a working prototype. Effort for production hardening: 2-3 additional hours.
