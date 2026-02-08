#include "viewmode_bollinger.h"

#include <cmath>
#include <iomanip>
#include <sstream>

static constexpr int    BB_PERIOD = 20;
static constexpr double BB_K      = 2.0;

static constexpr RGBA COL_BB_MID  = {100, 180, 255, 200};   // SMA line
static constexpr RGBA COL_BB_BAND = {100, 180, 255,  90};   // upper/lower bands
static constexpr RGBA COL_BB_FILL = {100, 180, 255,  25};   // shaded area

void renderBollingerOverlay(SDL_Renderer* ren, TTF_Font* fontSm,
                            const std::vector<PricePoint>& price_history,
                            const ChartRegion& cr) {
    int dispN = cr.N;
    int off   = cr.displayStart;
    int total = static_cast<int>(price_history.size());
    if (dispN <= 0) return;

    // Compute bands for every displayed point using full lookback data.
    // For display index di, the real index is off + di.
    // We need BB_PERIOD points ending at off+di, i.e. real indices
    // [off+di - BB_PERIOD+1 .. off+di], which must be >= 0.
    struct Band { double upper, mid, lower; bool valid; };
    std::vector<Band> bands(dispN);

    for (int di = 0; di < dispN; ++di) {
        int ri = off + di;                       // real index
        int start = ri - BB_PERIOD + 1;
        if (start < 0 || ri >= total) {
            bands[di] = {0, 0, 0, false};
            continue;
        }
        double sum = 0;
        for (int j = start; j <= ri; ++j)
            sum += price_history[j].price;
        double sma = sum / BB_PERIOD;

        double sq = 0;
        for (int j = start; j <= ri; ++j) {
            double d = price_history[j].price - sma;
            sq += d * d;
        }
        double sd = std::sqrt(sq / BB_PERIOD);
        bands[di] = { sma + BB_K * sd, sma, sma - BB_K * sd, true };
    }

    // Shaded fill between upper and lower bands
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(ren, COL_BB_FILL.r, COL_BB_FILL.g,
                           COL_BB_FILL.b, COL_BB_FILL.a);
    for (int di = 0; di < dispN; ++di) {
        if (!bands[di].valid) continue;
        int x  = cr.toX(di);
        int yU = cr.toY(bands[di].upper);
        int yL = cr.toY(bands[di].lower);
        SDL_RenderDrawLine(ren, x, yU, x, yL);
    }

    // Upper band line
    SDL_SetRenderDrawColor(ren, COL_BB_BAND.r, COL_BB_BAND.g,
                           COL_BB_BAND.b, COL_BB_BAND.a);
    for (int di = 1; di < dispN; ++di) {
        if (!bands[di - 1].valid || !bands[di].valid) continue;
        SDL_RenderDrawLine(ren, cr.toX(di - 1), cr.toY(bands[di - 1].upper),
                                cr.toX(di),     cr.toY(bands[di].upper));
    }

    // Lower band line
    for (int di = 1; di < dispN; ++di) {
        if (!bands[di - 1].valid || !bands[di].valid) continue;
        SDL_RenderDrawLine(ren, cr.toX(di - 1), cr.toY(bands[di - 1].lower),
                                cr.toX(di),     cr.toY(bands[di].lower));
    }

    // SMA middle line (thicker)
    SDL_SetRenderDrawColor(ren, COL_BB_MID.r, COL_BB_MID.g,
                           COL_BB_MID.b, COL_BB_MID.a);
    for (int di = 1; di < dispN; ++di) {
        if (!bands[di - 1].valid || !bands[di].valid) continue;
        thickLine(ren, cr.toX(di - 1), cr.toY(bands[di - 1].mid),
                       cr.toX(di),     cr.toY(bands[di].mid));
    }

    // Labels on the right edge
    const Band& last = bands[dispN - 1];
    if (!last.valid) return;
    struct LabelInfo { double val; RGBA col; const char* tag; };
    LabelInfo labels[] = {
        { last.upper, COL_BB_BAND, "+2s" },
        { last.mid,   COL_BB_MID,  "SMA" },
        { last.lower, COL_BB_BAND, "-2s" },
    };
    for (auto& lb : labels) {
        int y = cr.toY(lb.val);
        if (y < cr.cT || y > cr.cB) continue;
        std::ostringstream oss;
        oss << lb.tag << " " << std::fixed << std::setprecision(2) << lb.val;
        drawText(ren, fontSm, oss.str(), cr.cR + 4, y, lb.col, 0, 1);
    }
}
