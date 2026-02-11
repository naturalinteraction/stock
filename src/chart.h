#pragma once

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

#include <algorithm>
#include <functional>
#include <string>
#include <vector>

// ─── Layout constants ───
constexpr int DEFAULT_GRAPH_WIDTH   = 1200;
constexpr int DEFAULT_GRAPH_HEIGHT  = 700;
constexpr int MIN_GRAPH_WIDTH       = 800;
constexpr int MIN_GRAPH_HEIGHT      = 500;

// Margin ratios (percentage of window size)
constexpr double MARGIN_LEFT_RATIO    = 0.075;    // 90/1200
constexpr double MARGIN_RIGHT_RATIO   = 0.06;     // 70/1200
constexpr double MARGIN_TOP_RATIO     = 0.07;     // 50/700
constexpr double MARGIN_BOTTOM_RATIO  = 0.11;     // 80/700

// Minimum margin values
constexpr int MIN_MARGIN_LEFT   = 90;
constexpr int MIN_MARGIN_RIGHT  = 70;
constexpr int MIN_MARGIN_TOP    = 50;
constexpr int MIN_MARGIN_BOTTOM = 80;

// ─── Colour type ───
struct RGBA { Uint8 r, g, b, a; };

// ─── Window dimensions structure ───
struct WindowDimensions {
    int width;
    int height;
    int marginLeft;
    int marginRight;
    int marginTop;
    int marginBottom;

    static WindowDimensions calculate(int w, int h) {
        WindowDimensions wd;
        wd.width = w;
        wd.height = h;
        wd.marginLeft = std::max(MIN_MARGIN_LEFT,
                                static_cast<int>(w * MARGIN_LEFT_RATIO));
        wd.marginRight = std::max(MIN_MARGIN_RIGHT,
                                 static_cast<int>(w * MARGIN_RIGHT_RATIO));
        wd.marginTop = std::max(MIN_MARGIN_TOP,
                               static_cast<int>(h * MARGIN_TOP_RATIO));
        wd.marginBottom = std::max(MIN_MARGIN_BOTTOM,
                                  static_cast<int>(h * MARGIN_BOTTOM_RATIO));
        return wd;
    }
};

// ─── Data ───
struct PricePoint {
    double      price;
    std::string date;   // "YYYY-MM-DD"
};

// ─── View modes ───
enum class ViewMode { Trend, Bollinger, MACross, _COUNT };
constexpr int VIEW_MODE_COUNT = static_cast<int>(ViewMode::_COUNT);
constexpr int LOOKBACK_DAYS   = 21;

// ─── Chart region (precomputed from margins) ───
struct ChartRegion {
    int cL, cR, cT, cB, cW, cH;
    int N;                          // displayed points
    int displayStart;               // index into price_history of first displayed point
    std::function<int(double)> toY;
    std::function<int(int)>    toX; // maps display index (0..N-1) to pixel X
};

// ─── Shared drawing helpers ───
void drawText(SDL_Renderer* r, TTF_Font* f, const std::string& text,
              int x, int y, RGBA col, int ax = 0, int ay = 0);

void drawDashedHLine(SDL_Renderer* ren, int x1, int x2, int y,
                     int dashLen = 6, int gapLen = 4);

void thickLine(SDL_Renderer* r, int x1, int y1, int x2, int y2);

// ─── Chart rendering color palette ───
namespace ChartColors {
    static constexpr RGBA BG          = { 18,  18,  40, 255};  // Background
    static constexpr RGBA GRID        = { 45,  45,  75, 255};  // Grid lines
    static constexpr RGBA AXIS        = { 90,  90, 130, 255};  // Axis lines
    static constexpr RGBA LINE        = {  0, 200, 100, 255};  // Price line
    static constexpr RGBA FILL        = {  0, 200, 100,  30};  // Area under line
    static constexpr RGBA TEXT        = {180, 180, 200, 255};  // Labels
    static constexpr RGBA TITLE       = {240, 240, 255, 255};  // Title
    static constexpr RGBA DOT         = {  0, 255, 140, 255};  // Data points
    static constexpr RGBA HOVER_LABEL = {255, 255, 255, 255};  // Hover label text
    static constexpr RGBA LABEL_BG    = {0, 0, 0, 30};         // Hover label background
}

// ─── Chart rendering functions ───
void renderChart(SDL_Renderer* ren, TTF_Font* font, TTF_Font* fontSm,
                 const std::vector<PricePoint>& price_history,
                 const std::vector<std::string>& tickers, int currentTickerIndex,
                 ViewMode viewMode, int displayDays, const WindowDimensions& winDim);
