#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>
#include <filesystem>

namespace macrotrainer {

// One recorded click. This is what gets saved into a .gdph file.
struct ClickRecord {
    uint64_t frame;     // physical tick since attempt start (240 tps)
    float    x;         // player's world X at the tick
    float    y;         // player's world Y
    uint8_t  button;    // 1 = jump, 2 = left, 3 = right
    bool     player2;
    bool     down;      // true = press, false = release
};

// State machine for recording clicks in practice mode.
//
// Two buffers:
//   confirmed — clicks up to the last checkpoint (validated)
//   pending   — clicks after the last checkpoint (discarded on death)
//
// When the player places a new checkpoint, pending -> confirmed.
// When the player dies, pending is dropped.
// When the level is completed or the player quits, if any clicks exist,
// auto-save can write them all to disk.
class Recorder {
public:
    // Toggle recording.
    void start();
    void stop();
    [[nodiscard]] bool isRecording() const { return m_recording; }

    // Called every PlayLayer::update tick.
    void tickFrame();

    // Reset everything (on PlayLayer::init / resetLevel).
    void reset();

    // Hook entry points.
    void onPress  (uint8_t button, bool player2, float playerX, float playerY);
    void onRelease(uint8_t button, bool player2, float playerX, float playerY);
    void onCheckpointPlaced();
    void onPlayerDeath();

    // Save confirmed + pending to a .gdph JSON file.
    bool save(const std::filesystem::path& path,
              uint32_t levelId, const std::string& levelName) const;

    // True if there's anything worth saving (confirmed or pending non-empty).
    [[nodiscard]] bool hasData() const {
        return !m_confirmed.empty() || !m_pending.empty();
    }

    // Stats for the UI.
    [[nodiscard]] size_t confirmedCount() const { return m_confirmed.size(); }
    [[nodiscard]] size_t pendingCount()   const { return m_pending.size();   }
    [[nodiscard]] uint64_t currentFrame() const { return m_currentFrame; }

private:
    bool     m_recording    = false;
    uint64_t m_currentFrame = 0;
    std::vector<ClickRecord> m_confirmed;
    std::vector<ClickRecord> m_pending;
};

} // namespace macrotrainer
