#pragma once

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

#include <functional>
#include <string>
#include <vector>

// ─── Layout constants ───
constexpr int GRAPH_WIDTH   = 1200;
constexpr int GRAPH_HEIGHT  = 700;
constexpr int MARGIN_LEFT   = 90;
constexpr int MARGIN_RIGHT  = 70;
constexpr int MARGIN_TOP    = 50;
constexpr int MARGIN_BOTTOM = 80;

// ─── Colour type ───
struct RGBA { Uint8 r, g, b, a; };

// ─── Data ───
struct PricePoint {
    double      price;
    std::string date;   // "YYYY-MM-DD"
};

// ─── View modes ───
enum class ViewMode { PriceChart, PriceChartStats, Bollinger, MACross, PriceAction };
constexpr int VIEW_MODE_COUNT = 5;
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
