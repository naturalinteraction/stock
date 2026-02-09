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
    Config config;
    std::ifstream ifs(filename);
    if (!ifs.is_open()) {
        std::cerr << "Config file " << filename << " not found. Creating default.\n";
        saveConfig(config, filename); // Save default config
        return config;
    }

    std::string line;
    std::string content;
    while (std::getline(ifs, line)) {
        content += line;
    }
    ifs.close();

    // Basic JSON parsing for "view_mode" property
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

    // Basic JSON parsing for "fullscreen" property
    const std::string fullscreen_search_key = "\"fullscreen\":";
    pos = content.find(fullscreen_search_key);
    if (pos != std::string::npos) {
        size_t value_start = pos + fullscreen_search_key.length();
        // Skip whitespace
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
    ofs << "    \"fullscreen\": " << (config.fullscreen ? "true" : "false") << "\n";
    ofs << "}\n";
    ofs.close();
}