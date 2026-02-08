#pragma once

#include "chart.h"

void renderBollingerOverlay(SDL_Renderer* ren, TTF_Font* fontSm,
                            const std::vector<PricePoint>& price_history,
                            const ChartRegion& cr);
