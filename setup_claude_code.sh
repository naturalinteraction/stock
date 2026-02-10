#!/bin/bash

# Setup helper for Claude Code MCP integration
# This script automatically configures Claude Code to use the stock app

STOCK_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
LAUNCHER_PATH="$STOCK_DIR/mcp_launcher.py"
MCP_CONFIG_DIR="$HOME/.claude"
MCP_CONFIG_FILE="$MCP_CONFIG_DIR/mcp-servers.json"

echo "📋 Claude Code MCP Setup Helper"
echo "================================"
echo ""
echo "Stock app directory: $STOCK_DIR"
echo "Launcher script: $LAUNCHER_PATH"
echo "Config will be saved to: $MCP_CONFIG_FILE"
echo ""

# Check if app is built
if [ ! -f "$STOCK_DIR/bin/stock" ]; then
    echo "❌ Stock app not found. Please run 'make' first:"
    echo ""
    echo "  cd $STOCK_DIR"
    echo "  make"
    echo ""
    exit 1
fi

echo "✅ Stock app found at $STOCK_DIR/bin/stock"
echo ""

# Check if Python 3 is available
if ! command -v python3 &> /dev/null; then
    echo "❌ python3 not found. Please install Python 3:"
    echo ""
    echo "  Ubuntu/Debian: sudo apt install python3"
    echo "  macOS: brew install python3"
    echo ""
    exit 1
fi

echo "✅ Python 3 found: $(python3 --version)"
echo ""

# Create ~/.claude directory if needed
if [ ! -d "$MCP_CONFIG_DIR" ]; then
    echo "📁 Creating $MCP_CONFIG_DIR..."
    mkdir -p "$MCP_CONFIG_DIR"
fi

# Backup existing config if present
if [ -f "$MCP_CONFIG_FILE" ]; then
    echo "⚠️  Found existing MCP config"
    echo "    Backing up to $MCP_CONFIG_FILE.backup"
    cp "$MCP_CONFIG_FILE" "$MCP_CONFIG_FILE.backup"
fi

# Create the config with Python
echo "📝 Creating MCP server configuration..."

python3 << PYTHON_SCRIPT
import json
from pathlib import Path

config_file = Path("$MCP_CONFIG_FILE")
launcher_path = "$LAUNCHER_PATH"

# Load existing config or create new
if config_file.exists():
    with open(config_file) as f:
        config = json.load(f)
else:
    config = {}

# Ensure mcpServers key exists
if "mcpServers" not in config:
    config["mcpServers"] = {}

# Add/update stockChart entry
config["mcpServers"]["stockChart"] = {
    "command": "python3",
    "args": [launcher_path],
    "disabled": False,
    "environment": {
        "PYTHONUNBUFFERED": "1"
    }
}

# Write config
with open(config_file, "w") as f:
    json.dump(config, f, indent=2)

print(f"✅ Configuration saved to {config_file}")
PYTHON_SCRIPT

echo ""
echo "📋 Next steps:"
echo "1. Restart Claude Code completely (close and reopen)"
echo "2. Try asking: 'Show me Apple stock (AAPL)'"
echo "3. Watch the stock app window update!"
echo ""
echo "📚 For more details, see: $STOCK_DIR/CLAUDE_CODE_SETUP.md"
echo ""

# Show the config
echo "Current configuration:"
echo "─────────────────────"
python3 -m json.tool < "$MCP_CONFIG_FILE" 2>/dev/null || cat "$MCP_CONFIG_FILE"
echo ""
