#include "yahoo_finance.h"
#include "chart.h"

#include <curl/curl.h>

#include <algorithm>
#include <cmath>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>

// ═══════════════════════  Network  ═══════════════════════

static size_t curlWrite(void* buf, size_t sz, size_t n, std::string* out) {
    out->append(static_cast<char*>(buf), sz * n);
    return sz * n;
}

std::string fetchJSON(const std::string& currentTicker, int days) {
    CURL* c = curl_easy_init();
    if (!c) { std::cerr << "curl_easy_init failed\n"; return {}; }

    curl_easy_setopt(c, CURLOPT_COOKIEFILE, "");        // enable cookie engine
    curl_easy_setopt(c, CURLOPT_USERAGENT,
        "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36");
    curl_easy_setopt(c, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(c, CURLOPT_TIMEOUT, 15L);
    curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, curlWrite);

    // Step 1 — hit fc.yahoo.com to seed cookies
    std::string dummy;
    curl_easy_setopt(c, CURLOPT_URL, "https://fc.yahoo.com/");
    curl_easy_setopt(c, CURLOPT_WRITEDATA, &dummy);
    curl_easy_perform(c);                               // result ignored

    // Step 2 — obtain crumb token
    std::string crumb;
    curl_easy_setopt(c, CURLOPT_URL,
        "https://query2.finance.yahoo.com/v1/test/getcrumb");
    curl_easy_setopt(c, CURLOPT_WRITEDATA, &crumb);
    CURLcode res = curl_easy_perform(c);
    if (res != CURLE_OK || crumb.empty()) {
        std::cerr << "Warning: could not obtain auth crumb, trying without\n";
        crumb.clear();
    }

    // Step 3 — fetch chart data
    int calDays = static_cast<int>(days * 1.6) + 15;
    std::string url = "https://query2.finance.yahoo.com/v8/finance/chart/"
                    + currentTicker + "?range=" + std::to_string(calDays)
                    + "d&interval=1d";

    if (!crumb.empty()) {
        char* enc = curl_easy_escape(c, crumb.c_str(),
                                     static_cast<int>(crumb.size()));
        url += "&crumb=";
        url += enc;
        curl_free(enc);
    }

    std::string resp;
    curl_easy_setopt(c, CURLOPT_URL, url.c_str());
    curl_easy_setopt(c, CURLOPT_WRITEDATA, &resp);
    res = curl_easy_perform(c);

    long httpCode = 0;
    curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &httpCode);
    curl_easy_cleanup(c);

    if (res != CURLE_OK) {
        std::cerr << "HTTP request failed: " << curl_easy_strerror(res) << "\n";
        return {};
    }
    if (httpCode != 200) {
        std::cerr << "Yahoo Finance returned HTTP " << httpCode << "\n";
        if (httpCode == 404)
            std::cerr << "Ticker not found. Try adding an exchange suffix "
                         "(e.g. .DE  .L  .AS  .MI)\n";
        return {};
    }
    return resp;
}

// ═══════════════════════  JSON helpers  ═══════════════════════

static std::vector<double> jsonNumberArray(const std::string& js,
                                           const std::string& key,
                                           size_t from = 0) {
    std::vector<double> v;
    std::string needle = "\"" + key + "\"";
    size_t p = js.find(needle, from);
    if (p == std::string::npos) return v;
    p = js.find('[', p + needle.size());
    if (p == std::string::npos) return v;

    size_t depth = 1, e = p + 1;
    while (e < js.size() && depth > 0) {
        if (js[e] == '[') ++depth;
        else if (js[e] == ']') --depth;
        ++e;
    }

    std::string body = js.substr(p + 1, e - p - 2);
    std::istringstream ss(body);
    std::string tok;
    while (std::getline(ss, tok, ',')) {
        auto a = tok.find_first_not_of(" \t\n\r");
        if (a == std::string::npos) { v.push_back(NAN); continue; }
        tok = tok.substr(a);
        auto b = tok.find_last_not_of(" \t\n\r");
        if (b != std::string::npos) tok.resize(b + 1);

        if (tok == "null" || tok.empty())
            v.push_back(NAN);
        else {
            try   { v.push_back(std::stod(tok)); }
            catch (...) { v.push_back(NAN); }
        }
    }
    return v;
}

std::vector<PricePoint> parseResponse(const std::string& js, int maxDays) {
    std::vector<PricePoint> pts;
    if (js.find("\"result\":null") != std::string::npos) {
        std::cerr << "Yahoo Finance returned an error payload\n";
        return pts;
    }

    auto ts = jsonNumberArray(js, "timestamp");
    size_t qp = js.find("\"quote\"");
    if (qp == std::string::npos) {
        std::cerr << "No quote data in response\n";
        return pts;
    }
    auto cl = jsonNumberArray(js, "close", qp);

    size_t n = std::min(ts.size(), cl.size());
    for (size_t i = 0; i < n; ++i) {
        if (std::isnan(cl[i])) continue;           // skip non-trading days
        time_t t = static_cast<time_t>(ts[i]);
        struct tm tm{};
        gmtime_r(&t, &tm);
        char buf[11];
        strftime(buf, sizeof(buf), "%Y-%m-%d", &tm);
        pts.push_back({cl[i], buf});
    }

    if (static_cast<int>(pts.size()) > maxDays)
        pts.erase(pts.begin(),
                  pts.begin() + static_cast<long>(pts.size() - maxDays));
    return pts;
}
