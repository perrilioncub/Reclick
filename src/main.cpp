#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <Geode/modify/PlayerObject.hpp>
#include <Geode/binding/PlayLayer.hpp>
#include <Geode/binding/PauseLayer.hpp>
#include <Geode/binding/PlayerObject.hpp>
#include <Geode/binding/GJGameLevel.hpp>
#include <Geode/binding/CheckpointObject.hpp>
#include <Geode/binding/GameObject.hpp>
#include <Geode/binding/ButtonSprite.hpp>
#include <Geode/binding/CCMenuItemSpriteExtra.hpp>
#include <Geode/ui/Notification.hpp>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commdlg.h>

#include "Recorder.hpp"
#include "GdphLoader.hpp"
#include "TrainerHUD.hpp"
#include "GdphPicker.hpp"

using namespace geode::prelude;
using namespace macrotrainer;

// ---------------------------------------------------------------------------
// Globals
// ---------------------------------------------------------------------------
namespace {
    Recorder               g_recorder;
    std::optional<GdphData> g_loadedGdph;      // loaded but not yet attached
    TrainerHUD*            g_currentHud = nullptr;

    std::pair<bool, bool> classifyPlayer(PlayerObject* who) {
        auto pl = PlayLayer::get();
        if (!pl) return { false, false };
        if (who == pl->m_player1) return { true, false };
        if (who == pl->m_player2) return { true, true };
        return { false, false };
    }

    std::optional<std::filesystem::path> saveFilePicker(const std::wstring& suggestedName) {
        wchar_t filename[MAX_PATH] = {0};
        wcsncpy_s(filename, suggestedName.c_str(), MAX_PATH - 1);

        OPENFILENAMEW ofn{};
        ofn.lStructSize = sizeof(ofn);
        ofn.lpstrFilter = L"Practice Helper (*.gdph)\0*.gdph\0All files (*.*)\0*.*\0";
        ofn.lpstrFile   = filename;
        ofn.nMaxFile    = MAX_PATH;
        ofn.lpstrTitle  = L"Save practice recording";
        ofn.lpstrDefExt = L"gdph";
        ofn.Flags       = OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;

        if (GetSaveFileNameW(&ofn)) {
            return std::filesystem::path(filename);
        }
        return std::nullopt;
    }

    std::filesystem::path autoSaveDir() {
        wchar_t buf[MAX_PATH];
        GetModuleFileNameW(nullptr, buf, MAX_PATH);
        return std::filesystem::path(buf).parent_path() / "gdph-recordings";
    }

    std::string sanitizeFilename(std::string name) {
        for (auto& c : name) {
            if (strchr("<>:\"/\\|?*", c)) c = '_';
            if (static_cast<unsigned char>(c) < 32) c = '_';
        }
        if (name.empty()) name = "unnamed";
        return name;
    }

    void tryAutoSave(PlayLayer* pl, const char* reason) {
        if (!pl || !pl->m_level) return;
        if (!g_recorder.hasData()) return;

        const bool enabled = Mod::get()->getSettingValue<bool>("auto-save");
        if (!enabled) return;

        const std::string levelName = pl->m_level->m_levelName;
        const uint32_t    levelId   = static_cast<uint32_t>(pl->m_level->m_levelID);

        auto dir  = autoSaveDir();
        auto path = dir / (sanitizeFilename(levelName) + ".gdph");
        const size_t total =
            g_recorder.confirmedCount() + g_recorder.pendingCount();

        if (g_recorder.save(path, levelId, levelName)) {
            log::info("Auto-save ({}): wrote {}", reason, path.string());
            Notification::create(
                fmt::format("Auto-saved {} clicks ({})", total, reason),
                NotificationIcon::Success,
                3.0f
            )->show();
        } else {
            log::warn("Auto-save ({}) failed for {}", reason, path.string());
            Notification::create(
                "Auto-save failed", NotificationIcon::Error, 3.0f
            )->show();
        }
    }

    // Build the HUD from g_loadedGdph and parent it to the PlayLayer's
    // scroll layer. Only attaches if the loaded .gdph is for THIS level
    // (matching levelId), so bars from one level don't bleed into another.
    void attachHudIfLoaded(PlayLayer* pl) {
        if (!g_loadedGdph.has_value() || !pl || !pl->m_level) return;

        const uint32_t currentLevelId =
            static_cast<uint32_t>(pl->m_level->m_levelID);

        if (g_loadedGdph->levelId != currentLevelId) {
            log::info(
                "HUD: loaded .gdph is for level {} ('{}') but current "
                "level is {}, not attaching",
                g_loadedGdph->levelId, g_loadedGdph->levelName,
                currentLevelId);
            return;
        }

        CCNode* scrollLayer = pl->m_objectLayer;
        if (!scrollLayer) {
            log::warn("HUD: no m_objectLayer; bars would not scroll");
            scrollLayer = pl;
        }

        g_currentHud = TrainerHUD::create(*g_loadedGdph, scrollLayer);
    }
}

// ---------------------------------------------------------------------------
// PlayerObject — capture button press/release into recorder
// ---------------------------------------------------------------------------
class $modify(TrainerPlayer, PlayerObject) {
    bool pushButton(PlayerButton btn) {
        bool result = PlayerObject::pushButton(btn);
        if (g_recorder.isRecording()) {
            auto [ours, p2] = classifyPlayer(this);
            if (ours) {
                g_recorder.onPress(
                    static_cast<uint8_t>(btn), p2,
                    this->getPositionX(), this->getPositionY());
            }
        }
        return result;
    }

    bool releaseButton(PlayerButton btn) {
        bool result = PlayerObject::releaseButton(btn);
        if (g_recorder.isRecording()) {
            auto [ours, p2] = classifyPlayer(this);
            if (ours) {
                g_recorder.onRelease(
                    static_cast<uint8_t>(btn), p2,
                    this->getPositionX(), this->getPositionY());
            }
        }
        return result;
    }
};

// ---------------------------------------------------------------------------
// PlayLayer — lifecycle, recorder hooks, HUD attach
// ---------------------------------------------------------------------------
class $modify(TrainerPlayLayer, PlayLayer) {
    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) return false;
        g_recorder.reset();
        // Any previous HUD was parented to an old scroll layer that's now
        // gone. Clear the dangling pointer and reattach if a .gdph is
        // still loaded (which only happens if the user kept playing
        // without quitting to menu).
        g_currentHud = nullptr;
        attachHudIfLoaded(this);
        return true;
    }

    void update(float dt) {
        PlayLayer::update(dt);
        g_recorder.tickFrame();

        // Keep the player-position line glued to the icon's center X, and
        // let the HUD pick up live setting changes (master opacity etc.).
        if (g_currentHud) {
            if (auto* p1 = this->m_player1) {
                g_currentHud->updatePlayerLineX(p1->getPositionX());
            }
            g_currentHud->maybeRefreshFromSettings(dt);
        }
    }

    CheckpointObject* markCheckpoint() {
        auto cp = PlayLayer::markCheckpoint();
        if (cp) g_recorder.onCheckpointPlaced();
        return cp;
    }

    void destroyPlayer(PlayerObject* player, GameObject* object) {
        PlayLayer::destroyPlayer(player, object);
        g_recorder.onPlayerDeath();
    }

    // In practice mode this fires on every respawn — must NOT reset the
    // recorder (see notes in v0.3.1).
    void resetLevel() {
        PlayLayer::resetLevel();
    }

    void levelComplete() {
        tryAutoSave(this, "complete");
        PlayLayer::levelComplete();
    }

    void onQuit() {
        tryAutoSave(this, "quit");
        g_recorder.stop();
        g_recorder.reset();

        // Clean up HUD properly — remove from its parent (scroll layer)
        // and drop both the HUD pointer and the loaded data so the next
        // level starts fresh. If the user wants to use the same recording
        // on another level, they can Load GDPH again (we could add a
        // setting to keep it across levels, but default is to clear).
        if (g_currentHud) {
            g_currentHud->removeFromParentAndCleanup(true);
            g_currentHud = nullptr;
        }
        g_loadedGdph.reset();

        PlayLayer::onQuit();
    }
};

// ---------------------------------------------------------------------------
// PauseLayer — Start/Stop + Save + Load GDPH
// ---------------------------------------------------------------------------
class $modify(TrainerPauseLayer, PauseLayer) {
    void customSetup() {
        PauseLayer::customSetup();

        auto winSize = CCDirector::sharedDirector()->getWinSize();

        auto menu = CCMenu::create();
        menu->setPosition({ winSize.width - 90.f, winSize.height - 30.f });
        menu->setAnchorPoint({ 1.f, 1.f });

        const bool rec = g_recorder.isRecording();

        // Row 0: Start/Stop Recording
        auto recSpr = ButtonSprite::create(
            rec ? "Stop Recording" : "Start Recording",
            "bigFont.fnt",
            rec ? "GJ_button_06.png" : "GJ_button_01.png",
            0.6f);
        recSpr->setScale(0.45f);
        auto recBtn = CCMenuItemSpriteExtra::create(
            recSpr, this, menu_selector(TrainerPauseLayer::onToggleRecording));
        recBtn->setPosition({ 0, 0 });
        menu->addChild(recBtn);

        // Row 1: Save
        auto saveSpr = ButtonSprite::create(
            "Save", "bigFont.fnt", "GJ_button_02.png", 0.6f);
        saveSpr->setScale(0.50f);
        auto saveBtn = CCMenuItemSpriteExtra::create(
            saveSpr, this, menu_selector(TrainerPauseLayer::onSave));
        saveBtn->setPosition({ 0, -36.f });
        menu->addChild(saveBtn);

        // Row 2: Load / Unload GDPH. Label switches based on state.
        const bool hasLoaded = g_loadedGdph.has_value();
        auto loadSpr = ButtonSprite::create(
            hasLoaded ? "Unload GDPH" : "Load GDPH",
            "bigFont.fnt",
            hasLoaded ? "GJ_button_06.png" : "GJ_button_04.png",
            0.6f);
        loadSpr->setScale(0.45f);
        auto loadBtn = CCMenuItemSpriteExtra::create(
            loadSpr, this,
            hasLoaded
                ? menu_selector(TrainerPauseLayer::onUnloadGdph)
                : menu_selector(TrainerPauseLayer::onLoadGdph));
        loadBtn->setPosition({ 0, -72.f });
        menu->addChild(loadBtn);

        // Row 3: status line — rec counters + currently-loaded gdph.
        std::string statusText = fmt::format("C:{} P:{}",
            g_recorder.confirmedCount(),
            g_recorder.pendingCount());
        if (hasLoaded) {
            statusText += fmt::format(" | {} bars",
                g_loadedGdph->intervals.size());
        }
        auto status = CCLabelBMFont::create(statusText.c_str(), "chatFont.fnt");
        status->setScale(0.55f);
        status->setAnchorPoint({ 1.f, 0.5f });
        status->setPosition({ -5.f, -108.f });
        menu->addChild(status);

        this->addChild(menu, 10);
    }

    void onToggleRecording(CCObject*) {
        if (g_recorder.isRecording()) {
            g_recorder.stop();
            FLAlertLayer::create(
                "Recording Stopped",
                fmt::format("{} confirmed clicks, {} pending",
                    g_recorder.confirmedCount(),
                    g_recorder.pendingCount()),
                "OK")->show();
        } else {
            g_recorder.start();
            FLAlertLayer::create(
                "Recording Started",
                "Clicks will be recorded. Place checkpoints to lock in "
                "progress. Pending clicks are discarded on death.\n\n"
                "<cr>Warning:</c> other mods that push buttons (e.g. xdBot) "
                "will also be recorded. Stop recording before letting a "
                "bot play.",
                "OK")->show();
        }
    }

    void onSave(CCObject*) {
        if (!g_recorder.hasData()) {
            FLAlertLayer::create("Nothing to Save",
                "No clicks have been recorded yet.", "OK")->show();
            return;
        }
        auto pl = PlayLayer::get();
        if (!pl || !pl->m_level) {
            FLAlertLayer::create("Save Failed",
                "Could not get current level info.", "OK")->show();
            return;
        }

        std::string levelName = pl->m_level->m_levelName;
        std::wstring wname(levelName.begin(), levelName.end());
        wname += L".gdph";
        for (auto& c : wname) {
            if (wcschr(L"<>:\"/\\|?*", c)) c = L'_';
        }

        auto pathOpt = saveFilePicker(wname);
        if (!pathOpt.has_value()) return;

        const uint32_t levelId =
            static_cast<uint32_t>(pl->m_level->m_levelID);

        if (g_recorder.save(*pathOpt, levelId, levelName)) {
            FLAlertLayer::create("Saved",
                fmt::format("Saved {} clicks to {}",
                    g_recorder.confirmedCount() + g_recorder.pendingCount(),
                    pathOpt->filename().string()),
                "OK")->show();
        } else {
            FLAlertLayer::create("Save Failed",
                "Could not write file. Check Geode log for details.",
                "OK")->show();
        }
    }

    void onLoadGdph(CCObject*) {
        // Open our in-game picker. The callback runs when the user
        // picks a file (or with empty path on cancel).
        auto picker = GdphPickerLayer::create([this](std::filesystem::path path) {
            if (path.empty()) return;  // user cancelled

            auto result = GdphLoader::load(path);
            if (std::holds_alternative<GdphLoadError>(result)) {
                FLAlertLayer::create("Load Failed",
                    std::get<GdphLoadError>(result).message, "OK")->show();
                return;
            }

            g_loadedGdph = std::get<GdphData>(result);
            log::info("Loaded .gdph: {} with {} intervals",
                g_loadedGdph->levelName,
                g_loadedGdph->intervals.size());

            if (auto pl = PlayLayer::get()) {
                if (g_currentHud) {
                    g_currentHud->removeFromParentAndCleanup(true);
                    g_currentHud = nullptr;
                }
                // Only attach if level matches. If it doesn't, warn the
                // user — the file is loaded but not displayed.
                const uint32_t currentId =
                    pl->m_level
                        ? static_cast<uint32_t>(pl->m_level->m_levelID)
                        : 0;
                if (g_loadedGdph->levelId == currentId) {
                    attachHudIfLoaded(pl);
                    FLAlertLayer::create("Loaded",
                        fmt::format("{}: {} intervals",
                            g_loadedGdph->levelName,
                            g_loadedGdph->intervals.size()),
                        "OK")->show();
                } else {
                    FLAlertLayer::create("Loaded (not shown)",
                        fmt::format(
                            "File loaded for <cy>{}</c> ({} intervals), "
                            "but you're on a different level. Bars will "
                            "only appear when you enter the matching level.",
                            g_loadedGdph->levelName,
                            g_loadedGdph->intervals.size()),
                        "OK")->show();
                }
            } else {
                FLAlertLayer::create("Loaded",
                    fmt::format("{}: {} intervals — will show next time "
                                "you enter that level.",
                        g_loadedGdph->levelName,
                        g_loadedGdph->intervals.size()),
                    "OK")->show();
            }
        });

        if (picker) picker->show();
    }

    // Unload the current .gdph: removes bars from screen and drops the
    // cached data so the next level won't show it.
    void onUnloadGdph(CCObject*) {
        if (g_currentHud) {
            g_currentHud->removeFromParentAndCleanup(true);
            g_currentHud = nullptr;
        }
        g_loadedGdph.reset();
        log::info("Unloaded .gdph");
        // No alert — the button label flipping from "Unload" back to
        // "Load" on next pause is enough feedback, and staying silent
        // keeps the pause flow snappy.
    }
};

$on_mod(Loaded) {
    log::info("Reclick loaded (v1.0.0-alpha.3)");
}
