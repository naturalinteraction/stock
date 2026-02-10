#include "draw.h"

#include <cmath>
#include <cstdio>
#include <ctime>
#include <iomanip>
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

double niceStep(double range, int target) {
    if (range <= 0) return 1.0;
    double rough = range / target;
    double mag   = std::pow(10.0, std::floor(std::log10(rough)));
    double frac  = rough / mag;
    double nice  = (frac <= 1.5) ? 1
                 : (frac <= 3.5) ? 2
                 : (frac <= 7.5) ? 5
                 :                 10;
    return nice * mag;
}

void drawText(SDL_Renderer* r, TTF_Font* f, const std::string& text,
              int x, int y, RGBA col, int ax, int ay) {
    if (text.empty()) return;
    SDL_Color sc = {col.r, col.g, col.b, col.a};
    SDL_Surface* s = TTF_RenderText_Blended(f, text.c_str(), sc);
    if (!s) return;
    SDL_Texture* tx = SDL_CreateTextureFromSurface(r, s);
    if (!tx) { SDL_FreeSurface(s); return; }
    SDL_Rect rc = {x, y, s->w, s->h};
    if (ax == 1) rc.x -= rc.w / 2;
    else if (ax == 2) rc.x -= rc.w;
    if (ay == 1) rc.y -= rc.h / 2;
    SDL_RenderCopy(r, tx, nullptr, &rc);
    SDL_DestroyTexture(tx);
    SDL_FreeSurface(s);
}

std::string shortDate(const std::string& ymd) {
    if (ymd.size() < 10) return ymd;
    struct tm tm{};
    tm.tm_year = std::stoi(ymd.substr(0, 4)) - 1900;
    tm.tm_mon  = std::stoi(ymd.substr(5, 2)) - 1;
    tm.tm_mday = std::stoi(ymd.substr(8, 2));
    mktime(&tm);
    char buf[8];
    strftime(buf, sizeof(buf), "%b %d", &tm);
    return buf;
}

void thickLine(SDL_Renderer* r, int x1, int y1, int x2, int y2) {
    SDL_RenderDrawLine(r, x1, y1,     x2, y2);
    SDL_RenderDrawLine(r, x1, y1 - 1, x2, y2 - 1);
    SDL_RenderDrawLine(r, x1, y1 + 1, x2, y2 + 1);
}

void drawDashedHLine(SDL_Renderer* ren, int x1, int x2, int y,
                     int dashLen, int gapLen) {
    bool drawing = true;
    int seg = 0;
    for (int x = x1; x <= x2; ++x) {
        if (drawing) SDL_RenderDrawPoint(ren, x, y);
        if (++seg >= (drawing ? dashLen : gapLen)) {
            drawing = !drawing;
            seg = 0;
        }
    }
}
