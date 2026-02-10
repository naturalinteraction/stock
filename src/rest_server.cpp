#include "rest_server.h"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <SDL2/SDL.h>

RestServer::RestServer() 
    : m_running(false), m_newTickerRequest(false), m_serverSocket(-1), m_port(8080) {
}

RestServer::~RestServer() {
    stop();
}

void RestServer::start(int port) {
    if (m_running.load()) {
        std::cerr << "REST server already running\n";
        return;
    }
    
    m_port = port;
    m_running.store(true);
    
    m_serverThread = std::thread(&RestServer::serverLoop, this);
    std::cout << "REST server started on port " << port << "\n";
}

void RestServer::stop() {
    if (!m_running.load()) {
        return;
    }
    
    m_running.store(false);
    
    if (m_serverSocket >= 0) {
        shutdown(m_serverSocket, SHUT_RDWR);
        close(m_serverSocket);
        m_serverSocket = -1;
    }
    
    if (m_serverThread.joinable()) {
        m_serverThread.join();
    }
    
    std::cout << "REST server stopped\n";
}

void RestServer::serverLoop() {
    // Create socket
    m_serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (m_serverSocket < 0) {
        std::cerr << "Failed to create socket\n";
        return;
    }
    
    // Set socket options to reuse address
    int opt = 1;
    if (setsockopt(m_serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        std::cerr << "Failed to set socket options\n";
        close(m_serverSocket);
        return;
    }
    
    // Bind to address
    struct sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(m_port);
    
    if (bind(m_serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        std::cerr << "Failed to bind to port " << m_port << "\n";
        close(m_serverSocket);
        return;
    }
    
    // Listen for connections
    if (listen(m_serverSocket, 5) < 0) {
        std::cerr << "Failed to listen on port " << m_port << "\n";
        close(m_serverSocket);
        return;
    }
    
    while (m_running.load()) {
        struct sockaddr_in clientAddr;
        socklen_t clientLen = sizeof(clientAddr);
        
        // Accept connection with timeout
        fd_set readSet;
        FD_ZERO(&readSet);
        FD_SET(m_serverSocket, &readSet);
        
        struct timeval timeout;
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        
        int selectResult = select(m_serverSocket + 1, &readSet, nullptr, nullptr, &timeout);
        if (selectResult <= 0) {
            continue; // Timeout or error, continue loop
        }
        
        int clientSocket = accept(m_serverSocket, (struct sockaddr*)&clientAddr, &clientLen);
        if (clientSocket < 0) {
            if (m_running.load()) {
                std::cerr << "Failed to accept connection\n";
            }
            continue;
        }
        
        // Read request - may need multiple recv() calls to get full body
        std::string request;
        char buffer[4096];
        int bytesReceived = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
        if (bytesReceived > 0) {
            buffer[bytesReceived] = '\0';
            request.append(buffer, bytesReceived);

            // Check if we need to read more (Content-Length vs body received)
            size_t headerEnd = request.find("\r\n\r\n");
            if (headerEnd != std::string::npos) {
                size_t bodyReceived = request.size() - (headerEnd + 4);
                // Parse Content-Length from headers
                size_t contentLength = 0;
                size_t clPos = request.find("Content-Length:");
                if (clPos == std::string::npos)
                    clPos = request.find("content-length:");
                if (clPos != std::string::npos) {
                    contentLength = std::stoul(request.substr(clPos + 15));
                }
                // Keep reading until we have the full body
                while (bodyReceived < contentLength) {
                    bytesReceived = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
                    if (bytesReceived <= 0) break;
                    request.append(buffer, bytesReceived);
                    bodyReceived += bytesReceived;
                }
            }

            // Parse HTTP request line
            std::istringstream iss(request);
            std::string method, path, version;
            iss >> method >> path >> version;

            // Find body if any
            std::string body;
            if (headerEnd != std::string::npos) {
                body = request.substr(headerEnd + 4);
            }

            handleRequest(clientSocket, method, path, body);
        }
        
        close(clientSocket);
    }
}

void RestServer::handleRequest(int clientSocket, const std::string& method, const std::string& path, const std::string& body) {
    std::cout << "REST: " << method << " " << path << "\n";
    
    int statusCode = 200;
    std::string contentType = "application/json";
    std::string responseBody;
    
    if (method == "POST" && path == "/set-ticker") {
        // Parse JSON body to extract ticker
        std::string ticker;
        
        // Simple JSON parsing for "ticker" field
        size_t tickerPos = body.find("\"ticker\"");
        if (tickerPos != std::string::npos) {
            size_t colonPos = body.find(":", tickerPos);
            if (colonPos != std::string::npos) {
                size_t startQuote = body.find("\"", colonPos);
                if (startQuote != std::string::npos) {
                    size_t endQuote = body.find("\"", startQuote + 1);
                    if (endQuote != std::string::npos) {
                        ticker = body.substr(startQuote + 1, endQuote - startQuote - 1);
                    }
                }
            }
        }
        
        if (ticker.empty()) {
            statusCode = 400;
            responseBody = "{\"error\":\"Missing ticker field\"}";
        }
        else {
            // Validate ticker against available tickers
            {
                std::lock_guard<std::mutex> lock(m_tickersMutex);
                auto it = std::find(m_availableTickers.begin(), m_availableTickers.end(), ticker);
                if (it == m_availableTickers.end()) {
                    statusCode = 404;
                    responseBody = "{\"error\":\"Ticker not found\"}";
                }
            }
            
            if (statusCode == 200) {
                // Set the requested ticker
                {
                    std::lock_guard<std::mutex> lock(m_tickerMutex);
                    m_requestedTicker = ticker;
                    m_newTickerRequest.store(true);
                }

                // Push an SDL event to wake the main loop from SDL_WaitEvent
                SDL_Event wakeEvent;
                wakeEvent.type = SDL_USEREVENT;
                SDL_PushEvent(&wakeEvent);

                responseBody = "{\"status\":\"success\",\"ticker\":\"" + ticker + "\"}";
            }
        }
    }
    else if (method == "GET" && path == "/tickers") {
        std::lock_guard<std::mutex> lock(m_tickersMutex);
        std::string json = "{\"tickers\":[";
        for (size_t i = 0; i < m_availableTickers.size(); ++i) {
            if (i > 0) json += ",";
            json += "\"" + m_availableTickers[i] + "\"";
        }
        json += "]}";
        responseBody = json;
    }
    else if (method == "GET" && path == "/") {
        responseBody = "{\"service\":\"Stock Chart REST API\",\"endpoints\":[\"POST /set-ticker\",\"GET /tickers\"]}";
    }
    else {
        statusCode = 404;
        responseBody = "{\"error\":\"Endpoint not found\"}";
    }
    
    sendResponse(clientSocket, statusCode, contentType, responseBody);
}

void RestServer::sendResponse(int clientSocket, int statusCode, const std::string& contentType, const std::string& body) {
    std::string statusText;
    switch (statusCode) {
        case 200: statusText = "OK"; break;
        case 400: statusText = "Bad Request"; break;
        case 404: statusText = "Not Found"; break;
        default: statusText = "Unknown"; break;
    }
    
    // Build HTTP response
    std::ostringstream response;
    response << "HTTP/1.1 " << statusCode << " " << statusText << "\r\n";
    response << "Content-Type: " << contentType << "\r\n";
    response << "Content-Length: " << body.length() << "\r\n";
    response << "Access-Control-Allow-Origin: *\r\n";
    response << "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n";
    response << "Access-Control-Allow-Headers: Content-Type\r\n";
    response << "\r\n";
    response << body;
    
    // Send response to client
    std::string responseStr = response.str();
    send(clientSocket, responseStr.c_str(), responseStr.length(), 0);
    
    std::cout << "REST Response: " << statusCode << " " << statusText << " sent to client\n";
}

bool RestServer::hasNewTickerRequest() {
    return m_newTickerRequest.load();
}

std::string RestServer::getRequestedTicker() {
    std::lock_guard<std::mutex> lock(m_tickerMutex);
    return m_requestedTicker;
}

void RestServer::clearTickerRequest() {
    std::lock_guard<std::mutex> lock(m_tickerMutex);
    m_newTickerRequest.store(false);
    m_requestedTicker.clear();
}

void RestServer::setAvailableTickers(const std::vector<std::string>& tickers) {
    std::lock_guard<std::mutex> lock(m_tickersMutex);
    m_availableTickers = tickers;
}