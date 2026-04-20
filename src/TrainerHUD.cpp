#include "TrainerHUD.hpp"

#include <Geode/Geode.hpp>

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
    buildBars(data);
    return true;
}

namespace {
    // Parse the display-mode string from settings.
    enum class DisplayMode { Bars, Marks, StartEnd };

    DisplayMode readDisplayMode() {
        if (auto mod = Mod::get()) {
            try {
                auto s = mod->getSettingValue<std::string>("display-mode");
                if (s == "marks")     return DisplayMode::Marks;
                if (s == "start-end") return DisplayMode::StartEnd;
            } catch (...) {}
        }
        return DisplayMode::Bars;
    }

    // Helper: create a thin vertical line at world X with given color/alpha.
    CCLayerColor* makeMark(float x, float yBottom, float height,
                           ccColor3B color, GLubyte alpha)
    {
        auto bar = CCLayerColor::create({
            color.r, color.g, color.b, alpha
        });
        bar->setContentSize({ 3.f, height });
        bar->setAnchorPoint({ 0.5f, 0.f });
        bar->setPosition({ x, yBottom });
        return bar;
    }
}

void TrainerHUD::buildBars(const GdphData& data) {
    const float screenH = CCDirector::sharedDirector()->getWinSize().height;

    // Bars need to be MUCH taller than the visible screen because
    // m_objectLayer's Y can move a lot in levels with extreme camera
    // pans (e.g. Yatagarasu portal sections). 16x the screen, anchored
    // far below visible area, covers any reasonable camera movement.
    // OpenGL will clip out the off-screen parts, so the GPU cost of
    // making them this tall is negligible.
    const float barH       = screenH * 16.f;
    const float barYBottom = -screenH * 7.5f;

    // Read all the settings (with sane defaults if reading fails).
    ccColor3B tapColor   = ccc3(255, 255, 255);
    ccColor3B holdColor  = ccc3(255, 150, 0);
    int       tapAlpha   = 220;
    int       holdAlpha  = 110;
    float     holdThresh = 30.f;
    DisplayMode mode     = readDisplayMode();

    if (auto mod = Mod::get()) {
        try { tapColor   = mod->getSettingValue<ccColor3B>("tap-color"); }   catch (...) {}
        try { holdColor  = mod->getSettingValue<ccColor3B>("hold-color"); }  catch (...) {}
        try { tapAlpha   = mod->getSettingValue<int64_t>("tap-alpha"); }     catch (...) {}
        try { holdAlpha  = mod->getSettingValue<int64_t>("hold-alpha"); }    catch (...) {}
        try { holdThresh = static_cast<float>(mod->getSettingValue<double>("hold-threshold")); } catch (...) {}
    }

    const GLubyte tapA  = static_cast<GLubyte>(tapAlpha);
    const GLubyte holdA = static_cast<GLubyte>(holdAlpha);

    int barsAdded = 0;

    for (const auto& iv : data.intervals) {
        switch (mode) {
        case DisplayMode::Marks: {
            // Just a thin line at the start of every click. End is ignored.
            this->addChild(makeMark(iv.startX, barYBottom, barH, tapColor, tapA));
            barsAdded++;
            break;
        }

        case DisplayMode::StartEnd: {
            // Two thin lines: start (tap color) and end (hold color).
            this->addChild(makeMark(iv.startX, barYBottom, barH, tapColor,  tapA));
            // Skip the end mark if it would overlap the start (very short tap).
            if (iv.endX - iv.startX > 1.f) {
                this->addChild(makeMark(iv.endX, barYBottom, barH, holdColor, holdA));
            }
            barsAdded += 2;
            break;
        }

        case DisplayMode::Bars:
        default: {
            const float w = iv.width();
            if (w < holdThresh) {
                // Short tap — thin bright line.
                this->addChild(makeMark(iv.startX, barYBottom, barH, tapColor, tapA));
                barsAdded++;
            } else {
                // Long hold — wide translucent rectangle.
                const float barWidth = std::max(6.f, w);
                auto bar = CCLayerColor::create({
                    holdColor.r, holdColor.g, holdColor.b, holdA
                });
                bar->setContentSize({ barWidth, barH });
                bar->setAnchorPoint({ 0.f, 0.f });
                bar->setPosition({ iv.startX, barYBottom });
                this->addChild(bar);

                // Bright left edge so the start moment is crisp.
                auto edge = CCLayerColor::create({
                    holdColor.r, holdColor.g, holdColor.b,
                    static_cast<GLubyte>(std::min(255, holdAlpha + 110))
                });
                edge->setContentSize({ 2.f, barH });
                edge->setAnchorPoint({ 0.f, 0.f });
                edge->setPosition({ iv.startX, barYBottom });
                this->addChild(edge);

                barsAdded += 2;
            }
            break;
        }
        }
    }

    log::info("TrainerHUD: rendered {} marks/bars from {} intervals (mode={})",
        barsAdded, data.intervals.size(),
        mode == DisplayMode::Marks    ? "marks" :
        mode == DisplayMode::StartEnd ? "start-end" : "bars");
}
