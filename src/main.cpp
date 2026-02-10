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

// ─── Colour palette ───
static constexpr RGBA COL_BG    = { 18,  18,  40, 255};
static constexpr RGBA COL_GRID  = { 45,  45,  75, 255};
static constexpr RGBA COL_AXIS  = { 90,  90, 130, 255};
static constexpr RGBA COL_LINE  = {  0, 200, 100, 255};
static constexpr RGBA COL_FILL  = {  0, 200, 100,  30};
static constexpr RGBA COL_TEXT  = {180, 180, 200, 255};
static constexpr RGBA COL_TITLE = {240, 240, 255, 255};
static constexpr RGBA COL_DOT   = {  0, 255, 140, 255};
static constexpr RGBA COL_HOVER_LABEL = {255, 255, 255, 255}; // White for hover label
static constexpr RGBA COL_LABEL_BG    = {0, 0, 0, 30}; // Semi-transparent dark background (alpha 90 out of 255)
// --- Current ticker state ---
static int g_currentTickerIndex = 0; // Index of the currently displayed ticker

// ═══════════════════════  Font discovery  ═══════════════════════

static std::string findFont() {
    static const char* paths[] = {
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/TTF/DejaVuSans.ttf",
        "/usr/share/fonts/dejavu-sans-fonts/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
        "/usr/share/fonts/liberation-sans/LiberationSans-Regular.ttf",
        "/usr/share/fonts/truetype/ubuntu/Ubuntu-R.ttf",
        "/usr/share/fonts/truetype/freefont/FreeSans.ttf",
        "/usr/share/fonts/truetype/noto/NotoSans-Regular.ttf",
        "/usr/share/fonts/google-noto/NotoSans-Regular.ttf",
        "/usr/share/fonts/noto/NotoSans-Regular.ttf",
    };
    for (auto* p : paths) {
        if (FILE* f = fopen(p, "r")) { fclose(f); return p; }
    }
    return {};
}

// ═══════════════════════  Chart renderer  ═══════════════════════

static void renderChart(SDL_Renderer* ren, TTF_Font* font, TTF_Font* fontSm,
                        const std::vector<PricePoint>& price_history,
                        const std::vector<std::string>& tickers, int currentTickerIndex, ViewMode viewMode,
                        int displayDays, const WindowDimensions& winDim) {
    // Clear
    SDL_SetRenderDrawColor(ren, COL_BG.r, COL_BG.g, COL_BG.b, 255);
    SDL_RenderClear(ren);

    const int cL = winDim.marginLeft;
    const int cR = winDim.width - winDim.marginRight;
    const int cT = winDim.marginTop;
    const int cB = winDim.height - winDim.marginBottom;
    const int cW = cR - cL;
    const int cH = cB - cT;

    // Clip drawing to chart area
    SDL_Rect clipRect = {cL, cT, cW + 1, cH + 1}; // Expand clipping region by 1 pixel to include borders
    SDL_RenderSetClipRect(ren, &clipRect);

    int total = static_cast<int>(price_history.size());
    int dispN = std::min(total, displayDays);
    int off   = total - dispN;          // lookback data lives at 0..off-1

    // Reset mouse in chart area flag for this render cycle
    setMouseInChartArea(false);

    if (price_history.empty()) {
        drawText(ren, font, "No data available",
                 winDim.width / 2, winDim.height / 2, COL_TEXT, 1, 1);
        return;
    }

    // Price range (displayed points only)
    double lo = price_history[off].price, hi = lo;
    for (int i = off; i < total; ++i) {
        lo = std::min(lo, price_history[i].price);
        hi = std::max(hi, price_history[i].price);
    }
    double range = hi - lo;
    if (range < 0.01) range = 1.0;
    double pad = range * 0.08;
    lo -= pad;
    hi += pad;

    double step = niceStep(hi - lo, 8);
    double gMin = std::floor(lo / step) * step;
    double gMax = std::ceil(hi  / step) * step;
    range = gMax - gMin;

    // toY: price → pixel Y
    auto toY = [&](double price) -> int {
        return cB - static_cast<int>((price - gMin) / range * cH);
    };
    // toX: display index (0..dispN-1) → pixel X
    auto toX = [&](int di) -> int {
        return (dispN <= 1) ? cL + cW / 2
                            : cL + static_cast<int>(double(di) / (dispN - 1) * cW);
    };

    // ── Horizontal grid + price labels ──
    SDL_SetRenderDrawColor(ren, COL_GRID.r, COL_GRID.g, COL_GRID.b, COL_GRID.a);
    for (double p = gMin; p <= gMax + step * 0.001; p += step) {
        int y = toY(p);
        if (y < cT || y > cB) continue;
        SDL_RenderDrawLine(ren, cL, y, cR, y);
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(step >= 1.0 ? 1 : 2) << p;
        drawText(ren, fontSm, oss.str(), cL - 8, y, COL_TEXT, 2, 1);
    }

    // ── Vertical grid + date labels ──
    int maxLabels = cW / 75;
    int labelStep = std::max(1, (dispN - 1) / std::max(1, maxLabels));
    for (int di = 0; di < dispN; di += labelStep) {
        int x = toX(di);
        SDL_SetRenderDrawColor(ren, COL_GRID.r, COL_GRID.g, COL_GRID.b, COL_GRID.a);
        SDL_RenderDrawLine(ren, x, cT, x, cB);
        drawText(ren, fontSm, shortDate(price_history[off + di].date), x, cB + 8, COL_TEXT, 1, 0);
    }
    // Always draw rightmost vertical line if the loop didn't land on it
    {
        int lastDi = dispN - 1;
        if (lastDi % labelStep != 0) {
            int x = toX(lastDi);
            SDL_SetRenderDrawColor(ren, COL_GRID.r, COL_GRID.g, COL_GRID.b, COL_GRID.a);
            SDL_RenderDrawLine(ren, x, cT, x, cB);
            drawText(ren, fontSm, shortDate(price_history[off + lastDi].date), x, cB + 8, COL_TEXT, 1, 0);
        }
    }

    // ── Axes ──
    SDL_SetRenderDrawColor(ren, COL_AXIS.r, COL_AXIS.g, COL_AXIS.b, COL_AXIS.a);
    SDL_RenderDrawLine(ren, cL, cT, cL, cB);
    SDL_RenderDrawLine(ren, cL, cB, cR, cB);

    // ── Filled area under line ──
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(ren, COL_FILL.r, COL_FILL.g, COL_FILL.b, COL_FILL.a);
    for (int di = 0; di < dispN; ++di) {
        int x = toX(di), y = toY(price_history[off + di].price);
        SDL_RenderDrawLine(ren, x, y, x, cB);
    }

    // ── Price line ──
    SDL_SetRenderDrawColor(ren, COL_LINE.r, COL_LINE.g, COL_LINE.b, COL_LINE.a);
    for (int di = 1; di < dispN; ++di)
        thickLine(ren, toX(di - 1), toY(price_history[off + di - 1].price),
                       toX(di),     toY(price_history[off + di].price));

    // ── Data dots ──
    SDL_SetRenderDrawColor(ren, COL_DOT.r, COL_DOT.g, COL_DOT.b, COL_DOT.a);
    for (int di = 0; di < dispN; ++di) {
        int x = toX(di), y = toY(price_history[off + di].price);
        SDL_Rect dot = {x - 2, y - 2, 5, 5};
        SDL_RenderFillRect(ren, &dot);
    }

    // ── View-mode overlays ──
    {
        ChartRegion cr{cL, cR, cT, cB, cW, cH, dispN, off, toY, toX};
        if (viewMode == ViewMode::PriceChartStats)
            renderStatsOverlay(ren, fontSm, price_history, cr);
        else if (viewMode == ViewMode::Bollinger)
            renderBollingerOverlay(ren, fontSm, price_history, cr);
        else if (viewMode == ViewMode::MACross)
            renderMACrossOverlay(ren, fontSm, price_history, cr);

    }

    // ── Last-price annotation ──
    {
        const auto& last = price_history.back();
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(2) << last.price;
        int x = toX(dispN - 1), y = toY(last.price);
        // dashed tick on right edge
        SDL_SetRenderDrawColor(ren, COL_DOT.r, COL_DOT.g, COL_DOT.b, COL_DOT.a);
        for (int dx = x; dx <= cR; dx += 4)
            SDL_RenderDrawPoint(ren, dx, y);
        drawText(ren, fontSm, oss.str(), cR + 4, y, COL_DOT, 0, 1);
    }

    // Remove clip for labels drawn outside chart area
    SDL_RenderSetClipRect(ren, nullptr);

    // View mode rectangles and ticker buttons (drawn outside clipping region)
    const int barTopY = 5;
    int rightEdge = renderViewModeBar(ren, fontSm, viewMode, barTopY, cL, COL_BG);

    // Ticker buttons (right-aligned to chart edge)
    renderTickerBar(ren, fontSm, tickers, currentTickerIndex, barTopY, cR, COL_BG);

    // Trading days info (to the right of view mode buttons)
    std::string tickerInfo = std::to_string(dispN) + " Trading Days";
    drawText(ren, fontSm, tickerInfo, rightEdge + 20, barTopY + 15, COL_TEXT, 0, 1);

    // ── Instructions ──
    drawText(ren, fontSm, "TAB: switch view | R: reload | F: fullscreen | UpDownKeys/Mouse Wheel: zoom | ESC: quit",
             winDim.width / 2, winDim.height - 16, COL_GRID, 1, 1);

    // ── Mouse hover label ──
    if (getMouseX() >= cL && getMouseX() <= cR && getMouseY() >= cT && getMouseY() <= cB) {
        setMouseInChartArea(true);
        // Find the closest price point on the X axis to the mouse cursor
        int closest_di = -1;
        int min_dist = std::numeric_limits<int>::max();

        // Iterate only through displayed points
        for (int di = 0; di < dispN; ++di) {
            int x_coord = toX(di);
            int dist = std::abs(x_coord - getMouseX());
            if (dist < min_dist) {
                min_dist = dist;
                closest_di = di;
            }
        }

        if (closest_di != -1) {
            int hovered_real_idx = off + closest_di;
            if (hovered_real_idx >= 0 && hovered_real_idx < total) {
                const PricePoint& hovered_pp = price_history[hovered_real_idx];
                int hover_x = toX(closest_di);
                int hover_y = toY(hovered_pp.price);

                // Price label
                std::ostringstream price_oss;
                price_oss << std::fixed << std::setprecision(2) << hovered_pp.price;
                std::string price_str = price_oss.str();

                // Date label (formatted as at the bottom of the graph)
                std::string formatted_date = shortDate(hovered_pp.date);

                // Get text dimensions
                int price_w, price_h;
                TTF_SizeText(fontSm, price_str.c_str(), &price_w, &price_h);
                int date_w, date_h;
                TTF_SizeText(fontSm, formatted_date.c_str(), &date_w, &date_h);

                int label_padding = 5;
                int label_width = std::max(price_w, date_w) + label_padding * 2;
                int label_height = price_h + date_h + label_padding * 3; // 2 lines + 3 paddings (top, middle, bottom)

                // Calculate position for the background rectangle
                // Offset it slightly from the hover_x, hover_y
                int bg_x = hover_x + 10;
                int bg_y = hover_y - 30 - label_padding; // Start above the first line of text

                // Ensure label stays within screen bounds (right edge)
                if (bg_x + label_width > winDim.width) {
                    bg_x = winDim.width - label_width - 5; // 5 pixels margin from right edge
                }
                // Ensure label stays within screen bounds (top edge)
                if (bg_y < cT) {
                    bg_y = cT + 5; // 5 pixels margin from top edge
                }

                SDL_Rect bg_rect = {bg_x, bg_y, label_width, label_height};

                // Draw semi-transparent background
                SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
                SDL_SetRenderDrawColor(ren, COL_LABEL_BG.r, COL_LABEL_BG.g,
                                       COL_LABEL_BG.b, COL_LABEL_BG.a);
                SDL_RenderFillRect(ren, &bg_rect);

                // Draw price text
                drawText(ren, fontSm, price_str, bg_x + label_padding, bg_y + label_padding, COL_HOVER_LABEL, 0, 0);

                // Draw date text
                drawText(ren, fontSm, formatted_date, bg_x + label_padding, bg_y + label_padding + price_h + label_padding, COL_HOVER_LABEL, 0, 0);
                
                // Draw a small dot or circle on the hovered point for better visibility
                SDL_SetRenderDrawColor(ren, COL_HOVER_LABEL.r, COL_HOVER_LABEL.g, COL_HOVER_LABEL.b, COL_HOVER_LABEL.a);
                SDL_Rect dot = {hover_x - 3, hover_y - 3, 7, 7};
                SDL_RenderDrawRect(ren, &dot);
            }
        }
    } else {
        setMouseInChartArea(false);
    }
}

// ═══════════════════════  main  ═══════════════════════

int main(int, char* []) {
    Config appConfig = loadConfig();
    g_currentTickerIndex = appConfig.lastActiveTickerIndex;
    // Ensure g_currentTickerIndex is within valid bounds
    if (static_cast<size_t>(g_currentTickerIndex) >= appConfig.tickers.size()) {
        g_currentTickerIndex = 0;
    }

    // Initialize REST server
    RestServer restServer;
    restServer.setAvailableTickers(appConfig.tickers);
    restServer.start(8080);

    ViewMode viewMode = appConfig.viewMode; // Declare and initialize viewMode here
    bool FULLSCREEN = appConfig.fullscreen; // Initialize FULLSCREEN as a local variable from config




    int fetchDays = FETCH_DATA_COUNT + LOOKBACK_DAYS;
    std::cout << "Fetching " << FETCH_DATA_COUNT << " trading days for " << appConfig.tickers[g_currentTickerIndex]
              << " (+" << LOOKBACK_DAYS << " lookback) ...\n";

    curl_global_init(CURL_GLOBAL_DEFAULT);
    std::string json = fetchJSON(appConfig.tickers[g_currentTickerIndex], fetchDays);

    if (json.empty()) {
        curl_global_cleanup();
        std::cerr << "Failed to fetch data. Check network and ticker symbol.\n";
        return 1;
    }

    auto price_history = parseResponse(json, fetchDays);
    if (price_history.empty()) {
        std::cerr << "No trading data found for " << appConfig.tickers[g_currentTickerIndex] << "\n";
        curl_global_cleanup();
        return 1;
    }

    std::cout << "Loaded " << price_history.size() << " trading days  ("
              << price_history.front().date << "  ->  " << price_history.back().date << ")\n";

    // ── SDL init ──
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::cerr << "SDL_Init: " << SDL_GetError() << "\n"; return 1;
    }
    if (TTF_Init() != 0) {
        std::cerr << "TTF_Init: " << TTF_GetError() << "\n";
        SDL_Quit(); return 1;
    }

    std::string fontPath = findFont();
    if (fontPath.empty()) {
        std::cerr << "No TrueType font found. Install dejavu or liberation fonts.\n";
        TTF_Quit(); SDL_Quit(); return 1;
    }

    TTF_Font* font   = TTF_OpenFont(fontPath.c_str(), 18);
    TTF_Font* fontSm = TTF_OpenFont(fontPath.c_str(), 13);
    if (!font || !fontSm) {
        std::cerr << "Failed to load font: " << TTF_GetError() << "\n";
        TTF_Quit(); SDL_Quit(); return 1;
    }

    Uint32 windowFlags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN; // Always start hidden and resizable

    SDL_Window* win = SDL_CreateWindow(
        ("Stock - " + appConfig.tickers[g_currentTickerIndex]).c_str(),
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        DEFAULT_GRAPH_WIDTH, DEFAULT_GRAPH_HEIGHT,
        windowFlags);
    if (!win) {
        std::cerr << "SDL_CreateWindow: " << SDL_GetError() << "\n";
        TTF_CloseFont(font); TTF_CloseFont(fontSm);
        TTF_Quit(); SDL_Quit(); return 1;
    }

    // Set fullscreen mode explicitly if enabled in config
    if (FULLSCREEN) {
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
        TTF_CloseFont(font); TTF_CloseFont(fontSm);
        TTF_Quit(); SDL_Quit(); return 1;
    }

    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);

    // ── Initial render ──

    // Show window after all setup and initial render is complete
    SDL_ShowWindow(win);

    // Apply fullscreen mode after showing the window if configured
    if (FULLSCREEN) {
        SDL_SetWindowFullscreen(win, SDL_WINDOW_FULLSCREEN);
        // Update winDim after setting fullscreen to get actual dimensions
        int newWidth, newHeight;
        SDL_GetWindowSize(win, &newWidth, &newHeight);
        winDim = WindowDimensions::calculate(newWidth, newHeight);
    }

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
    restServer.stop();
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    TTF_CloseFont(fontSm);
    TTF_CloseFont(font);
    TTF_Quit();
    SDL_Quit();
    saveConfig(appConfig);
    curl_global_cleanup();
    return 0;
}
