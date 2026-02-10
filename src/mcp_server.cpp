#include "mcp_server.h"

#include <iostream>
#include <sstream>
#include <cstring>
#include <cstdlib>

// Simple JSON helper functions for manual parsing/building
static std::string jsonString(const std::string& s) {
    std::string result = "\"";
    for (char c : s) {
        if (c == '"') result += "\\\"";
        else if (c == '\\') result += "\\\\";
        else if (c == '\n') result += "\\n";
        else if (c == '\r') result += "\\r";
        else if (c == '\t') result += "\\t";
        else result += c;
    }
    result += "\"";
    return result;
}

// Extract string value from JSON field
// Very basic - assumes format: "fieldname":"value"
static std::string getJsonString(const std::string& json, const std::string& field) {
    std::string needle = "\"" + field + "\":";
    size_t pos = json.find(needle);
    if (pos == std::string::npos) return "";

    pos += needle.length();
    // Skip whitespace
    while (pos < json.length() && (json[pos] == ' ' || json[pos] == '\t')) pos++;

    if (pos >= json.length() || json[pos] != '"') return "";
    pos++; // skip opening quote

    std::string result;
    while (pos < json.length() && json[pos] != '"') {
        if (json[pos] == '\\' && pos + 1 < json.length()) {
            pos++;
            if (json[pos] == 'n') result += '\n';
            else if (json[pos] == 't') result += '\t';
            else if (json[pos] == 'r') result += '\r';
            else if (json[pos] == '"') result += '"';
            else if (json[pos] == '\\') result += '\\';
            else result += json[pos];
        } else {
            result += json[pos];
        }
        pos++;
    }
    return result;
}

// Extract numeric value from JSON field
static int getJsonInt(const std::string& json, const std::string& field) {
    std::string needle = "\"" + field + "\":";
    size_t pos = json.find(needle);
    if (pos == std::string::npos) return -1;

    pos += needle.length();
    while (pos < json.length() && (json[pos] == ' ' || json[pos] == '\t')) pos++;

    std::string numStr;
    while (pos < json.length() && (std::isdigit(json[pos]) || json[pos] == '-')) {
        numStr += json[pos];
        pos++;
    }

    if (numStr.empty()) return -1;
    return std::stoi(numStr);
}

static void sendResponse(int id, const std::string& result) {
    std::string response = "{\"jsonrpc\":\"2.0\",\"id\":" + std::to_string(id) + ",\"result\":" + result + "}";
    std::cout << response << "\n";
    std::cout.flush();
}

static void sendError(int id, const std::string& message) {
    std::string response = "{\"jsonrpc\":\"2.0\",\"id\":" + std::to_string(id)
                         + ",\"error\":{\"code\":-1,\"message\":" + jsonString(message) + "}}";
    std::cout << response << "\n";
    std::cout.flush();
}

static void sendInitializeResponse(int id) {
    std::string response =
        "{\"jsonrpc\":\"2.0\",\"id\":" + std::to_string(id) +
        ",\"result\":{\"protocolVersion\":\"2024-11-05\",\"capabilities\":{\"tools\":{}},\"serverInfo\":{\"name\":\"StockChart\",\"version\":\"1.0\"}}}";
    std::cout << response << "\n";
    std::cout.flush();
}

static void sendToolsList(int id) {
    std::string response =
        "{\"jsonrpc\":\"2.0\",\"id\":" + std::to_string(id) +
        ",\"result\":{\"tools\":[{\"name\":\"set_ticker\",\"description\":\"Change the displayed stock ticker\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"ticker\":{\"type\":\"string\",\"description\":\"Stock ticker symbol (e.g., AAPL, VWCE.DE)\"},\"days\":{\"type\":\"integer\",\"description\":\"Number of days to display (optional, default 30)\"}},\"required\":[\"ticker\"]}}]}}";
    std::cout << response << "\n";
    std::cout.flush();
}

void startMCPServer(MCPCommandQueue& cmdQueue) {
    // Run MCP protocol on background thread
    std::thread mcpThread([&cmdQueue]() {
        std::cerr << "[MCP] Server started, waiting for messages on stdin\n";
        std::string line;

        while (std::getline(std::cin, line)) {
            if (line.empty()) continue;

            // Parse JSON-RPC message
            int id = getJsonInt(line, "id");
            if (id < 0) id = -1; // might be a notification

            std::string method = getJsonString(line, "method");

            if (method == "initialize") {
                sendInitializeResponse(id);
            }
            else if (method == "tools/list") {
                sendToolsList(id);
            }
            else if (method == "tools/call") {
                std::string toolName = getJsonString(line, "name");

                if (toolName == "set_ticker") {
                    // Extract ticker and optional days from arguments
                    // Arguments are in: "arguments":{"ticker":"AAPL","days":90}

                    // Find the arguments object
                    size_t argPos = line.find("\"arguments\":");
                    std::string argsJson = "";
                    if (argPos != std::string::npos) {
                        argPos += 12; // length of "arguments":
                        size_t startBrace = line.find('{', argPos);
                        if (startBrace != std::string::npos) {
                            int braceDepth = 0;
                            for (size_t i = startBrace; i < line.length(); i++) {
                                if (line[i] == '{') braceDepth++;
                                else if (line[i] == '}') braceDepth--;
                                argsJson += line[i];
                                if (braceDepth == 0) break;
                            }
                        }
                    }

                    std::string ticker = getJsonString(argsJson, "ticker");
                    int days = getJsonInt(argsJson, "days");

                    if (ticker.empty()) {
                        sendError(id, "Missing required parameter: ticker");
                    } else {
                        MCPCommand cmd;
                        cmd.tool = "set_ticker";
                        cmd.ticker = ticker;
                        cmd.days = days; // -1 if not provided
                        cmdQueue.push(cmd);

                        std::string result = "{\"success\":true,\"message\":\"Ticker changed to " + ticker + "\"}";
                        sendResponse(id, result);

                        std::cerr << "[MCP] set_ticker: " << ticker;
                        if (days > 0) std::cerr << " (" << days << " days)";
                        std::cerr << "\n";
                    }
                }
                else {
                    sendError(id, "Unknown tool: " + toolName);
                }
            }
            else if (!method.empty()) {
                sendError(id, "Unknown method: " + method);
            }
        }

        std::cerr << "[MCP] stdin closed, server shutting down\n";
    });

    // Detach thread so it runs in background
    mcpThread.detach();
}
