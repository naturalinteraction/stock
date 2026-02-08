# StockChart - ETF Price Chart Viewer

StockChart is a command-line interface (CLI) application for visualizing ETF price charts, offering various analysis view modes.

## Usage

To run the application, use the following command:

```bash
./bin/stock [TICKER] [DAYS]
```

-   `TICKER`: Yahoo Finance ticker symbol (e.g., `VWCE.DE`, `^GSPC`). Defaults to `VWCE.DE`.
-   `DAYS`: Number of trading days to display (e.g., `30`). Defaults to `30`.

**Example:**
```bash
./bin/stock AAPL 90
```

## Building from Source

### Prerequisites

To build StockChart, you need the following:
-   A C++17 compatible compiler (e.g., `g++`)
-   `SDL2` development libraries
-   `SDL2_ttf` development libraries
-   `libcurl` development libraries

**For Ubuntu/Debian based systems, you can install dependencies using:**
```bash
sudo apt install build-essential libsdl2-dev libsdl2-ttf-dev libcurl4-openssl-dev
```

### Build Commands

Navigate to the project root directory and run `make`:
```bash
make
```
This will compile the application and create the executable `bin/stock`.

## View Modes

StockChart supports different viewing modes to analyze price data. You can cycle through these modes by pressing the `TAB` key while the application is running.

Here is a list of available view modes:

*   **PriceChart**: The basic price chart displaying candlesticks/lines.
*   **PriceChartStats**: Overlays statistical information on the price chart.
*   **Bollinger**: Displays Bollinger Bands, showing market volatility and potential overbought/oversold conditions.
*   **MACross**: Shows Moving Average Crossover indicators (e.g., for trend identification).
*   **PriceAction**: Visualizes price action elements such as Support and Resistance levels (based on swing highs and lows).
*   *(Future View Mode)*: Description of a future view mode.

## Contributing / Extending

This project is designed to be extensible with new view modes. To add a new view mode:

1.  **Create Files**: Create two new files, `viewmode_YOURNAME.h` and `viewmode_YOURNAME.cpp`, in the `src/` directory.
2.  **Define Overlay Function**: In `viewmode_YOURNAME.h`, declare a function `void renderYOURNAMEOffset(SDL_Renderer* ren, TTF_Font* fontSm, const std::vector<PricePoint>& price_history, const ChartRegion& cr);` (or similar signature as existing view modes).
3.  **Implement Logic**: In `viewmode_YOURNAME.cpp`, implement the rendering logic for your new view mode.
4.  **Update `src/chart.h`**: Add `YOURNAME` to the `enum class ViewMode` and increment `VIEW_MODE_COUNT`.
5.  **Update `src/main.cpp`**:
    *   Include `viewmode_YOURNAME.h`.
    *   Add an `else if (viewMode == ViewMode::YOURNAME)` block in `renderChart` to call your new overlay function.
    *   Add a case for your new view mode to the `switch` statement that generates the title, providing a descriptive name.
6.  **Update `Makefile`**: Add `src/viewmode_YOURNAME.cpp` to the `SRC` variable.
7.  **Recompile**: Run `make` to recompile the project.

Remember to follow the existing coding style and structure.
