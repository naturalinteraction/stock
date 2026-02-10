#pragma once

#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

class RestServer {
public:
    RestServer();
    ~RestServer();
    
    void start(int port = 8080);
    void stop();
    
    // Thread-safe ticker management
    bool hasNewTickerRequest();
    std::string getRequestedTicker();
    void clearTickerRequest();
    
    // Get current tickers for validation
    void setAvailableTickers(const std::vector<std::string>& tickers);
    
private:
    void serverLoop();
    void handleRequest(int clientSocket, const std::string& method, const std::string& path, const std::string& body);
    void sendResponse(int clientSocket, int statusCode, const std::string& contentType, const std::string& body);
    
    std::atomic<bool> m_running;
    std::atomic<bool> m_newTickerRequest;
    std::string m_requestedTicker;
    std::mutex m_tickerMutex;
    
    std::vector<std::string> m_availableTickers;
    std::mutex m_tickersMutex;
    
    std::thread m_serverThread;
    int m_serverSocket;
    int m_port;
};