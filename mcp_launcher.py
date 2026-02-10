#!/usr/bin/env python3
"""
MCP Launcher for Stock App

This script acts as an MCP server for Claude Code. It:
1. Launches the stock application
2. Bridges MCP communication from Claude Code to the app's stdin/stdout
3. Manages the app subprocess lifecycle

Usage:
    python3 mcp_launcher.py

Claude Code Configuration (in ~/.claude/mcp-servers.json):
    {
      "mcpServers": {
        "stockChart": {
          "command": "python3",
          "args": ["/path/to/stock/mcp_launcher.py"],
          "disabled": false
        }
      }
    }
"""

import subprocess
import sys
import json
import os
import signal
import threading
from pathlib import Path

class StockAppMCPBridge:
    def __init__(self):
        # Start the stock app as subprocess
        script_dir = Path(__file__).parent
        app_path = script_dir / "bin" / "stock"

        if not app_path.exists():
            raise RuntimeError(f"Stock app not found at {app_path}. Did you run 'make'?")

        try:
            self.process = subprocess.Popen(
                [str(app_path)],
                stdin=subprocess.PIPE,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
                bufsize=1  # Line buffered
            )
            print(f"[MCP] Started stock app (PID {self.process.pid})", file=sys.stderr)
        except Exception as e:
            raise RuntimeError(f"Failed to start stock app: {e}")

        # Thread to forward stderr from app
        self.stderr_thread = threading.Thread(target=self._forward_stderr, daemon=True)
        self.stderr_thread.start()

    def _forward_stderr(self):
        """Forward app stderr to our stderr for debugging"""
        try:
            for line in self.process.stderr:
                print(f"[APP] {line.rstrip()}", file=sys.stderr)
        except:
            pass

    def send_message(self, message: str):
        """Send message to stock app"""
        try:
            self.process.stdin.write(message + "\n")
            self.process.stdin.flush()
        except BrokenPipeError:
            raise RuntimeError("Stock app disconnected")
        except Exception as e:
            raise RuntimeError(f"Failed to send message: {e}")

    def read_message(self) -> str:
        """Read response from stock app"""
        try:
            line = self.process.stdout.readline()
            if not line:
                raise RuntimeError("Stock app closed connection")
            return line.rstrip()
        except Exception as e:
            raise RuntimeError(f"Failed to read message: {e}")

    def close(self):
        """Clean shutdown"""
        try:
            self.process.stdin.close()
        except:
            pass
        try:
            self.process.terminate()
            self.process.wait(timeout=5)
        except:
            self.process.kill()


def main():
    """Main MCP server loop"""

    try:
        bridge = StockAppMCPBridge()
    except RuntimeError as e:
        print(json.dumps({
            "jsonrpc": "2.0",
            "error": {"code": -32603, "message": str(e)}
        }))
        sys.exit(1)

    try:
        # Read MCP messages from Claude Code and forward to app
        for line in sys.stdin:
            line = line.rstrip()
            if not line:
                continue

            try:
                # Forward message to app
                bridge.send_message(line)

                # Read response from app and send to Claude Code
                response = bridge.read_message()
                print(response)
                sys.stdout.flush()

            except RuntimeError as e:
                # Send error response
                try:
                    msg = json.loads(line)
                    error_response = {
                        "jsonrpc": "2.0",
                        "id": msg.get("id", -1),
                        "error": {"code": -32603, "message": str(e)}
                    }
                except:
                    error_response = {
                        "jsonrpc": "2.0",
                        "error": {"code": -32603, "message": str(e)}
                    }
                print(json.dumps(error_response))
                sys.stdout.flush()

    except KeyboardInterrupt:
        print("[MCP] Interrupted by user", file=sys.stderr)
    except Exception as e:
        print(f"[MCP] Unexpected error: {e}", file=sys.stderr)
    finally:
        bridge.close()
        print("[MCP] Bridge closed", file=sys.stderr)


if __name__ == "__main__":
    main()
