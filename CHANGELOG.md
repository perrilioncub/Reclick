# Changelog

All notable changes to Reclick will be documented here.

## [v1.0.0-alpha.2] - 2026-04-20

Bugfix + UX release.

### Fixed
- Bars from a previous level no longer appear when you enter a
  different level. The loaded `.gdph` is checked against the current
  level's ID, and the HUD is explicitly torn down on quit.

### Added
- **Unload GDPH** button in the pause menu. The Load button flips into
  Unload once a recording is loaded, letting you dismiss bars without
  leaving the level.
- Pause-menu status line now also shows how many bars are currently
  loaded (e.g. `C:5 P:2 | 287 bars`).
- Loading a `.gdph` while on a non-matching level shows an explanatory
  alert instead of silently mis-displaying.

## [v1.0.0-alpha.1] - 2026-04-18

First public alpha. Core recording and visualization works end-to-end.

### Added
- Practice-mode click recorder. Records every press and release as
  `(frame, x, y, button, p2, down)` events.
- Two-buffer state machine: clicks accumulate in a "pending" buffer
  until you place a checkpoint, then they're promoted to "confirmed".
  Death drops pending; confirmed survives.
- Auto-save to `<GD>/gdph-recordings/<LevelName>.gdph` on level
  completion or quit (toggleable in settings).
- Manual Save button in pause menu, with native Windows file dialog.
- `.gdph` JSON file format with level metadata and full click list.
- In-game GDPH picker — scans `gdph-recordings/`, shows a sortable
  list with per-row "info" buttons revealing details.
- HUD that draws guide bars at the world X-coordinates from a loaded
  `.gdph`. Bars are parented to `m_objectLayer` so they scroll
  naturally with the level.
- Three display modes: **Bars** (taps as lines, holds as rectangles),
  **Marks only** (only press start), **Start+End marks** (press and
  release, no rectangles).
- Configurable colors and opacity for tap and hold bars.
- Configurable hold threshold (px width above which a click counts
  as a hold rather than a tap).
- On-screen toast notification confirming auto-save.

### Known limitations
- The `frame` field in `.gdph` files is always `0` due to an
  un-hooked `update` method. The `x` field is the only one used for
  drawing, so this doesn't affect visuals.
- Multi-input duplicates: pressing mouse + spacebar simultaneously
  records two press events instead of one. Will be deduplicated in
  a future release.
- P1 and P2 (dual mode) clicks are recorded with a `p2` flag but
  rendered identically. P2 will get its own visual treatment later.
- No Mac/Linux/Android testing yet — Windows-only for now.
- Bots that synthesize button presses (xdBot etc.) get recorded too;
  stop recording before letting one play.
