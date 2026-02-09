#pragma once

#include "chart.h"
#include <string>

const std::string CONFIG_FILE_PATH = "data/config.json";
static const std::string CONFIG_DEFAULT_TICKER = "VWCE.DE";

struct Config {
    ViewMode viewMode;
    bool fullscreen;
    std::string ticker;

    // Default constructor
    Config() : viewMode(ViewMode::PriceChart), fullscreen(false), ticker(CONFIG_DEFAULT_TICKER) {}
};

Config loadConfig(const std::string& filename = CONFIG_FILE_PATH);
void saveConfig(const Config& config, const std::string& filename = CONFIG_FILE_PATH);
