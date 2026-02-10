# Project: Stock - Price Chart Viewer

## Project Overview

This project is a C++ application designed for visualizing stock price charts. It offers various analysis view modes, such as PriceChart, PriceChartStats, Bollinger Bands, Moving Average Crossover (MACross), and Price Action (Support/Resistance levels). The application is built using C++ and relies on SDL2 for graphics, SDL2_ttf for text rendering, `libcurl` for fetching stock data from Yahoo Finance, and a `rest_server` for providing a REST API to control the application (e.g., changing the displayed ticker).

## Building and Running

### Prerequisites

To build the application, ensure you have the following installed:

*   A C++17 compatible compiler (e.g., `g++`)
*   `SDL2` development libraries
*   `SDL2_ttf` development libraries
*   `libcurl` development libraries

For Ubuntu/Debian based systems, you can install these dependencies using:

```bash
sudo apt install build-essential libsdl2-dev libsdl2-ttf-dev libcurl4-openssl-dev
```

### Build Commands

Navigate to the project root directory and execute:

```bash
make
```

This command will compile the source code and create the executable `bin/stock`.

### Running the Application

To run the application, use the following command structure:

```bash
./bin/stock [TICKER] [DAYS]
```

*   `TICKER`: Yahoo Finance ticker symbol (e.g., `VWCE.DE`, `^GSPC`). Defaults to `VWCE.DE`.
*   `DAYS`: Number of trading days to display (e.g., `50`). Defaults to `30`.

**Example:**

```bash
./bin/stock AAPL 90
```

While the application is running, you can cycle through different view modes by pressing the `TAB` key.


## Project Structure

*   `src/config.h` / `src/config.cpp`: These files manage the application's configuration, including settings like the last active ticker, current view mode, fullscreen status, and the number of displayed trading days.
*   `src/rest_server.h` / `src/rest_server.cpp`: These files implement a RESTful API server that allows external control over the application, such as dynamically changing the displayed stock ticker.
*   `TODO.md`: This file contains a list of pending tasks and future development ideas for the project.

## Development Conventions

The project is structured to be modular and extensible, particularly for adding new view modes. Key conventions include:

*   **Modular View Modes:** Each analysis view mode has its own header (`viewmode_YOURNAME.h`) and source (`viewmode_YOURNAME.cpp`) file within the `src/` directory.
*   **Overlay Function Pattern:** New view modes should implement a rendering function with a signature similar to `void renderYOURNAMEOffset(SDL_Renderer* ren, TTF_Font* fontSm, const std::vector<PricePoint>& price_history, const ChartRegion& cr);`.
*   **Integration Points:** To add a new view mode, modifications are required in `src/chart.h` (to extend `enum class ViewMode` and `VIEW_MODE_COUNT`), `src/main.cpp` (to include the new header, call the rendering function, and update the title generation), and the `Makefile` (to add the new source file to compilation).
*   **Coding Style:** Developers should adhere to the existing coding style and structure observed in the `src/` directory.
