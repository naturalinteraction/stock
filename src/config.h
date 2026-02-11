#pragma once

#include <vector>
#include "chart.h"
#include <string>

const std::string CONFIG_FILE_PATH = "data/config.json";


struct Config {
    ViewMode viewMode;
    bool fullscreen;
    std::vector<std::string> tickers;
    int displayedDays;
    int lastActiveTickerIndex;

    // Default constructor
    Config() : viewMode(ViewMode::Trend), fullscreen(false), displayedDays(10), lastActiveTickerIndex(0) {}
};

Config loadConfig(const std::string& filename = CONFIG_FILE_PATH);
void saveConfig(const Config& config, const std::string& filename = CONFIG_FILE_PATH);
