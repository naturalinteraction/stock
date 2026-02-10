#pragma once

#include "chart.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <string>
#include <vector>

// Chart rendering color palette
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

// Find a TrueType font file on the system
std::string findFont();

// Render the main chart with all overlays and UI elements
void renderChart(SDL_Renderer* ren, TTF_Font* font, TTF_Font* fontSm,
                 const std::vector<PricePoint>& price_history,
                 const std::vector<std::string>& tickers, int currentTickerIndex,
                 ViewMode viewMode, int displayDays, const WindowDimensions& winDim);
