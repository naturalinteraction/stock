#include "viewmode_linear.h"

#include <cmath>
#include <iomanip>
#include <sstream>
#include <string>

static constexpr RGBA COL_LINEAR = {255, 180, 50, 220};   // Orange main linear line
static constexpr RGBA COL_BAND   = {255, 180, 50, 140};   // Orange confidence bands

void renderLinearOverlay(SDL_Renderer* ren, TTF_Font* fontSm,
                        const std::vector<PricePoint>& price_history,
                        const ChartRegion& cr) {
    int N   = cr.N;
    int off = cr.displayStart;

    // Need at least 2 points for linear regression
    if (N <= 1) return;

    // Clip drawing to chart area
    SDL_Rect clipRect = {cr.cL, cr.cT, cr.cW, cr.cH};
    SDL_RenderSetClipRect(ren, &clipRect);

    // Calculate linear regression: y = m*x + offset
    // Where x is the display index (0 to N-1)

    double mean_x = (N - 1) / 2.0;
    double sum_y = 0;
    for (int i = 0; i < N; ++i) {
        sum_y += price_history[off + i].price;
    }
    double mean_y = sum_y / N;

    // Calculate slope (m)
    double numerator = 0, denominator = 0;
    for (int i = 0; i < N; ++i) {
        double dx = i - mean_x;
        double dy = price_history[off + i].price - mean_y;
        numerator += dx * dy;
        denominator += dx * dx;
    }

    // Check for zero denominator (flat horizontal line case)
    double m = 0;
    if (std::abs(denominator) > 1e-10) {
        m = numerator / denominator;
    }

    // Calculate intercept
    double offset = mean_y - m * mean_x;

    // Calculate standard deviation of residuals
    double sum_sq_residuals = 0;
    for (int i = 0; i < N; ++i) {
        double fitted = m * i + offset;
        double residual = price_history[off + i].price - fitted;
        sum_sq_residuals += residual * residual;
    }
    double sigma = std::sqrt(sum_sq_residuals / N);

    // Calculate R² (coefficient of determination)
    double ss_tot = 0;
    for (int i = 0; i < N; ++i) {
        double dy = price_history[off + i].price - mean_y;
        ss_tot += dy * dy;
    }
    double r_squared = (std::abs(ss_tot) > 1e-10) ? (1.0 - (sum_sq_residuals / ss_tot)) : 0.0;

    // Draw shaded area between bands (using vertical lines)
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(ren, 255, 180, 50, 20);  // Very transparent orange
    for (int i = 0; i < N; ++i) {
        double fitted_upper = m * i + offset + sigma;
        double fitted_lower = m * i + offset - sigma;
        int y_upper = cr.toY(fitted_upper);
        int y_lower = cr.toY(fitted_lower);
        int x = cr.toX(i);

        // Draw vertical line between upper and lower band at this x position
        if (y_lower > y_upper) {
            SDL_RenderDrawLine(ren, x, y_upper, x, y_lower);
        }
    }

    // Draw upper confidence band (trend + sigma) - dashed
    SDL_SetRenderDrawColor(ren, COL_BAND.r, COL_BAND.g, COL_BAND.b, COL_BAND.a);
    for (int i = 0; i < N - 1; ++i) {
        double fitted_curr = m * i + offset + sigma;
        double fitted_next = m * (i + 1) + offset + sigma;
        int x1 = cr.toX(i);
        int y1 = cr.toY(fitted_curr);
        int x2 = cr.toX(i + 1);
        int y2 = cr.toY(fitted_next);

        // Draw dashes (4 pixels) with gaps (4 pixels)
        int dx = x2 - x1;
        int dy = y2 - y1;
        int steps = 4;  // Number of dashes per segment

        for (int d = 0; d < steps; ++d) {
            double t1 = (d * 2.0) / (steps * 2.0);
            double t2 = (d * 2.0 + 1.0) / (steps * 2.0);
            if (t2 > 1.0) t2 = 1.0;

            int px1 = x1 + static_cast<int>(dx * t1);
            int py1 = y1 + static_cast<int>(dy * t1);
            int px2 = x1 + static_cast<int>(dx * t2);
            int py2 = y1 + static_cast<int>(dy * t2);

            SDL_RenderDrawLine(ren, px1, py1, px2, py2);
        }
    }

    // Draw lower confidence band (trend - sigma) - dashed
    SDL_SetRenderDrawColor(ren, COL_BAND.r, COL_BAND.g, COL_BAND.b, COL_BAND.a);
    for (int i = 0; i < N - 1; ++i) {
        double fitted_curr = m * i + offset - sigma;
        double fitted_next = m * (i + 1) + offset - sigma;
        int x1 = cr.toX(i);
        int y1 = cr.toY(fitted_curr);
        int x2 = cr.toX(i + 1);
        int y2 = cr.toY(fitted_next);

        // Draw dashes (4 pixels) with gaps (4 pixels)
        int dx = x2 - x1;
        int dy = y2 - y1;
        int steps = 4;  // Number of dashes per segment

        for (int d = 0; d < steps; ++d) {
            double t1 = (d * 2.0) / (steps * 2.0);
            double t2 = (d * 2.0 + 1.0) / (steps * 2.0);
            if (t2 > 1.0) t2 = 1.0;

            int px1 = x1 + static_cast<int>(dx * t1);
            int py1 = y1 + static_cast<int>(dy * t1);
            int px2 = x1 + static_cast<int>(dx * t2);
            int py2 = y1 + static_cast<int>(dy * t2);

            SDL_RenderDrawLine(ren, px1, py1, px2, py2);
        }
    }

    // Draw main linear line (thick)
    SDL_SetRenderDrawColor(ren, COL_LINEAR.r, COL_LINEAR.g, COL_LINEAR.b, COL_LINEAR.a);
    for (int i = 1; i < N; ++i) {
        double fitted_prev = m * (i - 1) + offset;
        double fitted_curr = m * i + offset;
        thickLine(ren, cr.toX(i - 1), cr.toY(fitted_prev),
                       cr.toX(i),     cr.toY(fitted_curr));
    }

    // Remove clip for labels drawn outside chart area
    SDL_RenderSetClipRect(ren, nullptr);

    // Draw labels on right edge
    struct LinearLabel { double val; RGBA col; std::string label; };
    LinearLabel labels[] = {
        { m * (N - 1) + offset + sigma, COL_BAND, "+s" },
        { m * (N - 1) + offset,         COL_LINEAR, "linear" },
        { m * (N - 1) + offset - sigma, COL_BAND, "-s" },
    };

    for (auto& tl : labels) {
        int y = cr.toY(tl.val);
        if (y < cr.cT || y > cr.cB) continue;

        std::ostringstream oss;
        oss << std::fixed << std::setprecision(2) << tl.val;
        std::string lbl = tl.label + " " + oss.str();
        drawText(ren, fontSm, lbl, cr.cR + 4, y, tl.col, 0, 1);
    }

    // Draw slope and R² at bottom right
    std::ostringstream slope_oss;
    slope_oss << std::fixed << std::setprecision(4) << m;
    std::string slope_str = "m=" + slope_oss.str();
    drawText(ren, fontSm, slope_str, cr.cR + 4, cr.cB - 30, COL_LINEAR, 0, 0);

    std::ostringstream r2_oss;
    r2_oss << std::fixed << std::setprecision(3) << r_squared;
    std::string r2_str = "R²=" + r2_oss.str();
    drawText(ren, fontSm, r2_str, cr.cR + 4, cr.cB - 12, COL_LINEAR, 0, 0);
}
