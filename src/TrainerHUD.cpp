#include "TrainerHUD.hpp"

#include <Geode/Geode.hpp>
#include <algorithm>

using namespace macrotrainer;
using namespace geode::prelude;
using namespace cocos2d;

TrainerHUD* TrainerHUD::create(const GdphData& data, CCNode* scrollLayer) {
    auto node = new TrainerHUD();
    if (node && node->init(data)) {
        node->autorelease();
        if (scrollLayer) {
            scrollLayer->addChild(node, 9999);
        }
        return node;
    }
    CC_SAFE_DELETE(node);
    return nullptr;
}

bool TrainerHUD::init(const GdphData& data) {
    if (!CCNode::init()) return false;
    m_data    = data;
    m_applied = readSettings();
    buildBars();
    buildPlayerLine();
    return true;
}

namespace {
    // Bars need to be MUCH taller than the visible screen because
    // m_objectLayer's Y can move a lot in levels with extreme camera
    // pans (e.g. Yatagarasu portal sections). 16x the screen, anchored
    // far below visible area, covers any reasonable camera movement.
    // OpenGL will clip out the off-screen parts, so the GPU cost of
    // making them this tall is negligible.
    float barHeight() {
        return CCDirector::sharedDirector()->getWinSize().height * 16.f;
    }
    float barYBottom() {
        return -CCDirector::sharedDirector()->getWinSize().height * 7.5f;
    }

    // Helper: create a thin vertical line at world X with given color/alpha.
    // The player line uses this same helper as the tap marks, so both share
    // any anchor/position quirks — "line touches mark" is therefore exactly
    // the press moment, with no systematic offset between the two.
    CCLayerColor* makeMark(float x, float yBottom, float height,
                           ccColor3B color, GLubyte alpha, float width = 3.f)
    {
        auto bar = CCLayerColor::create({
            color.r, color.g, color.b, alpha
        });
        bar->setContentSize({ width, height });
        bar->setAnchorPoint({ 0.5f, 0.f });
        bar->setPosition({ x, yBottom });
        return bar;
    }
}

TrainerHUD::DisplaySettings TrainerHUD::readSettings() {
    DisplaySettings s;  // defaults from the struct definition
    if (auto mod = Mod::get()) {
        try { s.mode        = mod->getSettingValue<std::string>("display-mode"); }      catch (...) {}
        try { s.tapColor    = mod->getSettingValue<ccColor3B>("tap-color"); }           catch (...) {}
        try { s.holdColor   = mod->getSettingValue<ccColor3B>("hold-color"); }          catch (...) {}
        try { s.lineColor   = mod->getSettingValue<ccColor3B>("player-line-color"); }   catch (...) {}
        try { s.tapAlpha    = static_cast<int>(mod->getSettingValue<int64_t>("tap-alpha")); }         catch (...) {}
        try { s.holdAlpha   = static_cast<int>(mod->getSettingValue<int64_t>("hold-alpha")); }        catch (...) {}
        try { s.lineAlpha   = static_cast<int>(mod->getSettingValue<int64_t>("player-line-alpha")); } catch (...) {}
        try { s.masterPct   = static_cast<int>(mod->getSettingValue<int64_t>("overlay-opacity")); }   catch (...) {}
        try { s.lineEnabled = mod->getSettingValue<bool>("player-line"); }              catch (...) {}
        try { s.holdThresh  = static_cast<float>(mod->getSettingValue<double>("hold-threshold")); }   catch (...) {}
    }
    return s;
}

bool TrainerHUD::sameSettings(const DisplaySettings& a, const DisplaySettings& b) {
    auto ceq = [](const ccColor3B& x, const ccColor3B& y) {
        return x.r == y.r && x.g == y.g && x.b == y.b;
    };
    return a.mode == b.mode
        && ceq(a.tapColor,  b.tapColor)
        && ceq(a.holdColor, b.holdColor)
        && ceq(a.lineColor, b.lineColor)
        && a.tapAlpha    == b.tapAlpha
        && a.holdAlpha   == b.holdAlpha
        && a.lineAlpha   == b.lineAlpha
        && a.masterPct   == b.masterPct
        && a.lineEnabled == b.lineEnabled
        && a.holdThresh  == b.holdThresh;
}

GLubyte TrainerHUD::scaledAlpha(int baseAlpha, int masterPct) {
    const int pct = std::clamp(masterPct, 0, 100);
    const int v   = baseAlpha * pct / 100;
    return static_cast<GLubyte>(std::clamp(v, 0, 255));
}

void TrainerHUD::buildBars() {
    const float barH = barHeight();
    const float yBot = barYBottom();
    const auto& s    = m_applied;

    enum class DisplayMode { Bars, Marks, StartEnd };
    DisplayMode mode = DisplayMode::Bars;
    if (s.mode == "marks")     mode = DisplayMode::Marks;
    if (s.mode == "start-end") mode = DisplayMode::StartEnd;

    // Master opacity scales every alpha in one place.
    const GLubyte tapA  = scaledAlpha(s.tapAlpha,  s.masterPct);
    const GLubyte holdA = scaledAlpha(s.holdAlpha, s.masterPct);

    int barsAdded = 0;

    for (const auto& iv : m_data.intervals) {
        switch (mode) {
        case DisplayMode::Marks: {
            // Just a thin line at the start of every click. End is ignored.
            this->addChild(makeMark(iv.startX, yBot, barH, s.tapColor, tapA));
            barsAdded++;
            break;
        }

        case DisplayMode::StartEnd: {
            // Two thin lines: start (tap color) and end (hold color).
            this->addChild(makeMark(iv.startX, yBot, barH, s.tapColor, tapA));
            // Skip the end mark if it would overlap the start (very short tap).
            if (iv.endX - iv.startX > 1.f) {
                this->addChild(makeMark(iv.endX, yBot, barH, s.holdColor, holdA));
            }
            barsAdded += 2;
            break;
        }

        case DisplayMode::Bars:
        default: {
            const float w = iv.width();
            if (w < s.holdThresh) {
                // Short tap — thin bright line.
                this->addChild(makeMark(iv.startX, yBot, barH, s.tapColor, tapA));
                barsAdded++;
            } else {
                // Long hold — wide translucent rectangle.
                const float barWidth = std::max(6.f, w);
                auto bar = CCLayerColor::create({
                    s.holdColor.r, s.holdColor.g, s.holdColor.b, holdA
                });
                bar->setContentSize({ barWidth, barH });
                bar->setAnchorPoint({ 0.f, 0.f });
                bar->setPosition({ iv.startX, yBot });
                this->addChild(bar);

                // Bright left edge so the start moment is crisp.
                auto edge = CCLayerColor::create({
                    s.holdColor.r, s.holdColor.g, s.holdColor.b,
                    scaledAlpha(std::min(255, s.holdAlpha + 110), s.masterPct)
                });
                edge->setContentSize({ 2.f, barH });
                edge->setAnchorPoint({ 0.f, 0.f });
                edge->setPosition({ iv.startX, yBot });
                this->addChild(edge);

                barsAdded += 2;
            }
            break;
        }
        }
    }

    log::info("TrainerHUD: rendered {} marks/bars from {} intervals "
              "(mode={}, master={}%)",
        barsAdded, m_data.intervals.size(), s.mode, s.masterPct);
}

void TrainerHUD::buildPlayerLine() {
    m_playerLine = nullptr;
    const auto& s = m_applied;
    if (!s.lineEnabled) return;

    const GLubyte a = scaledAlpha(s.lineAlpha, s.masterPct);
    if (a == 0) return;

    // Slightly thinner than tap marks (2px vs 3px) and its own color, so
    // it reads as "you" rather than "a click". Drawn above the bars.
    m_playerLine = makeMark(m_lastPlayerX, barYBottom(), barHeight(),
                            s.lineColor, a, 2.f);
    this->addChild(m_playerLine, 10);
}

void TrainerHUD::updatePlayerLineX(float x) {
    m_lastPlayerX = x;
    if (m_playerLine) {
        m_playerLine->setPositionX(x);
    }
}

void TrainerHUD::maybeRefreshFromSettings(float dt) {
    m_settingsPollAcc += dt;
    if (m_settingsPollAcc < 0.25f) return;
    m_settingsPollAcc = 0.f;

    auto s = readSettings();
    if (sameSettings(m_applied, s)) return;

    m_applied = s;
    rebuild();
}

void TrainerHUD::rebuild() {
    this->removeAllChildrenWithCleanup(true);
    m_playerLine = nullptr;
    buildBars();
    buildPlayerLine();
}
