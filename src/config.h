#pragma once

#include "chart.h"
#include <string>

const std::string CONFIG_FILE_PATH = "data/config.json";

struct Config {
    ViewMode viewMode;

    // Default constructor
    Config() : viewMode(ViewMode::PriceChart) {}
};

Config loadConfig(const std::string& filename = CONFIG_FILE_PATH);
void saveConfig(const Config& config, const std::string& filename = CONFIG_FILE_PATH);
