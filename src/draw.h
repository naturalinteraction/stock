#pragma once

#include "chart.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <string>

// Find a TrueType font file on the system
std::string findFont();

// Calculate nice step size for grid intervals
double niceStep(double range, int target);

// Draw text with alignment
// alignX: 0=left  1=centre  2=right
// alignY: 0=top   1=centre
void drawText(SDL_Renderer* r, TTF_Font* f, const std::string& text,
              int x, int y, RGBA col, int ax, int ay);

// Convert YYYY-MM-DD date to short format (e.g. "Jan 01")
std::string shortDate(const std::string& ymd);

// Draw a thick line (3 pixels wide)
void thickLine(SDL_Renderer* r, int x1, int y1, int x2, int y2);

// Draw a horizontal dashed line
void drawDashedHLine(SDL_Renderer* ren, int x1, int x2, int y,
                     int dashLen, int gapLen);
