# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Quick Start

**Build:** `make`

**Clean:** `make clean`

**Run:** `./bin/stock`
- Loads last active ticker from config.json, or defaults to VWCE.DE
- Default display: 30 days of data

**Keyboard controls:**
- **TAB** - Cycle through view modes (Candlestick, Bollinger Bands, MA Cross, Stats)
- **SPACE** - Cycle through available tickers
- **Mouse wheel** - Zoom in/out (increments by 1 day per scroll)

**UI controls:**
- Click ticker buttons (right side) to switch tickers
- Hover over chart for price/date labels

**REST API Server:**
- Server runs on `http://localhost:8080`
- `POST /set-ticker` - Change ticker (JSON: `{"ticker": "AAPL"}`)
- `GET /tickers` - List available tickers

## Project Overview

Stock is a C++ SDL2-based stock price chart viewer with multiple analysis view modes. It fetches historical price data via libcurl from Yahoo Finance and renders interactive charts.

### Dependencies
- C++17 compiler (g++)
- SDL2 (graphics)
- SDL2_ttf (text rendering)
- libcurl (data fetching)

### Architecture

**Main components:**
- `src/chart.h` - Core data structures and constants (PricePoint, ChartRegion, ViewMode enum, layout constants)
- `src/main.cpp` - Application entry point, event loop, data fetching, chart rendering, UI interactions
- `src/viewmode_*.cpp/.h` - Individual view mode implementations
- `src/rest_server.cpp/.h` - HTTP server for remote ticker changes (/set-ticker, /tickers endpoints)
- `src/config.h` - Configuration file management for persisting selected ticker and UI state
- `src/mcp_bridge.js` - MCP server bridge for Claude integration (allows ticker changes via Claude Code)

**Data flow:**
1. Fetch historical price data from Yahoo Finance using libcurl
2. Parse into vector<PricePoint> with price and date
3. On each frame, compute ChartRegion (scales, transformations) based on window and visible data range
4. Call current ViewMode's render function to draw overlay analysis
5. Handle TAB key to cycle through ViewMode enum

**Key Design Patterns:**
- **ViewMode enum + function dispatch:** `src/chart.h` defines `enum class ViewMode` with `_COUNT` sentinel. `src/main.cpp` switches on current mode to call appropriate render function.
- **Modular overlays:** Each view mode has its own .cpp/.h pair with a render function taking (SDL_Renderer*, TTF_Font*, price_history, ChartRegion).
- **Static globals for state:** Mouse position, chart data, font references stored as static module-level variables in main.cpp.

## Adding a New View Mode

1. Create `src/viewmode_YOURNAME.h` with function declaration: `void renderYOURNAMEOffset(SDL_Renderer* ren, TTF_Font* fontSm, const std::vector<PricePoint>& price_history, const ChartRegion& cr);`
2. Create `src/viewmode_YOURNAME.cpp` with implementation
3. Add `YOURNAME` to `enum class ViewMode` in `src/chart.h` (must be before `_COUNT`)
4. Include header in `src/main.cpp`
5. Add `else if (viewMode == ViewMode::YOURNAME)` block in `renderChart()` to call your function
6. Add case in switch statement generating view mode title (currently in `renderChart()`)
7. Add `src/viewmode_YOURNAME.cpp` to `SRC` variable in Makefile
8. Run `make` to recompile

The `_COUNT` sentinel in ViewMode enum is used to track total view modes. After modifying the enum, it auto-updates `VIEW_MODE_COUNT` (see recent commit 654c644 for how this is handled).

## Code Layout

**src/chart.h:** Layout constants (GRAPH_WIDTH=1200, GRAPH_HEIGHT=700, margins), RGBA struct, ViewMode enum, ChartRegion struct with coordinate transformation lambdas.

**src/main.cpp (~750 lines):**
- Global state (mouse, fonts, price data, current ticker/days)
- Data fetching from Yahoo Finance
- SDL initialization and event loop
- `renderChart()` - main render orchestrator that calls view mode functions
- View mode title generation

**Global transformations:**
- `ChartRegion.toY(price)` - maps price to pixel Y (inverted, higher prices = lower Y)
- `ChartRegion.toX(displayIndex)` - maps display index [0..N-1] to pixel X

**Hover feature:** Mouse position tracked in globals; render functions should use `g_mouseX`, `g_mouseY`, `g_mouseInChartArea` to draw hover labels. See recent commits (d72d038) for implementation pattern.

## Important Notes

- The application window is fixed size (1200x700 pixels internal rendering)
- All coordinates in pixel space; price data normalized via ChartRegion transforms
- Font handling: Two fonts loaded - `fontLg` (title) and `fontSm` (labels). Must call TTF_CloseFont() on cleanup.
- Colors defined as static RGBA constants at top of main.cpp; reuse these rather than hardcoding values
- **Configuration:** Last selected ticker and view preferences are saved to `config.json` in the working directory and auto-loaded on startup
- **REST Server:** Runs asynchronously on port 8080; ticker changes from REST API are validated and trigger immediate UI updates via thread-safe event signaling
- **UI Elements:** Ticker buttons are positioned at right edge of chart area; view mode tabs are displayed at bottom left. Both update dynamically as features are cycled.
- **Ticker Data:** Default tickers are hardcoded and managed via `DEFAULT_TICKERS`; custom tickers can be added via config.json
