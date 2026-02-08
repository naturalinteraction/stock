#pragma once

#include "chart.h"

// Forward declaration for the Price Action rendering function.
// This function will be responsible for drawing Price Action related
// elements on the chart.
void renderPriceActionOverlay(SDL_Renderer* ren, TTF_Font* fontSm,
                              const std::vector<PricePoint>& price_history,
                              const ChartRegion& cr);
