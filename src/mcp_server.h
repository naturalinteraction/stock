#pragma once

#include <string>
#include <functional>
#include <queue>
#include <mutex>
#include <thread>

// Simple command structure for MCP requests
struct MCPCommand {
    std::string tool;      // e.g., "set_ticker"
    std::string ticker;    // e.g., "AAPL"
    int days = -1;         // e.g., 60, or -1 if not specified
};

// Thread-safe command queue
class MCPCommandQueue {
public:
    void push(const MCPCommand& cmd) {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push(cmd);
    }

    bool tryPop(MCPCommand& cmd) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.empty()) return false;
        cmd = queue_.front();
        queue_.pop();
        return true;
    }

private:
    std::queue<MCPCommand> queue_;
    std::mutex mutex_;
};

// Start MCP server on a background thread
// This function reads JSON-RPC messages from stdin and writes responses to stdout
void startMCPServer(MCPCommandQueue& cmdQueue);
