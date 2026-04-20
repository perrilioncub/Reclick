#include "Recorder.hpp"

#include <Geode/Geode.hpp>
#include <matjson.hpp>

#include <chrono>
#include <fstream>

using namespace macrotrainer;
using namespace geode::prelude;

void Recorder::start() {
    m_recording = true;
    log::info("Recorder: started");
}

void Recorder::stop() {
    m_recording = false;
    log::info("Recorder: stopped. Confirmed={}, Pending={}",
        m_confirmed.size(), m_pending.size());
}

void Recorder::tickFrame() {
    if (m_recording) {
        m_currentFrame++;
    }
}

void Recorder::reset() {
    m_currentFrame = 0;
    m_confirmed.clear();
    m_pending.clear();
}

void Recorder::onPress(uint8_t button, bool player2, float playerX, float playerY) {
    if (!m_recording) return;
    m_pending.push_back({ m_currentFrame, playerX, playerY, button, player2, true });
}

void Recorder::onRelease(uint8_t button, bool player2, float playerX, float playerY) {
    if (!m_recording) return;
    m_pending.push_back({ m_currentFrame, playerX, playerY, button, player2, false });
}

void Recorder::onCheckpointPlaced() {
    if (!m_recording) return;
    if (!m_pending.empty()) {
        log::info("Recorder: checkpoint promoted {} pending clicks",
            m_pending.size());
        m_confirmed.insert(
            m_confirmed.end(),
            std::make_move_iterator(m_pending.begin()),
            std::make_move_iterator(m_pending.end())
        );
        m_pending.clear();
    }
}

void Recorder::onPlayerDeath() {
    if (!m_recording) return;
    if (!m_pending.empty()) {
        log::info("Recorder: death — discarding {} pending clicks",
            m_pending.size());
        m_pending.clear();
    }
}

bool Recorder::save(const std::filesystem::path& path,
                    uint32_t levelId, const std::string& levelName) const
{
    matjson::Value root = matjson::Value::object();
    root["version"]   = 1;
    root["levelId"]   = levelId;
    root["levelName"] = levelName;
    root["tickRate"]  = 240;

    {
        using namespace std::chrono;
        auto t = system_clock::to_time_t(system_clock::now());
        std::tm tm{};
    #ifdef _WIN32
        gmtime_s(&tm, &t);
    #else
        gmtime_r(&t, &tm);
    #endif
        char buf[32];
        std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm);
        root["recordedAt"] = std::string(buf);
    }

    matjson::Value clicks = matjson::Value::array();
    auto emit = [&](const ClickRecord& c) {
        matjson::Value obj = matjson::Value::object();
        obj["frame"]  = static_cast<int64_t>(c.frame);
        obj["x"]      = c.x;
        obj["y"]      = c.y;
        obj["button"] = static_cast<int>(c.button);
        obj["p2"]     = c.player2;
        obj["down"]   = c.down;
        clicks.push(obj);
    };
    for (const auto& c : m_confirmed) emit(c);
    for (const auto& c : m_pending)   emit(c);
    root["clicks"] = clicks;

    try {
        // Make sure the target directory exists.
        std::error_code ec;
        std::filesystem::create_directories(path.parent_path(), ec);

        std::ofstream f(path);
        if (!f) {
            log::warn("Recorder: cannot open {} for writing", path.string());
            return false;
        }
        f << root.dump(2);
        log::info("Recorder: saved {} clicks to {}",
            m_confirmed.size() + m_pending.size(), path.string());
        return true;
    } catch (const std::exception& e) {
        log::warn("Recorder: save failed — {}", e.what());
        return false;
    }
}
