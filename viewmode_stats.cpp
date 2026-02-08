#include "viewmode_stats.h"

#include <cmath>
#include <iomanip>
#include <sstream>
#include <string>

static constexpr RGBA COL_MEAN  = {255, 200,  50, 200};
static constexpr RGBA COL_SIGMA = {255, 130,  50, 160};

void renderStatsOverlay(SDL_Renderer* ren, TTF_Font* fontSm,
                        const std::vector<PricePoint>& price_history,
                        const ChartRegion& cr) {
    int N   = cr.N;
    int off = cr.displayStart;
    if (N <= 0) return;

    double sum = 0;
    for (int i = 0; i < N; ++i) sum += price_history[off + i].price;
    double mean = sum / N;

    double sq = 0;
    for (int i = 0; i < N; ++i) {
        double d = price_history[off + i].price - mean;
        sq += d * d;
    }
    double sigma = std::sqrt(sq / N);

    struct StatLine { double val; RGBA col; std::string label; };
    StatLine lines[] = {
        { mean + sigma, COL_SIGMA, "+s" },
        { mean,         COL_MEAN,  "m " },
        { mean - sigma, COL_SIGMA, "-s" },
    };
    for (auto& sl : lines) {
        int y = cr.toY(sl.val);
        if (y < cr.cT || y > cr.cB) continue;
        SDL_SetRenderDrawColor(ren, sl.col.r, sl.col.g, sl.col.b, sl.col.a);
        drawDashedHLine(ren, cr.cL, cr.cR, y);
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(2) << sl.val;
        std::string lbl = sl.label + " " + oss.str();
        drawText(ren, fontSm, lbl, cr.cR + 4, y, sl.col, 0, 1);
    }
}
