#include "viewmode_parabolic.h"

#include <cmath>
#include <iomanip>
#include <sstream>
#include <string>

static constexpr RGBA COL_PARABOLA = {255, 180, 50, 220};   // Orange main parabola
static constexpr RGBA COL_BAND     = {255, 180, 50, 140};   // Orange confidence bands

void renderParabolicOverlay(SDL_Renderer* ren, TTF_Font* fontSm,
                            const std::vector<PricePoint>& price_history,
                            const ChartRegion& cr) {
    int N   = cr.N;
    int off = cr.displayStart;

    // Need at least 3 points for quadratic regression
    if (N <= 2) return;

    // Clip drawing to chart area
    SDL_Rect clipRect = {cr.cL, cr.cT, cr.cW, cr.cH};
    SDL_RenderSetClipRect(ren, &clipRect);

    // Calculate quadratic regression: y = ax² + bx + c
    // Center x values for numerical stability: x ranges from -(N-1)/2 to +(N-1)/2

    double center_x = (N - 1) / 2.0;

    // Calculate sums needed for the normal equations
    double sum_x = 0, sum_x2 = 0, sum_x3 = 0, sum_x4 = 0;
    double sum_y = 0, sum_xy = 0, sum_x2y = 0;

    for (int i = 0; i < N; ++i) {
        double x = i - center_x;  // Center around 0
        double y = price_history[off + i].price;
        double x2 = x * x;
        double x3 = x2 * x;
        double x4 = x3 * x;

        sum_x += x;
        sum_x2 += x2;
        sum_x3 += x3;
        sum_x4 += x4;
        sum_y += y;
        sum_xy += x * y;
        sum_x2y += x2 * y;
    }

    // Solve the normal equations: [A][coeff] = [b]
    // From least squares minimization: y = ax² + bx + c
    // [Σx⁴   Σx³   Σx²] [a]   [Σx²y]
    // [Σx³   Σx²   Σx ] [b] = [Σxy ]
    // [Σx²   Σx    N  ] [c]   [Σy  ]

    double det = sum_x4 * (sum_x2 * N - sum_x * sum_x)
               - sum_x3 * (sum_x3 * N - sum_x * sum_x2)
               + sum_x2 * (sum_x3 * sum_x - sum_x2 * sum_x2);

    // Check for singular matrix
    if (std::abs(det) < 1e-10) return;

    // Using Cramer's rule to solve for a, b, c
    // Replace column 1 with RHS for a
    double det_a = sum_x2y * (sum_x2 * N - sum_x * sum_x)
                 - sum_xy * (sum_x3 * N - sum_x * sum_x2)
                 + sum_y * (sum_x3 * sum_x - sum_x2 * sum_x2);

    // Replace column 2 with RHS for b
    double det_b = sum_x4 * (sum_xy * N - sum_x * sum_y)
                 - sum_x3 * (sum_x2y * N - sum_x * sum_y)
                 + sum_x2 * (sum_x2y * sum_x - sum_xy * sum_x2);

    // Replace column 3 with RHS for c
    double det_c = sum_x4 * (sum_x2 * sum_y - sum_x * sum_xy)
                 - sum_x3 * (sum_x3 * sum_y - sum_x * sum_x2y)
                 + sum_x2 * (sum_x3 * sum_xy - sum_x2 * sum_x2y);

    double a = det_a / det;
    double b = det_b / det;
    double c = det_c / det;

    // Calculate standard deviation of residuals
    double sum_sq_residuals = 0;
    for (int i = 0; i < N; ++i) {
        double x = i - center_x;  // Use centered x
        double fitted = a * x * x + b * x + c;
        double residual = price_history[off + i].price - fitted;
        sum_sq_residuals += residual * residual;
    }
    double sigma = std::sqrt(sum_sq_residuals / N);

    // Calculate mean for R² calculation
    double mean_y = sum_y / N;
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
        double x = i - center_x;  // Use centered x
        double fitted_upper = a * x * x + b * x + c + sigma;
        double fitted_lower = a * x * x + b * x + c - sigma;
        int y_upper = cr.toY(fitted_upper);
        int y_lower = cr.toY(fitted_lower);
        int px = cr.toX(i);

        // Draw vertical line between upper and lower band at this x position
        if (y_lower > y_upper) {
            SDL_RenderDrawLine(ren, px, y_upper, px, y_lower);
        }
    }

    // Draw upper confidence band (parabola + sigma) - dashed
    SDL_SetRenderDrawColor(ren, COL_BAND.r, COL_BAND.g, COL_BAND.b, COL_BAND.a);
    for (int i = 0; i < N - 1; ++i) {
        double x_curr = i - center_x;  // Use centered x
        double x_next = (i + 1) - center_x;
        double fitted_curr = a * x_curr * x_curr + b * x_curr + c + sigma;
        double fitted_next = a * x_next * x_next + b * x_next + c + sigma;
        int x1 = cr.toX(i);
        int y1 = cr.toY(fitted_curr);
        int x2 = cr.toX(i + 1);
        int y2 = cr.toY(fitted_next);

        // Draw dashes (4 pixels) with gaps (4 pixels)
        int dx = x2 - x1;
        int dy = y2 - y1;
        int steps = 4;

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

    // Draw lower confidence band (parabola - sigma) - dashed
    SDL_SetRenderDrawColor(ren, COL_BAND.r, COL_BAND.g, COL_BAND.b, COL_BAND.a);
    for (int i = 0; i < N - 1; ++i) {
        double x_curr = i - center_x;  // Use centered x
        double x_next = (i + 1) - center_x;
        double fitted_curr = a * x_curr * x_curr + b * x_curr + c - sigma;
        double fitted_next = a * x_next * x_next + b * x_next + c - sigma;
        int x1 = cr.toX(i);
        int y1 = cr.toY(fitted_curr);
        int x2 = cr.toX(i + 1);
        int y2 = cr.toY(fitted_next);

        // Draw dashes (4 pixels) with gaps (4 pixels)
        int dx = x2 - x1;
        int dy = y2 - y1;
        int steps = 4;

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

    // Draw main parabola (thick)
    SDL_SetRenderDrawColor(ren, COL_PARABOLA.r, COL_PARABOLA.g, COL_PARABOLA.b, COL_PARABOLA.a);
    for (int i = 1; i < N; ++i) {
        double x_prev = (i - 1) - center_x;  // Use centered x
        double x_curr = i - center_x;
        double fitted_prev = a * x_prev * x_prev + b * x_prev + c;
        double fitted_curr = a * x_curr * x_curr + b * x_curr + c;
        thickLine(ren, cr.toX(i - 1), cr.toY(fitted_prev),
                       cr.toX(i),     cr.toY(fitted_curr));
    }

    // Remove clip for labels drawn outside chart area
    SDL_RenderSetClipRect(ren, nullptr);

    // Draw labels on right edge (at x = N-1)
    double x_last = (N - 1) - center_x;  // Use centered x
    struct ParabolicLabel { double val; RGBA col; std::string label; };
    ParabolicLabel labels[] = {
        { a * x_last * x_last + b * x_last + c + sigma, COL_BAND, "+σ" },
        { a * x_last * x_last + b * x_last + c,         COL_PARABOLA, "para" },
        { a * x_last * x_last + b * x_last + c - sigma, COL_BAND, "-σ" },
    };

    for (auto& pl : labels) {
        int y = cr.toY(pl.val);
        if (y < cr.cT || y > cr.cB) continue;

        std::ostringstream oss;
        oss << std::fixed << std::setprecision(2) << pl.val;
        std::string lbl = pl.label + " " + oss.str();
        drawText(ren, fontSm, lbl, cr.cR + 4, y, pl.col, 0, 1);
    }

    // Draw coefficients at bottom right
    std::ostringstream a_oss, b_oss, r2_oss;
    a_oss << std::fixed << std::setprecision(5) << a;
    b_oss << std::fixed << std::setprecision(4) << b;
    r2_oss << std::fixed << std::setprecision(3) << r_squared;

    std::string a_str = "a=" + a_oss.str();
    std::string b_str = "b=" + b_oss.str();
    std::string r2_str = "R²=" + r2_oss.str();

    drawText(ren, fontSm, a_str, cr.cR + 4, cr.cB - 46, COL_PARABOLA, 0, 0);
    drawText(ren, fontSm, b_str, cr.cR + 4, cr.cB - 28, COL_PARABOLA, 0, 0);
    drawText(ren, fontSm, r2_str, cr.cR + 4, cr.cB - 10, COL_PARABOLA, 0, 0);
}
