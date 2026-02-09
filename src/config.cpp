#include "config.h"
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <vector>
#include <algorithm>

// Mapping enum to string
static std::map<ViewMode, std::string> viewModeToString = {
    {ViewMode::PriceChart, "PriceChart"},
    {ViewMode::PriceChartStats, "PriceChartStats"},
    {ViewMode::Bollinger, "Bollinger"},
    {ViewMode::MACross, "MACross"},
};

// Mapping string to enum
static std::map<std::string, ViewMode> stringToViewMode = {
    {"PriceChart", ViewMode::PriceChart},
    {"PriceChartStats", ViewMode::PriceChartStats},
    {"Bollinger", ViewMode::Bollinger},
    {"MACross", ViewMode::MACross},
};


Config loadConfig(const std::string& filename) {
    Config config; // This will hold default values if file not found or parsing fails
    std::ifstream ifs(filename);
    if (!ifs.is_open()) {
        std::cerr << "Config file " << filename << " not found. Creating default.\n";
        config.tickers.push_back("VWCE.DE");
        config.tickers.push_back("VHYL.AS");
        config.tickers.push_back("VWCE.MI");
        config.tickers.push_back("WS5X.MI");
        config.tickers.push_back("USDEUR=X");
        config.tickers.push_back("EURUSD=X");
        saveConfig(config, filename); // Save default config
        return config;
    }

    std::string content((std::istreambuf_iterator<char>(ifs)),
                        (std::istreambuf_iterator<char>()));
    ifs.close();

    // Parse "view_mode"
    const std::string view_mode_search_key = "\"view_mode\":";
    size_t pos = content.find(view_mode_search_key);
    if (pos != std::string::npos) {
        size_t value_start_quote = content.find("\"", pos + view_mode_search_key.length());
        if (value_start_quote != std::string::npos) {
            size_t value_end_quote = content.find("\"", value_start_quote + 1);
            if (value_end_quote != std::string::npos) {
                std::string modeStr = content.substr(value_start_quote + 1, value_end_quote - (value_start_quote + 1));
                if (stringToViewMode.count(modeStr)) {
                    config.viewMode = stringToViewMode[modeStr];
                } else {
                    std::cerr << "Unknown view_mode '" << modeStr << "' in config file. Using default.\n";
                }
            }
        }
    }

    // Parse "fullscreen"
    const std::string fullscreen_search_key = "\"fullscreen\":";
    pos = content.find(fullscreen_search_key);
    if (pos != std::string::npos) {
        size_t value_start = pos + fullscreen_search_key.length();
        while (value_start < content.length() && (content[value_start] == ' ' || content[value_start] == '\t')) {
            value_start++;
        }
        if (value_start < content.length()) {
            size_t value_end = content.find_first_of(" \t\n\r,}", value_start);
            std::string boolStr = content.substr(value_start, value_end - value_start);
            if (boolStr == "true") {
                config.fullscreen = true;
            } else if (boolStr == "false") {
                config.fullscreen = false;
            } else {
                std::cerr << "Unknown fullscreen value '" << boolStr << "' in config file. Using default.\n";
            }
        }
    }

    // Parse "tickers"
    const std::string tickers_search_key = "\"tickers\":";
    pos = content.find(tickers_search_key);
    if (pos != std::string::npos) {
        size_t array_start = content.find("[", pos + tickers_search_key.length());
        size_t array_end = content.find("]", array_start);

        if (array_start != std::string::npos && array_end != std::string::npos && array_start < array_end) {
            std::string tickers_str = content.substr(array_start + 1, array_end - (array_start + 1));
            
            size_t start = 0;
            size_t end = tickers_str.find(",");
            while (end != std::string::npos) {
                std::string ticker_val = tickers_str.substr(start, end - start);
                // Remove leading/trailing whitespace and quotes
                ticker_val.erase(0, ticker_val.find_first_not_of(" \t\"\n"));
                ticker_val.erase(ticker_val.find_last_not_of(" \t\"\n") + 1);
                config.tickers.push_back(ticker_val);
                start = end + 1;
                end = tickers_str.find(",", start);
            }
            // Add the last ticker
            std::string ticker_val = tickers_str.substr(start);
            ticker_val.erase(0, ticker_val.find_first_not_of(" \t\"\n"));
            ticker_val.erase(ticker_val.find_last_not_of(" \t\"\n") + 1);
            config.tickers.push_back(ticker_val);

            // Determine the filler ticker: first parsed ticker, or use the hardcoded defaults
            if (config.tickers.empty()) {
                config.tickers.push_back("VWCE.DE");
                config.tickers.push_back("VHYL.AS");
                config.tickers.push_back("VWCE.MI");
                config.tickers.push_back("WS5X.MI");
                config.tickers.push_back("USDEUR=X");
                config.tickers.push_back("EURUSD=X");
            }

            // If less than 6 tickers are provided, fill with first parsed ticker (if available)
            if (config.tickers.size() < 6) {
                std::string firstTicker = config.tickers[0]; // Assumes config.tickers is not empty due to previous block
                while (config.tickers.size() < 6) {
                    config.tickers.push_back(firstTicker);
                }
            }
            // If more than 6 tickers are provided, truncate
            if (config.tickers.size() > 6) {
                config.tickers.resize(6);
            }

        } else {
            std::cerr << "Malformed tickers array in config file. Using default.\n";
        }
    }


    // Parse "displayed_days"
    const std::string displayed_days_search_key = "\"displayed_days\":";
    pos = content.find(displayed_days_search_key);
    if (pos != std::string::npos) {
        size_t value_start = pos + displayed_days_search_key.length();
        // Skip whitespace
        while (value_start < content.length() && (content[value_start] == ' ' || content[value_start] == '\t')) {
            value_start++;
        }
        if (value_start < content.length()) {
            size_t value_end = content.find_first_of(" \t\n\r,}", value_start);
            std::string daysStr = content.substr(value_start, value_end - value_start);
            try {
                config.displayedDays = std::stoi(daysStr);
            } catch (...) {
                std::cerr << "Malformed displayed_days value '" << daysStr << "' in config file. Using default.\n";
            }
        }
    }

    // Parse "last_active_ticker_index"
    const std::string last_active_ticker_index_search_key = "\"last_active_ticker_index\":";
    pos = content.find(last_active_ticker_index_search_key);
    if (pos != std::string::npos) {
        size_t value_start = pos + last_active_ticker_index_search_key.length();
        // Skip whitespace
        while (value_start < content.length() && (content[value_start] == ' ' || content[value_start] == '\t')) {
            value_start++;
        }
        if (value_start < content.length()) {
            size_t value_end = content.find_first_of(" \t\n\r,}", value_start);
            std::string indexStr = content.substr(value_start, value_end - value_start);
            try {
                config.lastActiveTickerIndex = std::stoi(indexStr);
            } catch (...) {
                std::cerr << "Malformed last_active_ticker_index value '" << indexStr << "' in config file. Using default.\n";
            }
        }
    }

    return config;
}

void saveConfig(const Config& config, const std::string& filename) {
    // Ensure the data directory exists
    system("mkdir -p data");

    std::ofstream ofs(filename);
    if (!ofs.is_open()) {
        std::cerr << "Failed to open config file " << filename << " for writing.\n";
        return;
    }

    ofs << "{\n";
    ofs << "    \"view_mode\": \"" << viewModeToString[config.viewMode] << "\",\n";
    ofs << "    \"fullscreen\": " << (config.fullscreen ? "true" : "false") << ",\n";
    ofs << "    \"tickers\": [\n";
    for (size_t i = 0; i < config.tickers.size(); ++i) {
        ofs << "        \"" << config.tickers[i] << "\"";
        if (i < config.tickers.size() - 1) {
            ofs << ",";
        }
        ofs << "\n";
    }
    ofs << "    ],\n";
    ofs << "    \"displayed_days\": " << config.displayedDays << ",\n";
    ofs << "    \"last_active_ticker_index\": " << config.lastActiveTickerIndex << "\n";
    ofs << "}\n";
    ofs.close();
}