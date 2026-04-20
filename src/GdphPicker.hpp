#pragma once

#include <Geode/Geode.hpp>
#include <Geode/cocos/include/cocos2d.h>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace macrotrainer {

// One scanned .gdph file with metadata pre-extracted from the JSON.
struct GdphEntry {
    std::filesystem::path path;       // full path on disk
    std::string           levelName;  // from JSON, fallback to filename
    int                   intervals;  // approximate count of press-release pairs
    std::string           recordedAt; // ISO timestamp from JSON, may be empty
    uintmax_t             fileSize;   // bytes on disk
};

// Modal layer that shows a scrollable list of every .gdph in
// <GD>/gdph-recordings/. The user can either tap a row to load it, or
// tap the "i" button on the right for a details popup.
//
// Calls the user-provided callback with the chosen path, or with an
// empty path if cancelled.
class GdphPickerLayer : public cocos2d::CCLayer {
public:
    using OnPick = std::function<void(std::filesystem::path)>;

    static GdphPickerLayer* create(OnPick onPick);
    void show();

protected:
    bool init(OnPick onPick);

    // Builds the list rows.
    void rebuildList();

    // Click handlers.
    void onClose(cocos2d::CCObject*);
    void onPick(cocos2d::CCObject* sender);
    void onInfo(cocos2d::CCObject* sender);

private:
    OnPick                  m_onPick;
    std::vector<GdphEntry>  m_entries;
    cocos2d::CCNode*        m_listContainer = nullptr; // scrollable rows live here
};

} // namespace macrotrainer
