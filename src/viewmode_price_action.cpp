#include "viewmode_price_action.h"

#include <cmath>
#include <iomanip>
#include <sstream>
#include <algorithm> // For std::sort, std::unique, std::min, std::max

// --- Price Action specific constants and colors ---
static constexpr RGBA COL_SR_LEVEL     = {255, 200, 100, 180}; // Support/Resistance lines
static constexpr RGBA COL_SR_LABEL     = {255, 200, 100, 255}; // Support/Resistance labels
static constexpr RGBA COL_UPTREND      = {100, 200, 255, 200};  // Uptrend lines (blue)
static constexpr RGBA COL_DOWNTREND    = {255, 100, 100, 200};  // Downtrend lines (red)
static constexpr RGBA COL_PATTERN      = {200, 150, 255, 200};  // Chart patterns (purple)
static constexpr RGBA COL_PATTERN_LABEL = {200, 150, 255, 255};

static constexpr int LOOKBACK_SWING = 2; // Periods to look left and right for swing high/low
static constexpr int MAX_SR_LEVELS  = 5; // Maximum number of S/R levels to display
static constexpr double PRICE_TOLERANCE = 2.0; // Price range tolerance for pattern detection

// Helper to check if a point is a swing high
bool isSwingHigh(int idx, int total, int lookback, const std::vector<PricePoint>& history) {
    if (idx < lookback || idx >= total - lookback) return false;
    double currentPrice = history[idx].price;
    for (int i = 1; i <= lookback; ++i) {
        if (history[idx - i].price >= currentPrice || history[idx + i].price >= currentPrice) {
            return false;
        }
    }
    return true;
}

// Helper to check if a point is a swing low
bool isSwingLow(int idx, int total, int lookback, const std::vector<PricePoint>& history) {
    if (idx < lookback || idx >= total - lookback) return false;
    double currentPrice = history[idx].price;
    for (int i = 1; i <= lookback; ++i) {
        if (history[idx - i].price <= currentPrice || history[idx + i].price <= currentPrice) {
            return false;
        }
    }
    return true;
}

// Structure to hold swing point info
struct SwingPoint {
    int index;
    double price;
    bool isHigh;
};

// Get all swing points (highs and lows) in the displayed range
std::vector<SwingPoint> getSwingPoints(int displayStart, int displayN, int total,
                                       const std::vector<PricePoint>& history) {
    std::vector<SwingPoint> swings;
    const int lookback = LOOKBACK_SWING;

    for (int i = displayStart + lookback; i < displayStart + displayN - lookback; ++i) {
        if (i < lookback || i >= total - lookback) continue;

        if (isSwingHigh(i, total, lookback, history)) {
            swings.push_back({i, history[i].price, true});
        } else if (isSwingLow(i, total, lookback, history)) {
            swings.push_back({i, history[i].price, false});
        }
    }
    return swings;
}

// Detect double top pattern (two highs at similar price levels with a valley between)
bool detectDoubleTop(const std::vector<SwingPoint>& swings, int& idx1, int& idx2) {
    for (size_t i = 0; i + 2 < swings.size(); ++i) {
        if (swings[i].isHigh && !swings[i+1].isHigh && swings[i+2].isHigh) {
            double diff = std::abs(swings[i].price - swings[i+2].price);
            if (diff < PRICE_TOLERANCE) {
                idx1 = i;
                idx2 = i + 2;
                return true;
            }
        }
    }
    return false;
}

// Detect double bottom pattern (two lows at similar price levels with a peak between)
bool detectDoubleBottom(const std::vector<SwingPoint>& swings, int& idx1, int& idx2) {
    for (size_t i = 0; i + 2 < swings.size(); ++i) {
        if (!swings[i].isHigh && swings[i+1].isHigh && !swings[i+2].isHigh) {
            double diff = std::abs(swings[i].price - swings[i+2].price);
            if (diff < PRICE_TOLERANCE) {
                idx1 = i;
                idx2 = i + 2;
                return true;
            }
        }
    }
    return false;
}

// Detect head and shoulders pattern
bool detectHeadAndShoulders(const std::vector<SwingPoint>& swings, int& leftShoulder,
                           int& head, int& rightShoulder) {
    for (size_t i = 1; i + 3 < swings.size(); ++i) {
        // Pattern: High - Low - Higher High - Low - High (lower than center)
        if (swings[i-1].isHigh && !swings[i].isHigh &&
            swings[i+1].isHigh && !swings[i+2].isHigh && swings[i+3].isHigh) {

            // Head should be higher than shoulders
            if (swings[i+1].price > swings[i-1].price &&
                swings[i+1].price > swings[i+3].price &&
                std::abs(swings[i-1].price - swings[i+3].price) < PRICE_TOLERANCE) {
                leftShoulder = i - 1;
                head = i + 1;
                rightShoulder = i + 3;
                return true;
            }
        }
    }
    return false;
}


void renderPriceActionOverlay(SDL_Renderer* ren, TTF_Font* fontSm,
                              const std::vector<PricePoint>& price_history,
                              const ChartRegion& cr) {
    if (price_history.empty()) {
        return;
    }

    int total = static_cast<int>(price_history.size());
    if (cr.N <= 0 || cr.displayStart + cr.N > total) return;

    // Clip drawing to chart area
    SDL_Rect clipRect = {cr.cL, cr.cT, cr.cW, cr.cH};
    SDL_RenderSetClipRect(ren, &clipRect);

    std::vector<double> sr_levels;

    // Identify potential swing highs and lows within the displayed range
    for (int i = cr.displayStart + LOOKBACK_SWING; i < cr.displayStart + cr.N - LOOKBACK_SWING; ++i) {
        if (i < 0 || i >= total) continue;

        if (isSwingHigh(i, total, LOOKBACK_SWING, price_history)) {
            sr_levels.push_back(price_history[i].price);
        } else if (isSwingLow(i, total, LOOKBACK_SWING, price_history)) {
            sr_levels.push_back(price_history[i].price);
        }
    }

    // Sort levels and remove close duplicates
    std::sort(sr_levels.begin(), sr_levels.end());
    sr_levels.erase(std::unique(sr_levels.begin(), sr_levels.end(), [](double a, double b) {
        return std::abs(a - b) < 0.5;
    }), sr_levels.end());

    // Limit S/R levels if too many
    if (sr_levels.size() > MAX_SR_LEVELS) {
        std::vector<double> filtered_sr_levels;
        double currentMidPrice = price_history[cr.displayStart + cr.N / 2].price;

        std::sort(sr_levels.begin(), sr_levels.end(), [&](double a, double b) {
            return std::abs(a - currentMidPrice) < std::abs(b - currentMidPrice);
        });

        for (size_t i = 0; i < std::min((size_t)MAX_SR_LEVELS, sr_levels.size()); ++i) {
            filtered_sr_levels.push_back(sr_levels[i]);
        }
        sr_levels = filtered_sr_levels;
        std::sort(sr_levels.begin(), sr_levels.end());
    }

    // Remove clip for labels drawn outside chart area
    SDL_RenderSetClipRect(ren, nullptr);

    // Draw S/R levels
    SDL_SetRenderDrawColor(ren, COL_SR_LEVEL.r, COL_SR_LEVEL.g,
                           COL_SR_LEVEL.b, COL_SR_LEVEL.a);
    for (double level : sr_levels) {
        int y = cr.toY(level);
        if (y >= cr.cT && y <= cr.cB) {
            drawDashedHLine(ren, cr.cL, cr.cR, y);
            std::ostringstream oss;
            oss << "S/R " << std::fixed << std::setprecision(2) << level;
            drawText(ren, fontSm, oss.str(), cr.cR + 4, y, COL_SR_LABEL, 0, 1);
        }
    }

    // Get swing points for trend line and pattern detection
    std::vector<SwingPoint> swings = getSwingPoints(cr.displayStart, cr.N, total, price_history);

    // Draw trend lines
    if (swings.size() >= 2) {
        // Uptrend: connect swing lows
        std::vector<SwingPoint> lows;
        for (const auto& s : swings) {
            if (!s.isHigh) lows.push_back(s);
        }
        if (lows.size() >= 2) {
            SDL_SetRenderDrawColor(ren, COL_UPTREND.r, COL_UPTREND.g,
                                   COL_UPTREND.b, COL_UPTREND.a);
            int x1 = cr.toX(lows[0].index - cr.displayStart);
            int y1 = cr.toY(lows[0].price);
            int x2 = cr.toX(lows[lows.size()-1].index - cr.displayStart);
            int y2 = cr.toY(lows[lows.size()-1].price);
            SDL_RenderDrawLine(ren, x1, y1, x2, y2);
        }

        // Downtrend: connect swing highs
        std::vector<SwingPoint> highs;
        for (const auto& s : swings) {
            if (s.isHigh) highs.push_back(s);
        }
        if (highs.size() >= 2) {
            SDL_SetRenderDrawColor(ren, COL_DOWNTREND.r, COL_DOWNTREND.g,
                                   COL_DOWNTREND.b, COL_DOWNTREND.a);
            int x1 = cr.toX(highs[0].index - cr.displayStart);
            int y1 = cr.toY(highs[0].price);
            int x2 = cr.toX(highs[highs.size()-1].index - cr.displayStart);
            int y2 = cr.toY(highs[highs.size()-1].price);
            SDL_RenderDrawLine(ren, x1, y1, x2, y2);
        }
    }

    // Detect and draw chart patterns
    int dt_idx1 = -1, dt_idx2 = -1;
    int db_idx1 = -1, db_idx2 = -1;
    int ls = -1, h = -1, rs = -1;

    SDL_SetRenderDrawColor(ren, COL_PATTERN.r, COL_PATTERN.g,
                           COL_PATTERN.b, COL_PATTERN.a);

    // Double Top
    if (detectDoubleTop(swings, dt_idx1, dt_idx2)) {
        int x1 = cr.toX(swings[dt_idx1].index - cr.displayStart);
        int y1 = cr.toY(swings[dt_idx1].price);
        int x2 = cr.toX(swings[dt_idx2].index - cr.displayStart);
        int y2 = cr.toY(swings[dt_idx2].price);

        // Draw pattern outline
        thickLine(ren, x1, y1, x2, y2);
        SDL_Rect dot1 = {x1 - 3, y1 - 3, 6, 6};
        SDL_Rect dot2 = {x2 - 3, y2 - 3, 6, 6};
        SDL_RenderDrawRect(ren, &dot1);
        SDL_RenderDrawRect(ren, &dot2);

        drawText(ren, fontSm, "Double Top", (x1 + x2) / 2, std::min(y1, y2) - 15,
                 COL_PATTERN_LABEL, 1, 1);
    }

    // Double Bottom
    if (detectDoubleBottom(swings, db_idx1, db_idx2)) {
        int x1 = cr.toX(swings[db_idx1].index - cr.displayStart);
        int y1 = cr.toY(swings[db_idx1].price);
        int x2 = cr.toX(swings[db_idx2].index - cr.displayStart);
        int y2 = cr.toY(swings[db_idx2].price);

        // Draw pattern outline
        thickLine(ren, x1, y1, x2, y2);
        SDL_Rect dot1 = {x1 - 3, y1 - 3, 6, 6};
        SDL_Rect dot2 = {x2 - 3, y2 - 3, 6, 6};
        SDL_RenderDrawRect(ren, &dot1);
        SDL_RenderDrawRect(ren, &dot2);

        drawText(ren, fontSm, "Double Bottom", (x1 + x2) / 2, std::max(y1, y2) + 15,
                 COL_PATTERN_LABEL, 1, 0);
    }

    // Head and Shoulders
    if (detectHeadAndShoulders(swings, ls, h, rs)) {
        int xls = cr.toX(swings[ls].index - cr.displayStart);
        int yls = cr.toY(swings[ls].price);
        int xh = cr.toX(swings[h].index - cr.displayStart);
        int yh = cr.toY(swings[h].price);
        int xrs = cr.toX(swings[rs].index - cr.displayStart);
        int yrs = cr.toY(swings[rs].price);

        // Draw neckline between valleys
        int x_valley1 = (xls + xh) / 2;
        int y_valley1 = (yls + yh) / 2;
        int x_valley2 = (xh + xrs) / 2;
        int y_valley2 = (yh + yrs) / 2;
        SDL_RenderDrawLine(ren, x_valley1, y_valley1, x_valley2, y_valley2);

        // Draw shoulders and head
        SDL_Rect ls_dot = {xls - 3, yls - 3, 6, 6};
        SDL_Rect h_dot = {xh - 3, yh - 3, 6, 6};
        SDL_Rect rs_dot = {xrs - 3, yrs - 3, 6, 6};
        SDL_RenderDrawRect(ren, &ls_dot);
        SDL_RenderDrawRect(ren, &h_dot);
        SDL_RenderDrawRect(ren, &rs_dot);

        drawText(ren, fontSm, "H&S", xh, yh - 20, COL_PATTERN_LABEL, 1, 1);
    }
}
