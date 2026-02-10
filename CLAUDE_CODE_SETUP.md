# Claude Code MCP Integration Setup

This guide explains how to configure Claude Code to control the stock app via MCP.

## Quick Setup (3 steps)

### 1. Get the Full Path to Your Stock Directory

```bash
cd /home/av/src/stock
pwd
# Output: /home/av/src/stock
```

Save this path. You'll need it in step 2.

### 2. Edit Claude Code's MCP Configuration

Claude Code stores MCP server configurations in `~/.claude/mcp-servers.json`

If the file doesn't exist, create it:

```bash
mkdir -p ~/.claude
```

Then edit `~/.claude/mcp-servers.json` and add the stock app server:

```json
{
  "mcpServers": {
    "stockChart": {
      "command": "python3",
      "args": ["/home/av/src/stock/mcp_launcher.py"],
      "disabled": false,
      "environment": {
        "PYTHONUNBUFFERED": "1"
      }
    }
  }
}
```

**Important:** Replace `/home/av/src/stock` with your actual stock directory path from step 1.

### 3. Restart Claude Code

Close and reopen Claude Code (or use `/help` to reload if supported).

## How It Works

```
Claude Code                          Your Terminal
   ↓                                    ↓
MCP Client          Python Bridge      Stock App
   │                    │                │
   └──→ "set_ticker" ──→ mcp_launcher.py
                         │
                         └──→ ./bin/stock (running in background)
                              ├─ Fetches data from Yahoo Finance
                              └─ Updates SDL window
   │←─── response ─────────────────────┘
   │
Display result to user
```

## Testing the Connection

### Option A: Ask Claude Code Directly

In Claude Code, ask:
```
Show me Apple stock (AAPL)
```

Claude Code will:
1. Call the `set_ticker` tool with `{"ticker":"AAPL"}`
2. mcp_launcher.py forwards this to the running stock app
3. Stock app fetches data and updates display
4. Claude Code reports success

### Option B: Manual Testing

Terminal 1 - Build and verify app works:
```bash
cd /home/av/src/stock
make
./bin/stock  # Test locally, press ESC to exit
```

Terminal 2 - Test the launcher directly:
```bash
python3 ~/src/stock/mcp_launcher.py < test_mcp.sh
```

This simulates Claude Code sending MCP commands.

## Configuration Details

### What mcp_launcher.py Does

1. **Launches** the stock app as a subprocess
2. **Bridges** stdin/stdout between Claude Code and the app
3. **Forwards** stderr from app to your terminal (for debugging)
4. **Manages** subprocess lifecycle (cleanup on exit)

### Environment Variables

- `PYTHONUNBUFFERED=1` - Ensures line-buffered output (needed for real-time MCP)

### Error Handling

If Claude Code gets an error when calling `set_ticker`:
- Check that `make` was run and `bin/stock` exists
- Check file permissions: `ls -la /home/av/src/stock/bin/stock`
- Look for error messages in Claude Code's debug output

## Troubleshooting

### "Command not found: python3"

Ensure Python 3 is installed:
```bash
python3 --version
# or try: python --version
```

If using `python` instead of `python3`, update the mcp-servers.json:
```json
"command": "python",
```

### "Stock app not found at bin/stock"

The launcher couldn't find the compiled app. Make sure you ran:
```bash
cd /home/av/src/stock
make
```

### "No such file or directory" for mcp_launcher.py

The path in mcp-servers.json is wrong. Use the full absolute path from step 1.

### Connection hangs or times out

The stock app might be crashing. Test it directly:
```bash
./bin/stock
# If it exits immediately, something is wrong
# Check for missing fonts or SDL libraries
```

### Claude Code says "Unknown tool: set_ticker"

The MCP initialization didn't work. Check:
1. Python script can run: `python3 mcp_launcher.py --help` should not error
2. Rebuild: `make clean && make`
3. Restart Claude Code completely

## Advanced: Multiple Stock Apps

If you want multiple instances (e.g., one for technical analysis, one for price tracking):

Create a copy of mcp_launcher.py for each:
```bash
cp mcp_launcher.py mcp_launcher_ta.py
cp mcp_launcher.py mcp_launcher_price.py
```

Register both in mcp-servers.json:
```json
{
  "mcpServers": {
    "stockChartTA": {
      "command": "python3",
      "args": ["/home/av/src/stock/mcp_launcher.py"]
    },
    "stockChartPrice": {
      "command": "python3",
      "args": ["/home/av/src/stock/mcp_launcher_price.py"]
    }
  }
}
```

Now Claude can control two separate stock app windows independently!

## Uninstalling

To disable the MCP server, either:

**Option 1:** Set `disabled: true` in mcp-servers.json
```json
"disabled": true,
```

**Option 2:** Remove the entire `stockChart` entry from mcpServers

**Option 3:** Delete ~/.claude/mcp-servers.json (removes all MCP servers)

## Next Steps

1. ✅ Configure mcp-servers.json (done above)
2. ✅ Restart Claude Code (done)
3. Try asking Claude: "Show me Tesla stock (TSLA)"
4. Watch the stock app window update in real-time!

---

**Need help?** Check `MCP_PROTOTYPE.md` for protocol details or run `make clean && make` to rebuild everything.
