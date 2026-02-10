#include "ui.h"
#include "draw.h"

// Internal state
static SDL_Rect g_viewModeTabs[4] = {};
static int g_viewModeTabCount = 0;

static SDL_Rect g_tickerButtons[6] = {};
static int g_tickerButtonCount = 0;

static int g_mouseX = 0;
static int g_mouseY = 0;
static bool g_mouseInChartArea = false;

// Mouse state accessors
int getMouseX() { return g_mouseX; }
int getMouseY() { return g_mouseY; }
bool isMouseInChartArea() { return g_mouseInChartArea; }
void setMouseInChartArea(bool inChart) { g_mouseInChartArea = inChart; }
void setMousePosition(int x, int y) { g_mouseX = x; g_mouseY = y; }

std::string getViewModeName(ViewMode mode) {
    switch (mode) {
        case ViewMode::PriceChart:      return "Price";
        case ViewMode::PriceChartStats: return "Stats";
        case ViewMode::Bollinger:       return "Bollinger";
        case ViewMode::MACross:         return "MACross";
        default:                        return "Unknown";
    }
}

bool getClickedViewMode(int mouseX, int mouseY, ViewMode& outMode) {
    for (int i = 0; i < g_viewModeTabCount; i++) {
        const SDL_Rect& tab = g_viewModeTabs[i];
        if (mouseX >= tab.x && mouseX < tab.x + tab.w &&
            mouseY >= tab.y && mouseY < tab.y + tab.h) {
            outMode = static_cast<ViewMode>(i);
            return true;
        }
    }
    return false;
}

bool getHoveredViewMode(int mouseX, int mouseY, ViewMode& outMode) {
    for (int i = 0; i < g_viewModeTabCount; i++) {
        const SDL_Rect& tab = g_viewModeTabs[i];
        if (mouseX >= tab.x && mouseX < tab.x + tab.w &&
            mouseY >= tab.y && mouseY < tab.y + tab.h) {
            outMode = static_cast<ViewMode>(i);
            return true;
        }
    }
    return false;
}

bool getClickedTicker(int mouseX, int mouseY, int& outTickerIndex) {
    for (int i = 0; i < g_tickerButtonCount; i++) {
        const SDL_Rect& button = g_tickerButtons[i];
        if (mouseX >= button.x && mouseX < button.x + button.w &&
            mouseY >= button.y && mouseY < button.y + button.h) {
            outTickerIndex = i;
            return true;
        }
    }
    return false;
}

bool getHoveredTicker(int mouseX, int mouseY, int& outTickerIndex) {
    for (int i = 0; i < g_tickerButtonCount; i++) {
        const SDL_Rect& button = g_tickerButtons[i];
        if (mouseX >= button.x && mouseX < button.x + button.w &&
            mouseY >= button.y && mouseY < button.y + button.h) {
            outTickerIndex = i;
            return true;
        }
    }
    return false;
}

int renderViewModeBar(SDL_Renderer* ren, TTF_Font* font, ViewMode currentMode,
                      int topY, int leftMargin, RGBA bgColor) {
    const int rectHeight = 30;
    const int rectSpacing = 10;
    const int rectPadding = 12;

    int currentX = leftMargin;
    g_viewModeTabCount = VIEW_MODE_COUNT;

    // Check hover state once
    ViewMode hoveredMode;
    bool isHovering = getHoveredViewMode(g_mouseX, g_mouseY, hoveredMode);

    // Draw each rectangle
    for (int i = 0; i < VIEW_MODE_COUNT; i++) {
        ViewMode mode = static_cast<ViewMode>(i);
        std::string name = getViewModeName(mode);

        int textW, textH;
        TTF_SizeText(font, name.c_str(), &textW, &textH);

        int rectW = textW + 2 * rectPadding;
        SDL_Rect rect = {currentX, topY, rectW, rectHeight};

        // Store tab rectangle for click detection
        g_viewModeTabs[i] = rect;

        // Draw filled rectangle
        if (mode == currentMode) {
            // Active view mode - filled with highlight color
            SDL_SetRenderDrawColor(ren, UIColors::VIEWMODE_ACTIVE.r, UIColors::VIEWMODE_ACTIVE.g,
                                   UIColors::VIEWMODE_ACTIVE.b, UIColors::VIEWMODE_ACTIVE.a);
        } else if (isHovering && mode == hoveredMode) {
            // Hovered but not active - filled with hover color
            SDL_SetRenderDrawColor(ren, UIColors::VIEWMODE_HOVER.r, UIColors::VIEWMODE_HOVER.g,
                                   UIColors::VIEWMODE_HOVER.b, UIColors::VIEWMODE_HOVER.a);
        } else {
            // Inactive view mode - filled with background color
            SDL_SetRenderDrawColor(ren, UIColors::VIEWMODE_BG.r, UIColors::VIEWMODE_BG.g,
                                   UIColors::VIEWMODE_BG.b, UIColors::VIEWMODE_BG.a);
        }
        SDL_RenderFillRect(ren, &rect);

        // Draw text centered in rectangle
        RGBA textColor = (mode == currentMode) ? bgColor : UIColors::VIEWMODE_TEXT;
        drawText(ren, font, name, currentX + rectW / 2, topY + rectHeight / 2,
                 textColor, 1, 1);

        currentX += rectW + rectSpacing;
    }

    // Return the right edge position (rightmost x + spacing)
    return currentX - rectSpacing;
}

void renderTickerBar(SDL_Renderer* ren, TTF_Font* font, const std::vector<std::string>& tickers,
                     int currentTickerIndex, int topY, int rightMargin, RGBA bgColor) {
    const int rectHeight = 30;
    const int rectSpacing = 10;
    const int rectPadding = 12;

    int currentX = rightMargin;
    g_tickerButtonCount = std::min(6, static_cast<int>(tickers.size()));

    // Check hover state once
    int hoveredTickerIndex;
    bool isHovering = getHoveredTicker(g_mouseX, g_mouseY, hoveredTickerIndex);

    // Draw each ticker button from right to left
    for (int i = g_tickerButtonCount - 1; i >= 0; i--) {
        const std::string& ticker = tickers[i];

        int textW, textH;
        TTF_SizeText(font, ticker.c_str(), &textW, &textH);

        int rectW = textW + 2 * rectPadding;
        SDL_Rect rect = {currentX - rectW, topY, rectW, rectHeight};

        // Store button rectangle for click detection
        g_tickerButtons[i] = rect;

        // Draw filled rectangle
        if (i == currentTickerIndex) {
            // Active ticker - filled with highlight color
            SDL_SetRenderDrawColor(ren, UIColors::VIEWMODE_ACTIVE.r, UIColors::VIEWMODE_ACTIVE.g,
                                   UIColors::VIEWMODE_ACTIVE.b, UIColors::VIEWMODE_ACTIVE.a);
        } else if (isHovering && i == hoveredTickerIndex) {
            // Hovered but not active - filled with hover color
            SDL_SetRenderDrawColor(ren, UIColors::VIEWMODE_HOVER.r, UIColors::VIEWMODE_HOVER.g,
                                   UIColors::VIEWMODE_HOVER.b, UIColors::VIEWMODE_HOVER.a);
        } else {
            // Inactive ticker - filled with background color
            SDL_SetRenderDrawColor(ren, UIColors::VIEWMODE_BG.r, UIColors::VIEWMODE_BG.g,
                                   UIColors::VIEWMODE_BG.b, UIColors::VIEWMODE_BG.a);
        }
        SDL_RenderFillRect(ren, &rect);

        // Draw text centered in rectangle
        RGBA textColor = (i == currentTickerIndex) ? bgColor : UIColors::VIEWMODE_TEXT;
        drawText(ren, font, ticker, currentX - rectW / 2, topY + rectHeight / 2,
                 textColor, 1, 1);

        currentX -= rectW + rectSpacing;
    }
}
