#include "GdphLoader.hpp"

#include <Geode/Geode.hpp>
#include <matjson.hpp>

#include <fstream>
#include <sstream>
#include <unordered_map>
#include <cstdint>

using namespace macrotrainer;
using namespace geode::prelude;

namespace {
    // Small helper: try to pull a numeric field as double; return default
    // on missing/wrong type.
    double numOr(const matjson::Value& obj, const char* key, double def) {
        if (!obj.contains(key)) return def;
        try { return obj[key].asDouble().unwrapOr(def); }
        catch (...) { return def; }
    }

    int intOr(const matjson::Value& obj, const char* key, int def) {
        if (!obj.contains(key)) return def;
        try { return static_cast<int>(obj[key].asInt().unwrapOr(def)); }
        catch (...) { return def; }
    }

    bool boolOr(const matjson::Value& obj, const char* key, bool def) {
        if (!obj.contains(key)) return def;
        try { return obj[key].asBool().unwrapOr(def); }
        catch (...) { return def; }
    }

    std::string strOr(const matjson::Value& obj, const char* key, const std::string& def) {
        if (!obj.contains(key)) return def;
        try { return obj[key].asString().unwrapOr(def); }
        catch (...) { return def; }
    }
}

GdphResult GdphLoader::load(const std::filesystem::path& path) {
    std::ifstream f(path);
    if (!f) {
        return GdphLoadError{ "Cannot open file: " + path.string() };
    }

    std::stringstream ss;
    ss << f.rdbuf();
    std::string contents = ss.str();

    auto parsed = matjson::parse(contents);
    if (!parsed) {
        // ParseError is not a plain string; format it with fmt.
        return GdphLoadError{
            fmt::format("JSON parse error: {}", parsed.unwrapErr())
        };
    }
    matjson::Value root = parsed.unwrap();

    GdphData data;
    data.levelId   = static_cast<uint32_t>(intOr(root, "levelId", 0));
    data.levelName = strOr(root, "levelName", "(unknown)");

    if (!root.contains("clicks")) {
        return GdphLoadError{ "No 'clicks' array in file" };
    }
    auto clicksVal = root["clicks"];

    // Walk through events, pair each press with the next matching release.
    // Key: (button, player2). If we see a press when a key is already open,
    // we drop the old one silently (means we missed the release).
    struct Pending { float x, y; };
    std::unordered_map<int, Pending> open; // key = button << 1 | p2

    size_t paired = 0;
    size_t dropped = 0;

    for (const auto& click : clicksVal) {
        const uint8_t button  = static_cast<uint8_t>(intOr(click, "button", 1));
        const bool    player2 = boolOr(click, "p2", false);
        const bool    down    = boolOr(click, "down", true);
        const float   x       = static_cast<float>(numOr(click, "x", 0.0));
        const float   y       = static_cast<float>(numOr(click, "y", 0.0));

        const int key = (static_cast<int>(button) << 1) | (player2 ? 1 : 0);

        if (down) {
            if (open.count(key)) {
                // Lost a release somewhere; overwrite silently.
                dropped++;
            }
            open[key] = { x, y };
        } else {
            auto it = open.find(key);
            if (it == open.end()) {
                // Unmatched release — ignore.
                dropped++;
                continue;
            }
            ClickInterval iv;
            iv.startX  = it->second.x;
            iv.startY  = it->second.y;
            iv.endX    = x;
            iv.button  = button;
            iv.player2 = player2;

            // Sanity: end must be >= start in normal gameplay. If for some
            // reason it isn't (player teleported backward?), clamp to a
            // minimum 1px so the bar is at least visible.
            if (iv.endX < iv.startX) iv.endX = iv.startX + 1.f;

            data.intervals.push_back(iv);
            open.erase(it);
            paired++;
        }
    }

    // Presses still open at end-of-file never got a release — drop them.
    dropped += open.size();

    log::info("GdphLoader: {} paired intervals, {} dropped events",
        paired, dropped);

    return data;
}
