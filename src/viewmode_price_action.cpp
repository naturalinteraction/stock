#include "viewmode_price_action.h"

#include <cmath>
#include <iomanip>
#include <sstream>
#include <algorithm> // For std::sort, std::unique, std::min, std::max

// --- Price Action specific constants and colors ---
static constexpr RGBA COL_SR_LEVEL  = {255, 200, 100, 180}; // Support/Resistance lines
static constexpr RGBA COL_SR_LABEL  = {255, 200, 100, 255}; // Support/Resistance labels

static constexpr int LOOKBACK_SWING = 2; // Periods to look left and right for swing high/low
static constexpr int MAX_SR_LEVELS  = 5; // Maximum number of S/R levels to display

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


void renderPriceActionOverlay(SDL_Renderer* ren, TTF_Font* fontSm,
                              const std::vector<PricePoint>& price_history,
                              const ChartRegion& cr) {
    if (price_history.empty()) {
        return;
    }

    int total = static_cast<int>(price_history.size());
    if (cr.N <= 0 || cr.displayStart + cr.N > total) return;

    std::vector<double> sr_levels;

    // Identify potential swing highs and lows within the displayed range
    // We iterate over the full history to find levels, then filter for displayed ones.
    for (int i = cr.displayStart + LOOKBACK_SWING; i < cr.displayStart + cr.N - LOOKBACK_SWING; ++i) {
        if (i < 0 || i >= total) continue; // Should not happen with adjusted loop bounds

        if (isSwingHigh(i, total, LOOKBACK_SWING, price_history)) {
            sr_levels.push_back(price_history[i].price);
        } else if (isSwingLow(i, total, LOOKBACK_SWING, price_history)) {
            sr_levels.push_back(price_history[i].price);
        }
    }

    // Sort levels and remove close duplicates
    std::sort(sr_levels.begin(), sr_levels.end());
    sr_levels.erase(std::unique(sr_levels.begin(), sr_levels.end(), [](double a, double b) {
        return std::abs(a - b) < 0.5; // Consider levels within 0.5 units as duplicates
    }), sr_levels.end());

    // If too many levels, prioritize those within the visible price range or most recent
    if (sr_levels.size() > MAX_SR_LEVELS) {
        // For simplicity, let's just take the MAX_SR_LEVELS levels closest to the current price range
        // A more sophisticated approach would be to assess significance or recency more accurately.
        std::vector<double> filtered_sr_levels;
        double currentMidPrice = (cr.toY.target<int(double)>() != nullptr) ? cr.toY.target<int(double)>()(0) : 0; // Get a mid-point from the Y-axis mapping (hacky, assumes 0 maps to something central)
        if (cr.N > 0) {
            currentMidPrice = price_history[cr.displayStart + cr.N / 2].price;
        }

        std::sort(sr_levels.begin(), sr_levels.end(), [&](double a, double b) {
            return std::abs(a - currentMidPrice) < std::abs(b - currentMidPrice);
        });

        for (size_t i = 0; i < std::min((size_t)MAX_SR_LEVELS, sr_levels.size()); ++i) {
            filtered_sr_levels.push_back(sr_levels[i]);
        }
        sr_levels = filtered_sr_levels;
        std::sort(sr_levels.begin(), sr_levels.end()); // Re-sort for consistent drawing
    }

    // Draw S/R levels
    SDL_SetRenderDrawColor(ren, COL_SR_LEVEL.r, COL_SR_LEVEL.g,
                           COL_SR_LEVEL.b, COL_SR_LEVEL.a);
    for (double level : sr_levels) {
        int y = cr.toY(level);
        // Only draw if within chart bounds
        if (y >= cr.cT && y <= cr.cB) {
            drawDashedHLine(ren, cr.cL, cr.cR, y);

            std::ostringstream oss;
            oss << "S/R " << std::fixed << std::setprecision(2) << level;
            drawText(ren, fontSm, oss.str(), cr.cR + 4, y, COL_SR_LABEL, 0, 1);
        }
    }

    // TODO: Further enhancements for Price Action:
    // - Implement more robust S/R identification (e.g., based on volume, longer timeframes)
    // - Draw trend lines
    // - Recognize candlestick patterns
    // - Display chart patterns
    // - Improve filtering and prioritization of S/R levels
}
