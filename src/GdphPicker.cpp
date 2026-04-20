#include "GdphPicker.hpp"

#include <Geode/Geode.hpp>
#include <Geode/binding/ButtonSprite.hpp>
#include <Geode/binding/CCMenuItemSpriteExtra.hpp>
#include <Geode/ui/ScrollLayer.hpp>
#include <matjson.hpp>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <fstream>
#include <sstream>
#include <algorithm>

using namespace macrotrainer;
using namespace geode::prelude;
using namespace cocos2d;

namespace {
    // <GD install dir>/gdph-recordings
    std::filesystem::path recordingsDir() {
        wchar_t buf[MAX_PATH];
        GetModuleFileNameW(nullptr, buf, MAX_PATH);
        return std::filesystem::path(buf).parent_path() / "gdph-recordings";
    }

    // Quick-and-dirty header read: parse just enough JSON to extract
    // levelName, recordedAt, and clicks count without loading every click.
    GdphEntry inspectFile(const std::filesystem::path& p) {
        GdphEntry e;
        e.path      = p;
        e.levelName = p.stem().string();
        e.intervals = 0;
        e.fileSize  = 0;

        std::error_code ec;
        e.fileSize = std::filesystem::file_size(p, ec);

        try {
            std::ifstream f(p);
            if (!f) return e;
            std::stringstream ss;
            ss << f.rdbuf();
            auto parsed = matjson::parse(ss.str());
            if (!parsed) return e;
            auto root = parsed.unwrap();

            if (root.contains("levelName")) {
                e.levelName = root["levelName"].asString().unwrapOr(e.levelName);
            }
            if (root.contains("recordedAt")) {
                e.recordedAt = root["recordedAt"].asString().unwrapOr("");
            }
            if (root.contains("clicks")) {
                // intervals = roughly half the click count (press+release pairs)
                int n = 0;
                for (const auto& _ : root["clicks"]) (void)_, n++;
                e.intervals = n / 2;
            }
        } catch (...) {
            // Don't let a bad file break the whole list.
        }
        return e;
    }
}

// ---------------------------------------------------------------------------
// GdphPickerLayer
// ---------------------------------------------------------------------------

GdphPickerLayer* GdphPickerLayer::create(OnPick onPick) {
    auto node = new GdphPickerLayer();
    if (node && node->init(std::move(onPick))) {
        node->autorelease();
        return node;
    }
    CC_SAFE_DELETE(node);
    return nullptr;
}

void GdphPickerLayer::show() {
    auto scene = CCDirector::sharedDirector()->getRunningScene();
    if (!scene) return;
    scene->addChild(this, 1000);  // very high z so we're above the pause menu
}

bool GdphPickerLayer::init(OnPick onPick) {
    if (!CCLayer::init()) return false;
    m_onPick = std::move(onPick);

    const auto winSize = CCDirector::sharedDirector()->getWinSize();

    // Dim background — captures clicks so the layer below doesn't get them.
    auto dim = CCLayerColor::create({ 0, 0, 0, 180 });
    dim->setContentSize(winSize);
    dim->setPosition(0, 0);
    this->addChild(dim);

    // Panel size & origin (centered).
    const float panelW = 460.f;
    const float panelH = 280.f;
    const float panelX = (winSize.width  - panelW) * 0.5f;
    const float panelY = (winSize.height - panelH) * 0.5f;

    // Panel background.
    auto panel = CCLayerColor::create({ 30, 30, 35, 240 });
    panel->setContentSize({ panelW, panelH });
    panel->setPosition({ panelX, panelY });
    this->addChild(panel);

    // Title.
    auto title = CCLabelBMFont::create("Choose a Recording", "bigFont.fnt");
    title->setScale(0.55f);
    title->setAnchorPoint({ 0.5f, 1.f });
    title->setPosition({ panelX + panelW * 0.5f, panelY + panelH - 8.f });
    this->addChild(title);

    // Close button (top right of panel).
    auto closeMenu = CCMenu::create();
    closeMenu->setPosition({ panelX + panelW - 18.f, panelY + panelH - 18.f });
    auto closeSpr = CCSprite::createWithSpriteFrameName("GJ_closeBtn_001.png");
    if (!closeSpr) {
        // Fallback if sprite frame missing in this version.
        auto bs = ButtonSprite::create("X", "bigFont.fnt", "GJ_button_06.png", 0.6f);
        bs->setScale(0.5f);
        auto closeBtn = CCMenuItemSpriteExtra::create(
            bs, this, menu_selector(GdphPickerLayer::onClose));
        closeMenu->addChild(closeBtn);
    } else {
        closeSpr->setScale(0.6f);
        auto closeBtn = CCMenuItemSpriteExtra::create(
            closeSpr, this, menu_selector(GdphPickerLayer::onClose));
        closeMenu->addChild(closeBtn);
    }
    this->addChild(closeMenu);

    // List container (we scan files first, then build rows).
    const float listX = panelX + 10.f;
    const float listY = panelY + 30.f;
    const float listW = panelW - 20.f;
    const float listH = panelH - 60.f;

    auto listBg = CCLayerColor::create({ 15, 15, 20, 200 });
    listBg->setContentSize({ listW, listH });
    listBg->setPosition({ listX, listY });
    this->addChild(listBg);

    m_listContainer = CCNode::create();
    m_listContainer->setContentSize({ listW, listH });
    m_listContainer->setPosition({ listX, listY });
    this->addChild(m_listContainer);

    rebuildList();
    return true;
}

void GdphPickerLayer::rebuildList() {
    m_entries.clear();
    m_listContainer->removeAllChildrenWithCleanup(true);

    auto dir = recordingsDir();
    if (!std::filesystem::exists(dir)) {
        // Show a "no folder" message.
        auto label = CCLabelBMFont::create(
            "No 'gdph-recordings' folder yet.\nRecord and save a level first.",
            "chatFont.fnt");
        label->setScale(0.7f);
        label->setAnchorPoint({ 0.5f, 0.5f });
        label->setAlignment(kCCTextAlignmentCenter);
        label->setPosition({
            m_listContainer->getContentSize().width  * 0.5f,
            m_listContainer->getContentSize().height * 0.5f
        });
        m_listContainer->addChild(label);
        return;
    }

    // Collect every .gdph in the folder.
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
        if (!entry.is_regular_file()) continue;
        auto path = entry.path();
        if (path.extension() != ".gdph") continue;
        m_entries.push_back(inspectFile(path));
    }

    // Sort by recordedAt descending (newest first), then by filename.
    std::sort(m_entries.begin(), m_entries.end(),
        [](const GdphEntry& a, const GdphEntry& b) {
            if (a.recordedAt != b.recordedAt) return a.recordedAt > b.recordedAt;
            return a.path.filename() < b.path.filename();
        });

    if (m_entries.empty()) {
        auto label = CCLabelBMFont::create(
            "Folder is empty.", "chatFont.fnt");
        label->setScale(0.7f);
        label->setAnchorPoint({ 0.5f, 0.5f });
        label->setPosition({
            m_listContainer->getContentSize().width  * 0.5f,
            m_listContainer->getContentSize().height * 0.5f
        });
        m_listContainer->addChild(label);
        return;
    }

    // Lay out rows from top down. Simple list, no scrolling for now —
    // most users won't have more than ~10 recordings in this folder.
    const float listW   = m_listContainer->getContentSize().width;
    const float listH   = m_listContainer->getContentSize().height;
    const float rowH    = 30.f;
    const float padding = 4.f;

    auto rowMenu = CCMenu::create();
    rowMenu->setPosition({ 0, 0 });
    rowMenu->setContentSize({ listW, listH });
    m_listContainer->addChild(rowMenu);

    for (size_t i = 0; i < m_entries.size(); ++i) {
        const auto& e = m_entries[i];
        const float rowY = listH - (i + 1) * (rowH + padding);
        if (rowY < -rowH) break;  // overflow — TODO add proper scrolling

        // Row background (alternating shades).
        auto rowBg = CCLayerColor::create(
            (i % 2 == 0) ? ccc4(40, 40, 50, 220) : ccc4(30, 30, 40, 220));
        rowBg->setContentSize({ listW - 8.f, rowH });
        rowBg->setPosition({ 4.f, rowY });
        m_listContainer->addChild(rowBg);

        // Level name label (left side, clickable).
        auto nameSpr = ButtonSprite::create(
            e.levelName.c_str(), 200, true,
            "bigFont.fnt", "GJ_button_05.png", rowH - 4.f, 0.5f);
        nameSpr->setScale(0.7f);
        auto nameBtn = CCMenuItemSpriteExtra::create(
            nameSpr, this, menu_selector(GdphPickerLayer::onPick));
        nameBtn->setTag(static_cast<int>(i));
        nameBtn->setAnchorPoint({ 0.f, 0.5f });
        nameBtn->setPosition({ 8.f, rowY + rowH * 0.5f });
        rowMenu->addChild(nameBtn);

        // Info "i" button (right side).
        auto infoSpr = ButtonSprite::create(
            "i", "bigFont.fnt", "GJ_button_04.png", 0.6f);
        infoSpr->setScale(0.55f);
        auto infoBtn = CCMenuItemSpriteExtra::create(
            infoSpr, this, menu_selector(GdphPickerLayer::onInfo));
        infoBtn->setTag(static_cast<int>(i));
        infoBtn->setAnchorPoint({ 1.f, 0.5f });
        infoBtn->setPosition({ listW - 12.f, rowY + rowH * 0.5f });
        rowMenu->addChild(infoBtn);
    }
}

void GdphPickerLayer::onClose(CCObject*) {
    if (m_onPick) m_onPick({});  // empty path = cancel
    this->removeFromParentAndCleanup(true);
}

void GdphPickerLayer::onPick(CCObject* sender) {
    auto* btn = static_cast<CCMenuItemSpriteExtra*>(sender);
    int idx = btn->getTag();
    if (idx < 0 || idx >= static_cast<int>(m_entries.size())) return;

    auto path = m_entries[idx].path;
    if (m_onPick) m_onPick(path);
    this->removeFromParentAndCleanup(true);
}

void GdphPickerLayer::onInfo(CCObject* sender) {
    auto* btn = static_cast<CCMenuItemSpriteExtra*>(sender);
    int idx = btn->getTag();
    if (idx < 0 || idx >= static_cast<int>(m_entries.size())) return;

    const auto& e = m_entries[idx];
    std::string body = fmt::format(
        "<cy>Level:</c> {}\n"
        "<cy>Intervals:</c> ~{}\n"
        "<cy>File size:</c> {} bytes\n"
        "<cy>Recorded:</c> {}\n"
        "<cy>Path:</c> {}",
        e.levelName,
        e.intervals,
        e.fileSize,
        e.recordedAt.empty() ? "(unknown)" : e.recordedAt,
        e.path.string()
    );

    FLAlertLayer::create(e.levelName.c_str(), body, "OK")->show();
}
