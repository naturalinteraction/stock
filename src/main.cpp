/*
 * Stock - ETF Price Chart Viewer
 *
 * Usage: ./stock
 *
 * Dependencies: SDL2, SDL2_ttf, libcurl
 *   Ubuntu/Debian: sudo apt install libsdl2-dev libsdl2-ttf-dev libcurl4-openssl-dev
 */

#include "chart.h"
#include "viewmode_bollinger.h"
#include "viewmode_macross.h"
#include "viewmode_stats.h"
#include "config.h"
#include "rest_server.h"
#include "yahoo_finance.h"
#include "draw.h"
#include "ui.h"
#include "render_chart.h"

#include <curl/curl.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

// ─── Defaults ───
constexpr int DEFAULT_DAYS = 90;
constexpr int DISPLAYED_DAYS_STEP = 5;
constexpr int FETCH_DATA_COUNT = 90;

// --- Current ticker state ---
static int g_currentTickerIndex = 0; // Index of the currently displayed ticker

// ═══════════════════════  SDL Initialization & Cleanup  ═══════════════════════

struct SDLResources {
    SDL_Window* window;
    SDL_Renderer* renderer;
    TTF_Font* fontLarge;
    TTF_Font* fontSmall;
    WindowDimensions winDim;
};

SDLResources initSDL(const Config& appConfig, bool& fullscreen) {
    // ── SDL init ──
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::cerr << "SDL_Init: " << SDL_GetError() << "\n";
        return {nullptr, nullptr, nullptr, nullptr, {}};
    }
    if (TTF_Init() != 0) {
        std::cerr << "TTF_Init: " << TTF_GetError() << "\n";
        SDL_Quit();
        return {nullptr, nullptr, nullptr, nullptr, {}};
    }

    std::string fontPath = findFont();
    if (fontPath.empty()) {
        std::cerr << "No TrueType font found. Install dejavu or liberation fonts.\n";
        TTF_Quit();
        SDL_Quit();
        return {nullptr, nullptr, nullptr, nullptr, {}};
    }

    TTF_Font* font   = TTF_OpenFont(fontPath.c_str(), 18);
    TTF_Font* fontSm = TTF_OpenFont(fontPath.c_str(), 13);
    if (!font || !fontSm) {
        std::cerr << "Failed to load font: " << TTF_GetError() << "\n";
        if (font) TTF_CloseFont(font);
        if (fontSm) TTF_CloseFont(fontSm);
        TTF_Quit();
        SDL_Quit();
        return {nullptr, nullptr, nullptr, nullptr, {}};
    }

    Uint32 windowFlags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN; // Always start hidden and resizable

    SDL_Window* win = SDL_CreateWindow(
        ("Stock - " + appConfig.tickers[g_currentTickerIndex]).c_str(),
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        DEFAULT_GRAPH_WIDTH, DEFAULT_GRAPH_HEIGHT,
        windowFlags);
    if (!win) {
        std::cerr << "SDL_CreateWindow: " << SDL_GetError() << "\n";
        TTF_CloseFont(font);
        TTF_CloseFont(fontSm);
        TTF_Quit();
        SDL_Quit();
        return {nullptr, nullptr, nullptr, nullptr, {}};
    }

    // Set fullscreen mode explicitly if enabled in config
    if (fullscreen) {
        SDL_SetWindowFullscreen(win, SDL_WINDOW_FULLSCREEN);
    }
    // Set minimum size unconditionally
    SDL_SetWindowMinimumSize(win, MIN_GRAPH_WIDTH, MIN_GRAPH_HEIGHT);

    // Get actual window size (may differ from initial size in fullscreen)
    int winWidth, winHeight;
    SDL_GetWindowSize(win, &winWidth, &winHeight);
    WindowDimensions winDim = WindowDimensions::calculate(winWidth, winHeight);

    SDL_Renderer* ren = SDL_CreateRenderer(win, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!ren) {
        std::cerr << "SDL_CreateRenderer: " << SDL_GetError() << "\n";
        SDL_DestroyWindow(win);
        TTF_CloseFont(font);
        TTF_CloseFont(fontSm);
        TTF_Quit();
        SDL_Quit();
        return {nullptr, nullptr, nullptr, nullptr, {}};
    }

    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);

    // Show window after all setup and initial render is complete
    SDL_ShowWindow(win);

    // Apply fullscreen mode after showing the window if configured
    if (fullscreen) {
        SDL_SetWindowFullscreen(win, SDL_WINDOW_FULLSCREEN);
        // Update winDim after setting fullscreen to get actual dimensions
        int newWidth, newHeight;
        SDL_GetWindowSize(win, &newWidth, &newHeight);
        winDim = WindowDimensions::calculate(newWidth, newHeight);
    }

    return {win, ren, font, fontSm, winDim};
}

void cleanup(SDLResources& resources, RestServer& restServer, Config& appConfig) {
    restServer.stop();
    SDL_DestroyRenderer(resources.renderer);
    SDL_DestroyWindow(resources.window);
    TTF_CloseFont(resources.fontSmall);
    TTF_CloseFont(resources.fontLarge);
    TTF_Quit();
    SDL_Quit();
    saveConfig(appConfig);
    curl_global_cleanup();
}

void initREST(RestServer& restServer, const Config& appConfig) {
    restServer.setAvailableTickers(appConfig.tickers);
    restServer.start(8080);
}

// ═══════════════════════  Loading & Initialization  ═══════════════════════

struct LoadedData {
    Config config;
    ViewMode viewMode;
    bool fullscreen;
    std::vector<PricePoint> price_history;
};

LoadedData load() {
    LoadedData data{};

    // Load configuration
    data.config = loadConfig();
    g_currentTickerIndex = data.config.lastActiveTickerIndex;
    // Ensure g_currentTickerIndex is within valid bounds
    if (static_cast<size_t>(g_currentTickerIndex) >= data.config.tickers.size()) {
        g_currentTickerIndex = 0;
    }

    data.viewMode = data.config.viewMode;
    data.fullscreen = data.config.fullscreen;

    // Fetch price data
    int fetchDays = FETCH_DATA_COUNT + LOOKBACK_DAYS;
    std::cout << "Fetching " << FETCH_DATA_COUNT << " trading days for " << data.config.tickers[g_currentTickerIndex]
              << " (+" << LOOKBACK_DAYS << " lookback) ...\n";

    curl_global_init(CURL_GLOBAL_DEFAULT);
    std::string json = fetchJSON(data.config.tickers[g_currentTickerIndex], fetchDays);

    if (json.empty()) {
        curl_global_cleanup();
        std::cerr << "Failed to fetch data. Check network and ticker symbol.\n";
        return data;
    }

    auto price_history = parseResponse(json, fetchDays);
    if (price_history.empty()) {
        std::cerr << "No trading data found for " << data.config.tickers[g_currentTickerIndex] << "\n";
        curl_global_cleanup();
        return data;
    }

    std::cout << "Loaded " << price_history.size() << " trading days  ("
              << price_history.front().date << "  ->  " << price_history.back().date << ")\n";

    data.price_history = std::move(price_history);
    return data;
}

// ═══════════════════════  main  ═══════════════════════

int main(int, char* []) {
    // Load configuration and price data
    LoadedData data = load();
    if (data.price_history.empty()) {
        return 1;
    }

    Config& appConfig = data.config;
    ViewMode viewMode = data.viewMode;
    bool FULLSCREEN = data.fullscreen;
    auto& price_history = data.price_history;

    // Initialize REST server
    RestServer restServer;
    initREST(restServer, appConfig);

    // Initialize SDL and create window/renderer
    SDLResources resources = initSDL(appConfig, FULLSCREEN);
    if (!resources.window || !resources.renderer) {
        curl_global_cleanup();
        return 1;
    }

    SDL_Window* win = resources.window;
    SDL_Renderer* ren = resources.renderer;
    TTF_Font* font = resources.fontLarge;
    TTF_Font* fontSm = resources.fontSmall;
    WindowDimensions winDim = resources.winDim;

    // ── Event loop ──
    bool running = true;
    bool mouseMoved = false; // Flag to track if mouse moved
    while (running) {
        // Check for REST server ticker updates
        if (restServer.hasNewTickerRequest()) {
            std::string newTicker = restServer.getRequestedTicker();
            restServer.clearTickerRequest();
            
            // Find ticker index
            auto it = std::find(appConfig.tickers.begin(), appConfig.tickers.end(), newTicker);
            if (it != appConfig.tickers.end()) {
                g_currentTickerIndex = static_cast<int>(std::distance(appConfig.tickers.begin(), it));
                appConfig.lastActiveTickerIndex = g_currentTickerIndex;
                saveConfig(appConfig);
                
                std::cout << "REST API: Switching to ticker: " << appConfig.tickers[g_currentTickerIndex] << " ...\n";
                
                // Fetch new data
                int fetchDays = FETCH_DATA_COUNT + LOOKBACK_DAYS;
                curl_global_init(CURL_GLOBAL_DEFAULT);
                std::string json = fetchJSON(appConfig.tickers[g_currentTickerIndex], fetchDays);
                
                if (!json.empty()) {
                    auto fresh = parseResponse(json, fetchDays);
                    if (!fresh.empty()) {
                        price_history = std::move(fresh);
                        std::cout << "Loaded " << price_history.size()
                                  << " trading days  ("
                                  << price_history.front().date << "  ->  "
                                  << price_history.back().date << ")\n";
                    } else {
                        std::cerr << "No trading data found for " << appConfig.tickers[g_currentTickerIndex] << "\n";
                    }
                } else {
                    std::cerr << "Failed to fetch data for " << appConfig.tickers[g_currentTickerIndex] << ". Check network and ticker symbol.\n";
                }
                curl_global_cleanup();
                
                // Update window title
                SDL_SetWindowTitle(win, ("Stock - " + appConfig.tickers[g_currentTickerIndex]).c_str());
                
                // Render immediately
                renderChart(ren, font, fontSm, price_history, appConfig.tickers, g_currentTickerIndex, viewMode, appConfig.displayedDays, winDim);
                SDL_RenderPresent(ren);
            }
        }
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) { // Process all events in the queue
            switch (ev.type) {
            case SDL_QUIT:
                running = false;
                break;
            case SDL_KEYDOWN:
                if (ev.key.keysym.sym == SDLK_ESCAPE)
                    running = false;
                else if (ev.key.keysym.sym == SDLK_TAB) {
                    viewMode = static_cast<ViewMode>(
                        (static_cast<int>(viewMode) + 1) % VIEW_MODE_COUNT);
                    appConfig.viewMode = viewMode;
                    saveConfig(appConfig);
                    // Rerender immediately for TAB key press
                    renderChart(ren, font, fontSm, price_history, appConfig.tickers, g_currentTickerIndex, viewMode, appConfig.displayedDays, winDim);
                    SDL_RenderPresent(ren);
                }
                else if (ev.key.keysym.sym == SDLK_SPACE) {
                    g_currentTickerIndex = (g_currentTickerIndex + 1) % appConfig.tickers.size();
                    appConfig.lastActiveTickerIndex = g_currentTickerIndex;
                    saveConfig(appConfig);
                    std::cout << "Switching to ticker: " << appConfig.tickers[g_currentTickerIndex] << " ...\n";

                    int fetchDays = FETCH_DATA_COUNT + LOOKBACK_DAYS;
                    // Re-initialize curl for each fetch to prevent issues with persistent handles
                    curl_global_init(CURL_GLOBAL_DEFAULT);
                    std::string json = fetchJSON(appConfig.tickers[g_currentTickerIndex], fetchDays);

                    if (!json.empty()) {
                        auto fresh = parseResponse(json, fetchDays);
                        if (!fresh.empty()) {
                            price_history = std::move(fresh);
                            std::cout << "Loaded " << price_history.size()
                                      << " trading days  ("
                                      << price_history.front().date << "  ->  "
                                      << price_history.back().date << ")\n";
                        } else {
                            std::cerr << "No trading data found for " << appConfig.tickers[g_currentTickerIndex] << "\n";
                        }
                    } else {
                        std::cerr << "Failed to fetch data for " << appConfig.tickers[g_currentTickerIndex] << ". Check network and ticker symbol.\n";
                    }
                    curl_global_cleanup();

                    renderChart(ren, font, fontSm, price_history, appConfig.tickers, g_currentTickerIndex, viewMode, appConfig.displayedDays, winDim);
                    SDL_RenderPresent(ren);
                }
                else if (ev.key.keysym.sym == SDLK_r) {
                    std::cout << "Reloading " << appConfig.tickers[0] << " ...\n";
                    int fetchDays = FETCH_DATA_COUNT + LOOKBACK_DAYS;
                    std::string rj = fetchJSON(appConfig.tickers[g_currentTickerIndex], fetchDays);
                    if (!rj.empty()) {
                        auto fresh = parseResponse(rj, fetchDays);
                        if (!fresh.empty()) {
                            price_history = std::move(fresh);
                            std::cout << "Loaded " << price_history.size()
                                      << " trading days  ("
                                      << price_history.front().date << "  ->  "
                                      << price_history.back().date << ")\n";
                        }
                    }
                    // Rerender immediately for R key press
                    renderChart(ren, font, fontSm, price_history, appConfig.tickers, g_currentTickerIndex, viewMode, appConfig.displayedDays, winDim);
                    SDL_RenderPresent(ren);
                }
                else if (ev.key.keysym.sym == SDLK_f) {
                    FULLSCREEN = !FULLSCREEN;
                    appConfig.fullscreen = FULLSCREEN;
                    saveConfig(appConfig);

                    if (FULLSCREEN) {
                        SDL_SetWindowFullscreen(win, SDL_WINDOW_FULLSCREEN);
                    } else {
                        SDL_SetWindowFullscreen(win, 0); // Go back to windowed mode
                    }

                    // Get new window size after fullscreen toggle
                    int newWidth, newHeight;
                    SDL_GetWindowSize(win, &newWidth, &newHeight);
                    winDim = WindowDimensions::calculate(newWidth, newHeight);

                    renderChart(ren, font, fontSm, price_history, appConfig.tickers, g_currentTickerIndex, viewMode, appConfig.displayedDays, winDim);
                    SDL_RenderPresent(ren);
                }
                else if (ev.key.keysym.sym == SDLK_UP) {
                    appConfig.displayedDays = std::max(10, appConfig.displayedDays - DISPLAYED_DAYS_STEP);
                    saveConfig(appConfig);
                    renderChart(ren, font, fontSm, price_history, appConfig.tickers, g_currentTickerIndex, viewMode, appConfig.displayedDays, winDim);
                    SDL_RenderPresent(ren);
                }
                else if (ev.key.keysym.sym == SDLK_DOWN) {
                    appConfig.displayedDays = std::min(90, appConfig.displayedDays + DISPLAYED_DAYS_STEP);
                    saveConfig(appConfig);
                    renderChart(ren, font, fontSm, price_history, appConfig.tickers, g_currentTickerIndex, viewMode, appConfig.displayedDays, winDim);
                    SDL_RenderPresent(ren);
                }
                break; // End of SDLK_DOWN case
            case SDL_MOUSEWHEEL:
                if (ev.wheel.y > 0) { // Scroll up
                    appConfig.displayedDays = std::max(10, appConfig.displayedDays - 1);
                } else if (ev.wheel.y < 0) { // Scroll down
                    appConfig.displayedDays = std::min(90, appConfig.displayedDays + 1);
                }
                saveConfig(appConfig);
                renderChart(ren, font, fontSm, price_history, appConfig.tickers, g_currentTickerIndex, viewMode, appConfig.displayedDays, winDim);
                SDL_RenderPresent(ren);
                break; // End of SDL_MOUSEWHEEL case
            case SDL_MOUSEMOTION:
                setMousePosition(ev.motion.x, ev.motion.y);
                mouseMoved = true; // Set flag, don't render immediately
                break;
            case SDL_MOUSEBUTTONDOWN:
                if (ev.button.button == SDL_BUTTON_LEFT) {
                    ViewMode clickedMode;
                    int clickedTickerIndex;
                    if (getClickedViewMode(ev.button.x, ev.button.y, clickedMode)) {
                        viewMode = clickedMode;
                        appConfig.viewMode = viewMode;
                        saveConfig(appConfig);
                        renderChart(ren, font, fontSm, price_history, appConfig.tickers, g_currentTickerIndex, viewMode, appConfig.displayedDays, winDim);
                        SDL_RenderPresent(ren);
                    } else if (getClickedTicker(ev.button.x, ev.button.y, clickedTickerIndex)) {
                        if (clickedTickerIndex != g_currentTickerIndex) {
                            g_currentTickerIndex = clickedTickerIndex;
                            appConfig.lastActiveTickerIndex = g_currentTickerIndex;
                            saveConfig(appConfig);
                            std::cout << "Switching to ticker: " << appConfig.tickers[g_currentTickerIndex] << " ...\n";

                            int fetchDays = FETCH_DATA_COUNT + LOOKBACK_DAYS;
                            curl_global_init(CURL_GLOBAL_DEFAULT);
                            std::string json = fetchJSON(appConfig.tickers[g_currentTickerIndex], fetchDays);

                            if (!json.empty()) {
                                auto fresh = parseResponse(json, fetchDays);
                                if (!fresh.empty()) {
                                    price_history = std::move(fresh);
                                    std::cout << "Loaded " << price_history.size()
                                              << " trading days  ("
                                              << price_history.front().date << "  ->  "
                                              << price_history.back().date << ")\n";
                                } else {
                                    std::cerr << "No trading data found for " << appConfig.tickers[g_currentTickerIndex] << "\n";
                                }
                            } else {
                                std::cerr << "Failed to fetch data for " << appConfig.tickers[g_currentTickerIndex] << ". Check network and ticker symbol.\n";
                            }
                            curl_global_cleanup();
                        }
                        renderChart(ren, font, fontSm, price_history, appConfig.tickers, g_currentTickerIndex, viewMode, appConfig.displayedDays, winDim);
                        SDL_RenderPresent(ren);
                    }
                }
                break;
            case SDL_WINDOWEVENT:
                if (ev.window.event == SDL_WINDOWEVENT_EXPOSED) {
                    // Rerender immediately for expose event
                    renderChart(ren, font, fontSm, price_history, appConfig.tickers, g_currentTickerIndex, viewMode, appConfig.displayedDays, winDim);
                    SDL_RenderPresent(ren);
                }
                else if (ev.window.event == SDL_WINDOWEVENT_RESIZED) {
                    int newWidth = ev.window.data1;
                    int newHeight = ev.window.data2;
                    winDim = WindowDimensions::calculate(newWidth, newHeight);
                    // Rerender immediately for resize event
                    renderChart(ren, font, fontSm, price_history, appConfig.tickers, g_currentTickerIndex, viewMode, appConfig.displayedDays, winDim);
                    SDL_RenderPresent(ren);
                }
                break;
            }
        }
        // After processing all events, if mouse moved, render once
        if (mouseMoved) {
            renderChart(ren, font, fontSm, price_history, appConfig.tickers, g_currentTickerIndex, viewMode, appConfig.displayedDays, winDim);
            SDL_RenderPresent(ren);
            mouseMoved = false; // Reset flag
        }
        // If no events are pending, and no mouse motion, SDL_WaitEvent will block until next event.
        // If there are events pending, SDL_PollEvent will consume them.
        // This structure ensures that rendering happens either immediately for certain events
        // or once per frame for coalesced mouse motion.
        if (!SDL_PollEvent(NULL) && !mouseMoved) { // Only wait if no events are pending and mouse didn't move
            SDL_WaitEvent(NULL); // Wait for an event to avoid busy-waiting
        }
    }

    // ── Cleanup ──
    cleanup(resources, restServer, appConfig);
    return 0;
}
