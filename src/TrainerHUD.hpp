#pragma once

#include <Geode/Geode.hpp>
#include <Geode/cocos/include/cocos2d.h>
#include "GdphLoader.hpp"

namespace macrotrainer {

// Renders vertical guide bars for every click interval in a loaded .gdph,
// plus (optionally) a vertical line locked to the player icon's center X
// so you can see the exact moment you cross a bar.
//
// Attaches itself to PlayLayer::m_objectLayer so the bars scroll with the
// level automatically — we don't need to move the bars each frame. The
// player line is the one exception: TrainerPlayLayer::update feeds it the
// player's X every frame via updatePlayerLineX().
//
// Style:
//   * Short taps (width < hold threshold): thin bright vertical line.
//   * Long holds (width >= threshold):     wide translucent rectangle
//                                          with a bright left edge.
//   * Player line: thin line through the icon's center X, own color.
//
// Every display setting (colors, alphas, mode, threshold, master opacity)
// is re-checked a few times per second while playing. If anything changed,
// the bars are rebuilt — so slider tweaks in Mod Settings apply live,
// no reload or level restart needed.
class TrainerHUD : public cocos2d::CCNode {
public:
    // scrollLayer must be the PlayLayer's m_objectLayer (or another node
    // that scrolls with the camera). The HUD parents itself to it.
    static TrainerHUD* create(const GdphData& data, cocos2d::CCNode* scrollLayer);

    void setVisible(bool v) { CCNode::setVisible(v); }

    // Called every frame with the player's world X (same coordinate space
    // the bars live in). Cheap — just moves one node.
    void updatePlayerLineX(float x);

    // Called every frame with dt; internally throttled to ~4 checks/sec.
    // Re-reads display settings and rebuilds if any of them changed.
    void maybeRefreshFromSettings(float dt);

protected:
    bool init(const GdphData& data);

private:
    // Snapshot of every setting that affects rendering. We poll and
    // compare snapshots instead of subscribing to per-setting listeners —
    // zero new API surface, and a handful of setting reads 4x/sec is
    // free compared to everything else a frame does.
    struct DisplaySettings {
        std::string        mode        = "bars";
        cocos2d::ccColor3B tapColor    = { 255, 255, 255 };
        cocos2d::ccColor3B holdColor   = { 255, 150,   0 };
        cocos2d::ccColor3B lineColor   = {   0, 255, 200 };
        int                tapAlpha    = 220;
        int                holdAlpha   = 110;
        int                lineAlpha   = 200;
        int                masterPct   = 100;   // 0-100, scales everything
        bool               lineEnabled = true;
        float              holdThresh  = 30.f;
    };

    static DisplaySettings readSettings();
    static bool sameSettings(const DisplaySettings& a, const DisplaySettings& b);

    // base 0-255 alpha scaled by master opacity percent, clamped.
    static GLubyte scaledAlpha(int baseAlpha, int masterPct);

    void rebuild();          // clear children, rebuild bars + player line
    void buildBars();
    void buildPlayerLine();

    GdphData        m_data;      // kept so we can rebuild live
    DisplaySettings m_applied;   // the settings the current build used

    cocos2d::CCLayerColor* m_playerLine = nullptr;
    float m_lastPlayerX     = 0.f;
    float m_settingsPollAcc = 0.f;
};

} // namespace macrotrainer
