#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace macrotrainer {

// One click event expressed as an interval in world space.
// For short taps, endX == startX + (a few px). For holds, width is larger.
struct ClickInterval {
    float   startX;   // world X at the press
    float   startY;   // world Y at the press
    float   endX;     // world X at the release
    uint8_t button;   // 1 = jump, 2 = left, 3 = right
    bool    player2;

    float width() const { return endX - startX; }
};

struct GdphData {
    uint32_t                  levelId = 0;
    std::string               levelName;
    std::vector<ClickInterval> intervals;
};

struct GdphLoadError {
    std::string message;
};

using GdphResult = std::variant<GdphData, GdphLoadError>;

class GdphLoader {
public:
    // Parses the .gdph file at `path`. On success returns GdphData with
    // every press paired with its matching release — unpaired events are
    // silently dropped (they can't be drawn as a bar anyway).
    static GdphResult load(const std::filesystem::path& path);
};

} // namespace macrotrainer
