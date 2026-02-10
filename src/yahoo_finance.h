#pragma once

#include <string>
#include <vector>

struct PricePoint;

// Fetch JSON response from Yahoo Finance for a given ticker and number of days
std::string fetchJSON(const std::string& currentTicker, int days);

// Parse the Yahoo Finance JSON response into price history
std::vector<PricePoint> parseResponse(const std::string& js, int maxDays);
