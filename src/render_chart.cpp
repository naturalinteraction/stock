#include "render_chart.h"
#include "draw.h"
#include "ui.h"
#include "viewmode_stats.h"
#include "viewmode_bollinger.h"
#include "viewmode_macross.h"

#include <algorithm>
#include <cstdio>
#include <cmath>
#include <iomanip>
#include <limits>
#include <sstream>

std::string findFont() {
    static const char* paths[] = {
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/TTF/DejaVuSans.ttf",
        "/usr/share/fonts/dejavu-sans-fonts/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
        "/usr/share/fonts/liberation-sans/LiberationSans-Regular.ttf",
        "/usr/share/fonts/truetype/ubuntu/Ubuntu-R.ttf",
        "/usr/share/fonts/truetype/freefont/FreeSans.ttf",
        "/usr/share/fonts/truetype/noto/NotoSans-Regular.ttf",
        "/usr/share/fonts/google-noto/NotoSans-Regular.ttf",
        "/usr/share/fonts/noto/NotoSans-Regular.ttf",
    };
    for (auto* p : paths) {
        if (FILE* f = fopen(p, "r")) { fclose(f); return p; }
    }
    return {};
}

void renderChart(SDL_Renderer* ren, TTF_Font* font, TTF_Font* fontSm,
                 const std::vector<PricePoint>& price_history,
                 const std::vector<std::string>& tickers, int currentTickerIndex,
                 ViewMode viewMode, int displayDays, const WindowDimensions& winDim) {
    // Clear
    SDL_SetRenderDrawColor(ren, ChartColors::BG.r, ChartColors::BG.g, ChartColors::BG.b, 255);
    SDL_RenderClear(ren);

    const int cL = winDim.marginLeft;
    const int cR = winDim.width - winDim.marginRight;
    const int cT = winDim.marginTop;
    const int cB = winDim.height - winDim.marginBottom;
    const int cW = cR - cL;
    const int cH = cB - cT;

    // Clip drawing to chart area
    SDL_Rect clipRect = {cL, cT, cW + 1, cH + 1}; // Expand clipping region by 1 pixel to include borders
    SDL_RenderSetClipRect(ren, &clipRect);

    int total = static_cast<int>(price_history.size());
    int dispN = std::min(total, displayDays);
    int off   = total - dispN;          // lookback data lives at 0..off-1

    // Reset mouse in chart area flag for this render cycle
    setMouseInChartArea(false);

    if (price_history.empty()) {
        drawText(ren, font, "No data available",
                 winDim.width / 2, winDim.height / 2, ChartColors::TEXT, 1, 1);
        return;
    }

    // Price range (displayed points only)
    double lo = price_history[off].price, hi = lo;
    for (int i = off; i < total; ++i) {
        lo = std::min(lo, price_history[i].price);
        hi = std::max(hi, price_history[i].price);
    }
    double range = hi - lo;
    if (range < 0.01) range = 1.0;
    double pad = range * 0.08;
    lo -= pad;
    hi += pad;

    double step = niceStep(hi - lo, 8);
    double gMin = std::floor(lo / step) * step;
    double gMax = std::ceil(hi  / step) * step;
    range = gMax - gMin;

    // toY: price → pixel Y
    auto toY = [&](double price) -> int {
        return cB - static_cast<int>((price - gMin) / range * cH);
    };
    // toX: display index (0..dispN-1) → pixel X
    auto toX = [&](int di) -> int {
        return (dispN <= 1) ? cL + cW / 2
                            : cL + static_cast<int>(double(di) / (dispN - 1) * cW);
    };

    // ── Horizontal grid + price labels ──
    SDL_SetRenderDrawColor(ren, ChartColors::GRID.r, ChartColors::GRID.g, ChartColors::GRID.b, ChartColors::GRID.a);
    for (double p = gMin; p <= gMax + step * 0.001; p += step) {
        int y = toY(p);
        if (y < cT || y > cB) continue;
        SDL_RenderDrawLine(ren, cL, y, cR, y);
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(step >= 1.0 ? 1 : 2) << p;
        drawText(ren, fontSm, oss.str(), cL - 8, y, ChartColors::TEXT, 2, 1);
    }

    // ── Vertical grid + date labels ──
    int maxLabels = cW / 75;
    int labelStep = std::max(1, (dispN - 1) / std::max(1, maxLabels));
    for (int di = 0; di < dispN; di += labelStep) {
        int x = toX(di);
        SDL_SetRenderDrawColor(ren, ChartColors::GRID.r, ChartColors::GRID.g, ChartColors::GRID.b, ChartColors::GRID.a);
        SDL_RenderDrawLine(ren, x, cT, x, cB);
        drawText(ren, fontSm, shortDate(price_history[off + di].date), x, cB + 8, ChartColors::TEXT, 1, 0);
    }
    // Always draw rightmost vertical line if the loop didn't land on it
    {
        int lastDi = dispN - 1;
        if (lastDi % labelStep != 0) {
            int x = toX(lastDi);
            SDL_SetRenderDrawColor(ren, ChartColors::GRID.r, ChartColors::GRID.g, ChartColors::GRID.b, ChartColors::GRID.a);
            SDL_RenderDrawLine(ren, x, cT, x, cB);
            drawText(ren, fontSm, shortDate(price_history[off + lastDi].date), x, cB + 8, ChartColors::TEXT, 1, 0);
        }
    }

    // ── Axes ──
    SDL_SetRenderDrawColor(ren, ChartColors::AXIS.r, ChartColors::AXIS.g, ChartColors::AXIS.b, ChartColors::AXIS.a);
    SDL_RenderDrawLine(ren, cL, cT, cL, cB);
    SDL_RenderDrawLine(ren, cL, cB, cR, cB);

    // ── Filled area under line ──
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(ren, ChartColors::FILL.r, ChartColors::FILL.g, ChartColors::FILL.b, ChartColors::FILL.a);
    for (int di = 0; di < dispN; ++di) {
        int x = toX(di), y = toY(price_history[off + di].price);
        SDL_RenderDrawLine(ren, x, y, x, cB);
    }

    // ── Price line ──
    SDL_SetRenderDrawColor(ren, ChartColors::LINE.r, ChartColors::LINE.g, ChartColors::LINE.b, ChartColors::LINE.a);
    for (int di = 1; di < dispN; ++di)
        thickLine(ren, toX(di - 1), toY(price_history[off + di - 1].price),
                       toX(di),     toY(price_history[off + di].price));

    // ── Data dots ──
    SDL_SetRenderDrawColor(ren, ChartColors::DOT.r, ChartColors::DOT.g, ChartColors::DOT.b, ChartColors::DOT.a);
    for (int di = 0; di < dispN; ++di) {
        int x = toX(di), y = toY(price_history[off + di].price);
        SDL_Rect dot = {x - 2, y - 2, 5, 5};
        SDL_RenderFillRect(ren, &dot);
    }

    // ── View-mode overlays ──
    {
        ChartRegion cr{cL, cR, cT, cB, cW, cH, dispN, off, toY, toX};
        if (viewMode == ViewMode::PriceChartStats)
            renderStatsOverlay(ren, fontSm, price_history, cr);
        else if (viewMode == ViewMode::Bollinger)
            renderBollingerOverlay(ren, fontSm, price_history, cr);
        else if (viewMode == ViewMode::MACross)
            renderMACrossOverlay(ren, fontSm, price_history, cr);

    }

    // ── Last-price annotation ──
    {
        const auto& last = price_history.back();
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(2) << last.price;
        int x = toX(dispN - 1), y = toY(last.price);
        // dashed tick on right edge
        SDL_SetRenderDrawColor(ren, ChartColors::DOT.r, ChartColors::DOT.g, ChartColors::DOT.b, ChartColors::DOT.a);
        for (int dx = x; dx <= cR; dx += 4)
            SDL_RenderDrawPoint(ren, dx, y);
        drawText(ren, fontSm, oss.str(), cR + 4, y, ChartColors::DOT, 0, 1);
    }

    // Remove clip for labels drawn outside chart area
    SDL_RenderSetClipRect(ren, nullptr);

    // View mode rectangles and ticker buttons (drawn outside clipping region)
    const int barTopY = 5;
    int rightEdge = renderViewModeBar(ren, fontSm, viewMode, barTopY, cL, ChartColors::BG);

    // Ticker buttons (right-aligned to chart edge)
    renderTickerBar(ren, fontSm, tickers, currentTickerIndex, barTopY, cR, ChartColors::BG);

    // Trading days info (to the right of view mode buttons)
    std::string tickerInfo = std::to_string(dispN) + " Trading Days";
    drawText(ren, fontSm, tickerInfo, rightEdge + 20, barTopY + 15, ChartColors::TEXT, 0, 1);

    // ── Instructions ──
    drawText(ren, fontSm, "TAB: switch view | R: reload | F: fullscreen | UpDownKeys/Mouse Wheel: zoom | ESC: quit",
             winDim.width / 2, winDim.height - 16, ChartColors::GRID, 1, 1);

    // ── Mouse hover label ──
    if (getMouseX() >= cL && getMouseX() <= cR && getMouseY() >= cT && getMouseY() <= cB) {
        setMouseInChartArea(true);
        // Find the closest price point on the X axis to the mouse cursor
        int closest_di = -1;
        int min_dist = std::numeric_limits<int>::max();

        // Iterate only through displayed points
        for (int di = 0; di < dispN; ++di) {
            int x_coord = toX(di);
            int dist = std::abs(x_coord - getMouseX());
            if (dist < min_dist) {
                min_dist = dist;
                closest_di = di;
            }
        }

        if (closest_di != -1) {
            int hovered_real_idx = off + closest_di;
            if (hovered_real_idx >= 0 && hovered_real_idx < total) {
                const PricePoint& hovered_pp = price_history[hovered_real_idx];
                int hover_x = toX(closest_di);
                int hover_y = toY(hovered_pp.price);

                // Price label
                std::ostringstream price_oss;
                price_oss << std::fixed << std::setprecision(2) << hovered_pp.price;
                std::string price_str = price_oss.str();

                // Date label (formatted as at the bottom of the graph)
                std::string formatted_date = shortDate(hovered_pp.date);

                // Get text dimensions
                int price_w, price_h;
                TTF_SizeText(fontSm, price_str.c_str(), &price_w, &price_h);
                int date_w, date_h;
                TTF_SizeText(fontSm, formatted_date.c_str(), &date_w, &date_h);

                int label_padding = 5;
                int label_width = std::max(price_w, date_w) + label_padding * 2;
                int label_height = price_h + date_h + label_padding * 3; // 2 lines + 3 paddings (top, middle, bottom)

                // Calculate position for the background rectangle
                // Offset it slightly from the hover_x, hover_y
                int bg_x = hover_x + 10;
                int bg_y = hover_y - 30 - label_padding; // Start above the first line of text

                // Ensure label stays within screen bounds (right edge)
                if (bg_x + label_width > winDim.width) {
                    bg_x = winDim.width - label_width - 5; // 5 pixels margin from right edge
                }
                // Ensure label stays within screen bounds (top edge)
                if (bg_y < cT) {
                    bg_y = cT + 5; // 5 pixels margin from top edge
                }

                SDL_Rect bg_rect = {bg_x, bg_y, label_width, label_height};

                // Draw semi-transparent background
                SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
                SDL_SetRenderDrawColor(ren, ChartColors::LABEL_BG.r, ChartColors::LABEL_BG.g,
                                       ChartColors::LABEL_BG.b, ChartColors::LABEL_BG.a);
                SDL_RenderFillRect(ren, &bg_rect);

                // Draw price text
                drawText(ren, fontSm, price_str, bg_x + label_padding, bg_y + label_padding, ChartColors::HOVER_LABEL, 0, 0);

                // Draw date text
                drawText(ren, fontSm, formatted_date, bg_x + label_padding, bg_y + label_padding + price_h + label_padding, ChartColors::HOVER_LABEL, 0, 0);

                // Draw a small dot or circle on the hovered point for better visibility
                SDL_SetRenderDrawColor(ren, ChartColors::HOVER_LABEL.r, ChartColors::HOVER_LABEL.g, ChartColors::HOVER_LABEL.b, ChartColors::HOVER_LABEL.a);
                SDL_Rect dot = {hover_x - 3, hover_y - 3, 7, 7};
                SDL_RenderDrawRect(ren, &dot);
            }
        }
    } else {
        setMouseInChartArea(false);
    }
}
