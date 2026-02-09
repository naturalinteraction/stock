#include "config.h"
#include <fstream>
#include <iostream>
#include <map>
#include <string> // Added for std::string functions

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

    // Parse "ticker"
    const std::string ticker_search_key = "\"ticker\":";
    pos = content.find(ticker_search_key);
    if (pos != std::string::npos) {
        size_t value_start_quote = content.find("\"", pos + ticker_search_key.length());
        if (value_start_quote != std::string::npos) {
            size_t value_end_quote = content.find("\"", value_start_quote + 1);
            if (value_end_quote != std::string::npos) {
                config.ticker = content.substr(value_start_quote + 1, value_end_quote - (value_start_quote + 1));
            } else {
                std::cerr << "Malformed ticker value in config file. Using default.\n";
            }
        } else {
            std::cerr << "Malformed ticker entry in config file. Using default.\n";
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
    ofs << "    \"ticker\": \"" << config.ticker << "\",\n";
    ofs << "    \"displayed_days\": " << config.displayedDays << "\n";
    ofs << "}\n";
    ofs.close();
}