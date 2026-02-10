# Stock - Interactive Price Chart Viewer

A C++ SDL2-based stock price chart viewer with multiple analysis view modes, REST API control, and interactive UI.

## Quick Start

**Build:** `make`

**Run:** `./bin/stock`

**Build And Run:** `./r.sh`

**Test REST Only:** `./test_rest.sh` (it will launch `bin/stock`)

**Test MCP:** `./r.sh`, then start `./mcp_bridge.py`, then `show VHYL.AS` in Claude Code

The application will:
- Load default tickers from config file
- Start REST API on port 8080
- Display interactive chart window

## Usage & Controls

### Interactive Controls
- **TAB**: Cycle through view modes
- **SPACE**: Switch between configured tickers
- **F**: Toggle fullscreen mode
- **R**: Reload current ticker data
- **↑/↓ Arrow Keys**: Zoom in/out (adjust displayed days)
- **Mouse Wheel**: Zoom in/out (1 day increments)
- **ESC**: Exit application

### Mouse Interaction
- **Click View Mode Tabs**: Select specific analysis mode
- **Click Ticker Buttons**: Switch directly to a ticker
- **Mouse Hover**: Display price/date information on chart
- **Window Resizing**: Chart scales automatically

## Configuration

The application uses `data/config.json` for persistent settings:

```json
{
    "view_mode": "PriceChart",
    "fullscreen": false,
    "tickers": ["VWCE.DE", "VHYL.AS", "WS5X.MI", "BTC-USD", "USDEUR=X", "EURUSD=X"],
    "displayed_days": 10,
    "last_active_ticker_index": 0
}
```

Default tickers are automatically created on first run.

## View Modes

*   **PriceChart**: Basic price chart with candlesticks and lines
*   **PriceChartStats**: Price chart with statistical overlays
*   **Bollinger**: Bollinger Bands for volatility analysis
*   **MACross**: Moving Average Crossover indicators for trend identification

## REST API

The application exposes a REST API on `http://localhost:8080`:

### Endpoints

**GET /** 
Returns API information

**GET /tickers**
Returns list of available tickers

**POST /set-ticker**
Change the current ticker:
```bash
curl -X POST -H "Content-Type: application/json" \
     -d '{"ticker":"AAPL"}' \
     http://localhost:8080/set-ticker
```

### MCP Integration

The application includes an MCP (Model Context Protocol) bridge at `mcp_bridge.py` for integration with AI assistants. Configure it with `.mcp.json`.

## Building from Source

### Prerequisites

-   C++17 compatible compiler (g++)
-   SDL2 development libraries
-   SDL2_ttf development libraries  
-   libcurl development libraries

**Ubuntu/Debian:**
```bash
sudo apt install build-essential libsdl2-dev libsdl2-ttf-dev libcurl4-openssl-dev
```

### Build Commands

```bash
make          # Build the application
make clean    # Clean build artifacts
```

The executable will be created at `bin/stock`.

## Architecture

**Core Components:**
- `src/main.cpp` - Application entry point and event loop
- `src/chart.h` - Data structures and layout constants
- `src/viewmode_*.cpp` - Individual view mode implementations
- `src/config.cpp` - Configuration management
- `src/rest_server.cpp` - HTTP API server

**Data Flow:**
1. Configuration loaded from JSON file
2. Historical data fetched from Yahoo Finance via libcurl
3. REST server starts on port 8080 for external control
4. Interactive SDL2 window displays charts with user controls

## Testing REST API

Use the provided test script:
```bash
./test_rest.sh
```

This will test all REST endpoints while the application is running.

## Contributing

### Adding a New View Mode

1. Create `src/viewmode_YOURNAME.h` with function declaration:
   ```cpp
   void renderYOURNAMEOffset(SDL_Renderer* ren, TTF_Font* fontSm, 
                             const std::vector<PricePoint>& price_history, 
                             const ChartRegion& cr);
   ```

2. Create `src/viewmode_YOURNAME.cpp` with implementation

3. Add `YOURNAME` to `enum class ViewMode` in `src/chart.h` (before `_COUNT`)

4. Include header in `src/main.cpp`

5. Add `else if (viewMode == ViewMode::YOURNAME)` block in `renderChart()`

6. Add case in view mode title generation switch statement

7. Add `src/viewmode_YOURNAME.cpp` to `SRC` variable in Makefile

8. Run `make` to recompile

The application maintains backward compatibility and follows existing coding patterns.