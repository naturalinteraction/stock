#pragma once

#include "chart.h"

void renderLinearOverlay(SDL_Renderer* ren, TTF_Font* fontSm,
                         const std::vector<PricePoint>& price_history,
                         const ChartRegion& cr);
