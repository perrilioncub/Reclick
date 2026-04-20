#pragma once

#include <Geode/Geode.hpp>
#include <Geode/cocos/include/cocos2d.h>
#include "GdphLoader.hpp"

namespace macrotrainer {

// Renders vertical guide bars for every click interval in a loaded .gdph.
// Attaches itself to PlayLayer::m_objectLayer so the bars scroll with the
// level automatically — we don't need to move anything each frame.
//
// Style:
//   * Short taps (width < hold threshold): thin bright vertical line.
//   * Long holds (width >= threshold):     wide translucent rectangle
//                                          with a bright left edge.
class TrainerHUD : public cocos2d::CCNode {
public:
    // scrollLayer must be the PlayLayer's m_objectLayer (or another node
    // that scrolls with the camera). The HUD parents itself to it.
    static TrainerHUD* create(const GdphData& data, cocos2d::CCNode* scrollLayer);

    void setVisible(bool v) { CCNode::setVisible(v); }

protected:
    bool init(const GdphData& data);

private:
    void buildBars(const GdphData& data);

    // Width threshold in world px: below = short tap, above = hold.
    // Cube taps are usually ~15-40 px, holds (ship/wave) start around 30+.
    static constexpr float kHoldThreshold = 30.f;
};

} // namespace macrotrainer
