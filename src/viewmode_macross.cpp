#include "viewmode_macross.h"

#include <cmath>
#include <iomanip>
#include <sstream>

static constexpr int SMA_SHORT = 9;
static constexpr int SMA_LONG  = 21;

static constexpr RGBA COL_SMA_SHORT = {255, 220,  50, 220};   // yellow
static constexpr RGBA COL_SMA_LONG  = {255, 100, 100, 220};   // red
static constexpr RGBA COL_BUY       = {  0, 255, 100, 255};   // green dot
static constexpr RGBA COL_SELL      = {255,  60,  60, 255};   // red dot

static double sma(const std::vector<PricePoint>& ph, int end, int period) {
    double sum = 0;
    for (int j = end - period + 1; j <= end; ++j)
        sum += ph[j].price;
    return sum / period;
}

void renderMACrossOverlay(SDL_Renderer* ren, TTF_Font* fontSm,
                          const std::vector<PricePoint>& price_history,
                          const ChartRegion& cr) {
    int dispN = cr.N;
    int off   = cr.displayStart;
    int total = static_cast<int>(price_history.size());
    if (dispN <= 0) return;

    // Clip drawing to chart area
    SDL_Rect clipRect = {cr.cL, cr.cT, cr.cW, cr.cH};
    SDL_RenderSetClipRect(ren, &clipRect);

    // Compute both SMAs for each displayed point
    struct MA { double s, l; bool validS, validL; };
    std::vector<MA> ma(dispN);

    for (int di = 0; di < dispN; ++di) {
        int ri = off + di;
        ma[di].validS = (ri >= SMA_SHORT - 1 && ri < total);
        ma[di].validL = (ri >= SMA_LONG  - 1 && ri < total);
        if (ma[di].validS) ma[di].s = sma(price_history, ri, SMA_SHORT);
        if (ma[di].validL) ma[di].l = sma(price_history, ri, SMA_LONG);
    }

    // Draw SMA long line
    SDL_SetRenderDrawColor(ren, COL_SMA_LONG.r, COL_SMA_LONG.g,
                           COL_SMA_LONG.b, COL_SMA_LONG.a);
    for (int di = 1; di < dispN; ++di) {
        if (!ma[di - 1].validL || !ma[di].validL) continue;
        SDL_RenderDrawLine(ren, cr.toX(di - 1), cr.toY(ma[di - 1].l),
                                cr.toX(di),     cr.toY(ma[di].l));
    }

    // Draw SMA short line (thicker, on top)
    SDL_SetRenderDrawColor(ren, COL_SMA_SHORT.r, COL_SMA_SHORT.g,
                           COL_SMA_SHORT.b, COL_SMA_SHORT.a);
    for (int di = 1; di < dispN; ++di) {
        if (!ma[di - 1].validS || !ma[di].validS) continue;
        thickLine(ren, cr.toX(di - 1), cr.toY(ma[di - 1].s),
                       cr.toX(di),     cr.toY(ma[di].s));
    }

    // Detect crossovers and draw signal dots
    for (int di = 1; di < dispN; ++di) {
        if (!ma[di - 1].validS || !ma[di - 1].validL) continue;
        if (!ma[di].validS     || !ma[di].validL)     continue;

        double prevDiff = ma[di - 1].s - ma[di - 1].l;
        double currDiff = ma[di].s     - ma[di].l;

        if (prevDiff <= 0 && currDiff > 0) {
            // Golden cross — buy signal
            SDL_SetRenderDrawColor(ren, COL_BUY.r, COL_BUY.g,
                                   COL_BUY.b, COL_BUY.a);
            int x = cr.toX(di), y = cr.toY(price_history[off + di].price);
            SDL_Rect dot = {x - 5, y - 5, 11, 11};
            SDL_RenderFillRect(ren, &dot);
        } else if (prevDiff >= 0 && currDiff < 0) {
            // Death cross — sell signal
            SDL_SetRenderDrawColor(ren, COL_SELL.r, COL_SELL.g,
                                   COL_SELL.b, COL_SELL.a);
            int x = cr.toX(di), y = cr.toY(price_history[off + di].price);
            SDL_Rect dot = {x - 5, y - 5, 11, 11};
            SDL_RenderFillRect(ren, &dot);
        }
    }

    // Remove clip for labels drawn outside chart area
    SDL_RenderSetClipRect(ren, nullptr);

    // Labels on the right edge
    if (ma[dispN - 1].validS) {
        std::ostringstream oss;
        oss << "S9 " << std::fixed << std::setprecision(2) << ma[dispN - 1].s;
        drawText(ren, fontSm, oss.str(), cr.cR + 4,
                 cr.toY(ma[dispN - 1].s), COL_SMA_SHORT, 0, 1);
    }
    if (ma[dispN - 1].validL) {
        std::ostringstream oss;
        oss << "S21 " << std::fixed << std::setprecision(2) << ma[dispN - 1].l;
        drawText(ren, fontSm, oss.str(), cr.cR + 4,
                 cr.toY(ma[dispN - 1].l), COL_SMA_LONG, 0, 1);
    }
}
