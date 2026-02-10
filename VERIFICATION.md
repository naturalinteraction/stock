# Verification Checklist

## ✅ Setup Complete

Follow these steps to verify your Claude Code MCP integration is working:

### Step 1: Verify Installation

```bash
# Check that the launcher script exists and is executable
ls -lah ~/src/stock/mcp_launcher.py

# Check that the config was created
cat ~/.claude/mcp-servers.json

# Check that Python 3 is available
python3 --version

# Check that the stock app is built
~/src/stock/bin/stock --help  # (this will show SDL error, but that's OK)
```

### Step 2: Restart Claude Code

Close Claude Code completely and reopen it. This ensures it loads the new MCP configuration.

### Step 3: Test the Connection

In Claude Code, ask:

```
Show me Apple stock (AAPL)
```

**What should happen:**
1. Claude Code recognizes you want to show a ticker
2. Claude Code calls the `set_ticker` tool with `{"ticker":"AAPL"}`
3. The MCP launcher starts the stock app in the background
4. The stock app fetches Apple's data from Yahoo Finance
5. You see an SDL window pop up with Apple stock chart
6. Claude Code confirms the action succeeded

### Step 4: Try Another Ticker

Ask Claude:

```
Switch to Tesla (TSLA)
```

The chart should update to show Tesla stock.

### Step 5: Multiple Tickers

Ask Claude to cycle through some tickers:

```
Show me:
1. Microsoft (MSFT)
2. Nvidia (NVDA)
3. Your configured default ticker
```

Each time, the window should update with the new chart.

## Troubleshooting

### Claude Code doesn't recognize the tools

**Problem:** Claude doesn't have access to `set_ticker` tool

**Solution:**
1. Verify config exists: `cat ~/.claude/mcp-servers.json`
2. Ensure it has `"stockChart"` entry
3. Restart Claude Code (completely close it)
4. Check Claude Code's MCP settings/logs

### No SDL window appears

**Problem:** Tool runs but no chart displays

**Solution:**
1. Test the launcher manually:
   ```bash
   python3 ~/src/stock/mcp_launcher.py < /dev/null
   ```
2. This should hang (waiting for input). Press Ctrl+C.
3. If it crashes, check:
   - `~/src/stock/bin/stock` exists and is executable
   - SDL2 libraries are installed: `sdl2-config --version`
   - Try running directly: `~/src/stock/bin/stock`

### Network error fetching data

**Problem:** Chart shows "No data available"

**Solution:**
1. Check internet connection
2. Verify ticker symbol is valid (e.g., AAPL not APPLE)
3. Add exchange suffix if needed: VWCE.DE, TSLA.L, etc.
4. Yahoo Finance might be blocking requests (rare) - wait a few minutes and retry

### "Command not found" or "No such file"

**Problem:** launcher script or stock app not found

**Solution:**
1. Check paths are absolute, not relative
2. Rebuild: `cd ~/src/stock && make clean && make`
3. Run setup again: `~/src/stock/setup_claude_code.sh`

### Connection timeout

**Problem:** Claude Code hangs when trying to call set_ticker

**Solution:**
1. Check Python can run the launcher:
   ```bash
   timeout 5 python3 ~/src/stock/mcp_launcher.py < /dev/null || echo "OK (timed out as expected)"
   ```
2. Check stock app doesn't crash on startup:
   ```bash
   timeout 2 ~/src/stock/bin/stock || echo "OK"
   ```
3. Look for missing dependencies: `ldd ~/src/stock/bin/stock`

## Success Indicators

You know it's working when:

- ✅ Claude Code recognizes the `set_ticker` tool exists
- ✅ Asking for a ticker symbol makes an SDL window appear
- ✅ The window displays a stock chart (after a few seconds of data loading)
- ✅ Asking for a different ticker updates the chart in real-time
- ✅ You can ask multiple consecutive ticker requests without errors

## Next Steps

Once everything works:
- Try complex requests: "Show me a portfolio: AAPL, TSLA, MSFT"
- Ask Claude to analyze the charts
- Use it in your control room setup!

---

If you encounter issues, check the debug output:
```bash
# See what Claude Code's MCP server is doing
tail -f ~/.claude/mcp-servers.json

# Or run the launcher with debug output
python3 ~/src/stock/mcp_launcher.py 2>&1 | tee launcher.log
```
