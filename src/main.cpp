/*
 * StockChart - ETF Price Chart Viewer
 *
 * Usage: ./stockchart [TICKER] [DAYS]
 *   TICKER  Yahoo Finance ticker symbol (default: VWCE.DE)
 *   DAYS    Number of trading days to display (default: 30)
 *
 * Dependencies: SDL2, SDL2_ttf, libcurl
 *   Ubuntu/Debian: sudo apt install libsdl2-dev libsdl2-ttf-dev libcurl4-openssl-dev
 */

#include "chart.h"
#include "viewmode_bollinger.h"
#include "viewmode_macross.h"
#include "viewmode_stats.h"
#include "config.h"


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
static constexpr RGBA COL_VIEWMODE_BG = {35, 35, 50, 255};      // Inactive tab (dark gray)
static constexpr RGBA COL_VIEWMODE_ACTIVE = {130, 130, 150, 255}; // Active tab (light gray)
static constexpr RGBA COL_VIEWMODE_TEXT = {180, 180, 200, 255}; // Text inside rectangles

// --- Mouse hover state ---
static int g_mouseX = 0;
static int g_mouseY = 0;
static bool g_mouseInChartArea = false;

// ═══════════════════════  Network  ═══════════════════════

static size_t curlWrite(void* buf, size_t sz, size_t n, std::string* out) {
    out->append(static_cast<char*>(buf), sz * n);
    return sz * n;
}

static std::string fetchJSON(const std::string& ticker, int days) {
    CURL* c = curl_easy_init();
    if (!c) { std::cerr << "curl_easy_init failed\n"; return {}; }

    curl_easy_setopt(c, CURLOPT_COOKIEFILE, "");        // enable cookie engine
    curl_easy_setopt(c, CURLOPT_USERAGENT,
        "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36");
    curl_easy_setopt(c, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(c, CURLOPT_TIMEOUT, 15L);
    curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, curlWrite);

    // Step 1 — hit fc.yahoo.com to seed cookies
    std::string dummy;
    curl_easy_setopt(c, CURLOPT_URL, "https://fc.yahoo.com/");
    curl_easy_setopt(c, CURLOPT_WRITEDATA, &dummy);
    curl_easy_perform(c);                               // result ignored

    // Step 2 — obtain crumb token
    std::string crumb;
    curl_easy_setopt(c, CURLOPT_URL,
        "https://query2.finance.yahoo.com/v1/test/getcrumb");
    curl_easy_setopt(c, CURLOPT_WRITEDATA, &crumb);
    CURLcode res = curl_easy_perform(c);
    if (res != CURLE_OK || crumb.empty()) {
        std::cerr << "Warning: could not obtain auth crumb, trying without\n";
        crumb.clear();
    }

    // Step 3 — fetch chart data
    int calDays = static_cast<int>(days * 1.6) + 15;
    std::string url = "https://query2.finance.yahoo.com/v8/finance/chart/"
                    + ticker + "?range=" + std::to_string(calDays)
                    + "d&interval=1d";
    if (!crumb.empty()) {
        char* enc = curl_easy_escape(c, crumb.c_str(),
                                     static_cast<int>(crumb.size()));
        url += "&crumb=";
        url += enc;
        curl_free(enc);
    }

    std::string resp;
    curl_easy_setopt(c, CURLOPT_URL, url.c_str());
    curl_easy_setopt(c, CURLOPT_WRITEDATA, &resp);
    res = curl_easy_perform(c);

    long httpCode = 0;
    curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &httpCode);
    curl_easy_cleanup(c);

    if (res != CURLE_OK) {
        std::cerr << "HTTP request failed: " << curl_easy_strerror(res) << "\n";
        return {};
    }
    if (httpCode != 200) {
        std::cerr << "Yahoo Finance returned HTTP " << httpCode << "\n";
        if (httpCode == 404)
            std::cerr << "Ticker not found. Try adding an exchange suffix "
                         "(e.g. .DE  .L  .AS  .MI)\n";
        return {};
    }
    return resp;
}

// ═══════════════════════  JSON helpers  ═══════════════════════

static std::vector<double> jsonNumberArray(const std::string& js,
                                           const std::string& key,
                                           size_t from = 0) {
    std::vector<double> v;
    std::string needle = "\"" + key + "\"";
    size_t p = js.find(needle, from);
    if (p == std::string::npos) return v;
    p = js.find('[', p + needle.size());
    if (p == std::string::npos) return v;

    size_t depth = 1, e = p + 1;
    while (e < js.size() && depth > 0) {
        if (js[e] == '[') ++depth;
        else if (js[e] == ']') --depth;
        ++e;
    }

    std::string body = js.substr(p + 1, e - p - 2);
    std::istringstream ss(body);
    std::string tok;
    while (std::getline(ss, tok, ',')) {
        auto a = tok.find_first_not_of(" \t\n\r");
        if (a == std::string::npos) { v.push_back(NAN); continue; }
        tok = tok.substr(a);
        auto b = tok.find_last_not_of(" \t\n\r");
        if (b != std::string::npos) tok.resize(b + 1);

        if (tok == "null" || tok.empty())
            v.push_back(NAN);
        else {
            try   { v.push_back(std::stod(tok)); }
            catch (...) { v.push_back(NAN); }
        }
    }
    return v;
}

static std::vector<PricePoint> parseResponse(const std::string& js, int maxDays) {
    std::vector<PricePoint> pts;
    if (js.find("\"result\":null") != std::string::npos) {
        std::cerr << "Yahoo Finance returned an error payload\n";
        return pts;
    }

    auto ts = jsonNumberArray(js, "timestamp");
    size_t qp = js.find("\"quote\"");
    if (qp == std::string::npos) {
        std::cerr << "No quote data in response\n";
        return pts;
    }
    auto cl = jsonNumberArray(js, "close", qp);

    size_t n = std::min(ts.size(), cl.size());
    for (size_t i = 0; i < n; ++i) {
        if (std::isnan(cl[i])) continue;           // skip non-trading days
        time_t t = static_cast<time_t>(ts[i]);
        struct tm tm{};
        gmtime_r(&t, &tm);
        char buf[11];
        strftime(buf, sizeof(buf), "%Y-%m-%d", &tm);
        pts.push_back({cl[i], buf});
    }

    if (static_cast<int>(pts.size()) > maxDays)
        pts.erase(pts.begin(),
                  pts.begin() + static_cast<long>(pts.size() - maxDays));
    return pts;
}

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

// ═══════════════════════  Drawing helpers  ═══════════════════════

static double niceStep(double range, int target) {
    if (range <= 0) return 1.0;
    double rough = range / target;
    double mag   = std::pow(10.0, std::floor(std::log10(rough)));
    double frac  = rough / mag;
    double nice  = (frac <= 1.5) ? 1
                 : (frac <= 3.5) ? 2
                 : (frac <= 7.5) ? 5
                 :                 10;
    return nice * mag;
}

// alignX: 0=left  1=centre  2=right     alignY: 0=top  1=centre
void drawText(SDL_Renderer* r, TTF_Font* f, const std::string& text,
              int x, int y, RGBA col, int ax, int ay) {
    if (text.empty()) return;
    SDL_Color sc = {col.r, col.g, col.b, col.a};
    SDL_Surface* s = TTF_RenderText_Blended(f, text.c_str(), sc);
    if (!s) return;
    SDL_Texture* tx = SDL_CreateTextureFromSurface(r, s);
    if (!tx) { SDL_FreeSurface(s); return; }
    SDL_Rect rc = {x, y, s->w, s->h};
    if (ax == 1) rc.x -= rc.w / 2;
    else if (ax == 2) rc.x -= rc.w;
    if (ay == 1) rc.y -= rc.h / 2;
    SDL_RenderCopy(r, tx, nullptr, &rc);
    SDL_DestroyTexture(tx);
    SDL_FreeSurface(s);
}

static std::string shortDate(const std::string& ymd) {
    if (ymd.size() < 10) return ymd;
    struct tm tm{};
    tm.tm_year = std::stoi(ymd.substr(0, 4)) - 1900;
    tm.tm_mon  = std::stoi(ymd.substr(5, 2)) - 1;
    tm.tm_mday = std::stoi(ymd.substr(8, 2));
    mktime(&tm);
    char buf[8];
    strftime(buf, sizeof(buf), "%b %d", &tm);
    return buf;
}

void thickLine(SDL_Renderer* r, int x1, int y1, int x2, int y2) {
    SDL_RenderDrawLine(r, x1, y1,     x2, y2);
    SDL_RenderDrawLine(r, x1, y1 - 1, x2, y2 - 1);
    SDL_RenderDrawLine(r, x1, y1 + 1, x2, y2 + 1);
}

void drawDashedHLine(SDL_Renderer* ren, int x1, int x2, int y,
                     int dashLen, int gapLen) {
    bool drawing = true;
    int seg = 0;
    for (int x = x1; x <= x2; ++x) {
        if (drawing) SDL_RenderDrawPoint(ren, x, y);
        if (++seg >= (drawing ? dashLen : gapLen)) {
            drawing = !drawing;
            seg = 0;
        }
    }
}

// ═══════════════════════  Chart renderer  ═══════════════════════

static std::string getViewModeName(ViewMode mode) {
    switch (mode) {
        case ViewMode::PriceChart:      return "Price";
        case ViewMode::PriceChartStats: return "Stats";
        case ViewMode::Bollinger:       return "Bollinger";
        case ViewMode::MACross:         return "MACross";
        default:                        return "Unknown";
    }
}

static int renderViewModeBar(SDL_Renderer* ren, TTF_Font* font, ViewMode currentMode,
                             int topY, int leftMargin) {
    const int rectHeight = 30;
    const int rectSpacing = 10;
    const int rectPadding = 12;

    int currentX = leftMargin;

    // Draw each rectangle
    for (int i = 0; i < VIEW_MODE_COUNT; i++) {
        ViewMode mode = static_cast<ViewMode>(i);
        std::string name = getViewModeName(mode);

        int textW, textH;
        TTF_SizeText(font, name.c_str(), &textW, &textH);

        int rectW = textW + 2 * rectPadding;
        SDL_Rect rect = {currentX, topY, rectW, rectHeight};

        // Draw filled rectangle
        if (mode == currentMode) {
            // Active view mode - filled with highlight color
            SDL_SetRenderDrawColor(ren, COL_VIEWMODE_ACTIVE.r, COL_VIEWMODE_ACTIVE.g,
                                   COL_VIEWMODE_ACTIVE.b, COL_VIEWMODE_ACTIVE.a);
        } else {
            // Inactive view mode - filled with background color
            SDL_SetRenderDrawColor(ren, COL_VIEWMODE_BG.r, COL_VIEWMODE_BG.g,
                                   COL_VIEWMODE_BG.b, COL_VIEWMODE_BG.a);
        }
        SDL_RenderFillRect(ren, &rect);

        // Draw text centered in rectangle
        RGBA textColor = (mode == currentMode) ? COL_BG : COL_VIEWMODE_TEXT;
        drawText(ren, font, name, currentX + rectW / 2, topY + rectHeight / 2,
                 textColor, 1, 1);

        currentX += rectW + rectSpacing;
    }

    // Return the right edge position (rightmost x + spacing)
    return currentX - rectSpacing;
}

static void renderChart(SDL_Renderer* ren, TTF_Font* font, TTF_Font* fontSm,
                        const std::vector<PricePoint>& price_history,
                        const std::string& ticker, ViewMode viewMode,
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

    int total = static_cast<int>(price_history.size());
    int dispN = std::min(total, displayDays);
    int off   = total - dispN;          // lookback data lives at 0..off-1

    // Reset mouse in chart area flag for this render cycle
    g_mouseInChartArea = false;

    // View mode rectangles (left-aligned to chart edge)
    const int barTopY = 5;
    int rightEdge = renderViewModeBar(ren, fontSm, viewMode, barTopY, cL);

    // Ticker info (to the right of view mode rectangles)
    std::string tickerInfo = ticker + " - " + std::to_string(dispN) + " Trading Days";
    drawText(ren, fontSm, tickerInfo, rightEdge + 20, barTopY + 15, COL_TEXT, 0, 1);

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

    // ── Instructions ──
    drawText(ren, fontSm, "TAB: switch view | R: reload | F: fullscreen | UpDownKeys/Mouse Wheel: zoom | ESC: quit",
             winDim.width / 2, winDim.height - 16, COL_GRID, 1, 1);

    // ── Mouse hover label ──
    if (g_mouseX >= cL && g_mouseX <= cR && g_mouseY >= cT && g_mouseY <= cB) {
        g_mouseInChartArea = true;
        // Find the closest price point on the X axis to the mouse cursor
        int closest_di = -1;
        int min_dist = std::numeric_limits<int>::max();

        // Iterate only through displayed points
        for (int di = 0; di < dispN; ++di) {
            int x_coord = toX(di);
            int dist = std::abs(x_coord - g_mouseX);
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
        g_mouseInChartArea = false;
    }
}

// ═══════════════════════  main  ═══════════════════════

int main(int argc, char* argv[]) {
    Config appConfig = loadConfig();
    std::string ticker = appConfig.ticker; // Initialize from config
    ViewMode viewMode = appConfig.viewMode; // Declare and initialize viewMode here
    bool FULLSCREEN = appConfig.fullscreen; // Initialize FULLSCREEN as a local variable from config

    if (argc >= 2) ticker = argv[1]; // Command line argument overrides config


    int fetchDays = FETCH_DATA_COUNT + LOOKBACK_DAYS;
    std::cout << "Fetching " << FETCH_DATA_COUNT << " trading days for " << ticker
              << " (+" << LOOKBACK_DAYS << " lookback) ...\n";

    curl_global_init(CURL_GLOBAL_DEFAULT);
    std::string json = fetchJSON(ticker, fetchDays);

    if (json.empty()) {
        curl_global_cleanup();
        std::cerr << "Failed to fetch data. Check network and ticker symbol.\n";
        return 1;
    }

    auto price_history = parseResponse(json, fetchDays);
    if (price_history.empty()) {
        std::cerr << "No trading data found for " << ticker << "\n";
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
        ("StockChart - " + ticker).c_str(),
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
                    renderChart(ren, font, fontSm, price_history, ticker, viewMode, appConfig.displayedDays, winDim);
                    SDL_RenderPresent(ren);
                }
                else if (ev.key.keysym.sym == SDLK_r) {
                    std::cout << "Reloading " << ticker << " ...\n";
                    std::string rj = fetchJSON(ticker, fetchDays);
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
                    renderChart(ren, font, fontSm, price_history, ticker, viewMode, appConfig.displayedDays, winDim);
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

                    renderChart(ren, font, fontSm, price_history, ticker, viewMode, appConfig.displayedDays, winDim);
                    SDL_RenderPresent(ren);
                }
                else if (ev.key.keysym.sym == SDLK_UP) {
                    appConfig.displayedDays = std::max(10, appConfig.displayedDays - DISPLAYED_DAYS_STEP);
                    saveConfig(appConfig);
                    renderChart(ren, font, fontSm, price_history, ticker, viewMode, appConfig.displayedDays, winDim);
                    SDL_RenderPresent(ren);
                }
                else if (ev.key.keysym.sym == SDLK_DOWN) {
                    appConfig.displayedDays = std::min(90, appConfig.displayedDays + DISPLAYED_DAYS_STEP);
                    saveConfig(appConfig);
                    renderChart(ren, font, fontSm, price_history, ticker, viewMode, appConfig.displayedDays, winDim);
                    SDL_RenderPresent(ren);
                }
                break; // End of SDLK_DOWN case
            case SDL_MOUSEWHEEL:
                if (ev.wheel.y > 0) { // Scroll up
                    appConfig.displayedDays = std::max(10, appConfig.displayedDays - DISPLAYED_DAYS_STEP);
                } else if (ev.wheel.y < 0) { // Scroll down
                    appConfig.displayedDays = std::min(90, appConfig.displayedDays + DISPLAYED_DAYS_STEP);
                }
                saveConfig(appConfig);
                renderChart(ren, font, fontSm, price_history, ticker, viewMode, appConfig.displayedDays, winDim);
                SDL_RenderPresent(ren);
                break; // End of SDL_MOUSEWHEEL case
            case SDL_MOUSEMOTION:
                g_mouseX = ev.motion.x;
                g_mouseY = ev.motion.y;
                mouseMoved = true; // Set flag, don't render immediately
                break;
            case SDL_WINDOWEVENT:
                if (ev.window.event == SDL_WINDOWEVENT_EXPOSED) {
                    // Rerender immediately for expose event
                    renderChart(ren, font, fontSm, price_history, ticker, viewMode, appConfig.displayedDays, winDim);
                    SDL_RenderPresent(ren);
                }
                else if (ev.window.event == SDL_WINDOWEVENT_RESIZED) {
                    int newWidth = ev.window.data1;
                    int newHeight = ev.window.data2;
                    winDim = WindowDimensions::calculate(newWidth, newHeight);
                    // Rerender immediately for resize event
                    renderChart(ren, font, fontSm, price_history, ticker, viewMode, appConfig.displayedDays, winDim);
                    SDL_RenderPresent(ren);
                }
                break;
            }
        }
        // After processing all events, if mouse moved, render once
        if (mouseMoved) {
            renderChart(ren, font, fontSm, price_history, ticker, viewMode, appConfig.displayedDays, winDim);
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
