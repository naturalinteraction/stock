#pragma once

#include "chart.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <string>
#include <vector>

// UI color palette
namespace UIColors {
    static constexpr RGBA VIEWMODE_BG     = {35, 35, 50, 255};      // Inactive tab
    static constexpr RGBA VIEWMODE_ACTIVE = {130, 130, 150, 255};   // Active tab
    static constexpr RGBA VIEWMODE_TEXT   = {180, 180, 200, 255};   // Text inside rectangles
}

// Get human-readable name for a view mode
std::string getViewModeName(ViewMode mode);

// Check if mouse click is within any view mode tab
// Returns true and sets outMode if clicked, false otherwise
bool getClickedViewMode(int mouseX, int mouseY, ViewMode& outMode);

// Check if mouse click is within any ticker button
// Returns true and sets outTickerIndex if clicked, false otherwise
bool getClickedTicker(int mouseX, int mouseY, int& outTickerIndex);

// Render view mode tab bar at the top
// Returns the right edge position of the rendered tabs
int renderViewModeBar(SDL_Renderer* ren, TTF_Font* font, ViewMode currentMode,
                      int topY, int leftMargin, RGBA bgColor);

// Render ticker button bar on the right side
void renderTickerBar(SDL_Renderer* ren, TTF_Font* font, const std::vector<std::string>& tickers,
                     int currentTickerIndex, int topY, int rightMargin, RGBA bgColor);

// Access mouse state (for hover detection)
int getMouseX();
int getMouseY();
bool isMouseInChartArea();
void setMouseInChartArea(bool inChart);

// Set mouse position (called from event handling)
void setMousePosition(int x, int y);
