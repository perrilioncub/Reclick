# Reclick

A practice-mode click recorder and visualizer for Geometry Dash, built
with the [Geode SDK](https://geode-sdk.org/).

Reclick records every click you make in practice mode and saves it as
a `.gdph` file. Loading a `.gdph` later draws vertical guide bars over
the level at the exact world positions where you clicked, so you can
study runs, fix bad timings, or share a strategy with someone else.

**The mod never presses any buttons for you.** It is purely a visual
overlay.

## ⚠ Author's note on legitimacy

Reclick is a powerful learning tool, and like most powerful tools it
can be misused. **The author considers a clear obtained by following
bars from your own past attempts NOT a legitimate clear.** If you
learn a level by recording yourself struggling through it in practice,
then completing it by mimicking those bars, you're not playing the
level — you're memorizing your own answer key. Publishing such a
record misrepresents your skill.

The intended workflow:

1. Build a macro in another tool, like **xdBot**.
2. Have that tool **replay** the macro through the level.
3. While the bot replays, Reclick records the resulting clicks into a
   `.gdph` file.
4. Use that `.gdph` as a **visual study aid** — see where the bot's
   inputs land, understand the timings, then practice without it.

Reclick deliberately doesn't press buttons or assist gameplay in any
way. What you do with the bars on screen is up to you and the
standards of your community.

## Quick start

1. Install the mod through the Geode mod browser (search for "Reclick"),
   or download the `.geode` file from
   [Releases](https://github.com/R-Darter/Reclick/releases) and drop it
   into `<GD>/geode/mods/`.
2. Launch a level. Open Practice Mode.
3. Pause. In the top right of the pause menu you'll see three new buttons:
   **Start Recording**, **Save**, **Load GDPH**.
4. Hit **Start Recording**. Play normally. Place checkpoints — they
   "lock in" your clicks so far. Death after a checkpoint discards
   anything you did since the checkpoint.
5. When you finish the level (or quit out of it), Reclick saves a
   `.gdph` file to `<GD>/gdph-recordings/<LevelName>.gdph` automatically.
   This can be turned off in mod settings.

## Loading a recording

Open any level → pause → **Load GDPH** → pick a file from the in-game
list. Vertical bars will appear at every X-position where you clicked
in the recording.

Recordings are tied to the level they were made on (via level ID), so
bars from one level never accidentally appear on a different level.
If you load a `.gdph` while on the wrong level, it gets remembered
but stays hidden until you enter the matching level.

## Unloading / switching recordings

The pause menu's **Load GDPH** button turns into **Unload GDPH** once
a recording is loaded. Hit it to clear the bars without leaving the
level.

Bars are also auto-cleared when you exit a level (pause → Menu), so
the next level always starts fresh.

To switch to a different recording mid-session, just Unload the
current one and Load a new one.

## Display modes

In **Mod Settings → Reclick → Display mode**:

- **Bars** (default): wide translucent rectangles for holds, thin lines
  for taps. Best for ship/wave/UFO sections.
- **Marks only**: a single thin line at the moment each click *starts*.
  Best for cube sections where holding doesn't matter.
- **Start+End marks**: thin lines at both the press and release of every
  click, no rectangles between.

Colors and opacity for each are also configurable.

## How it works

When you press a button, Reclick records the player's current world X/Y
coordinates. When you release, it records the X/Y at the moment of
release. The pair becomes one "interval".

To draw, Reclick attaches a child node to the level's scroll layer
(`PlayLayer::m_objectLayer`), so the bars stay anchored to world
coordinates and scroll naturally with the camera. There is no physics
simulation, no time math, no synchronization — the bars are placed at
the exact X where you actually pressed during recording. This works
regardless of speed portals, character size, game mode, or framerate.

## Limitations

- Recording captures any button press that goes through
  `PlayerObject::pushButton`. This includes inputs synthesized by other
  mods (xdBot etc), which is precisely why the
  "record-bot-replay-into-gdph" workflow above works.
- Hooks into `levelComplete` and `onQuit` to drive auto-save. If a
  third-party mod overrides these heavily, auto-save may be skipped.
  You can always Save manually from the pause menu.
- Visual only. Reclick will never make the player jump.

## File format

`.gdph` files are plain JSON, easy to inspect and edit. Header:

```json
{
  "version": 1,
  "levelId": 60978746,
  "levelName": "The Golden",
  "tickRate": 240,
  "recordedAt": "2026-04-18T18:18:37Z",
  "clicks": [
    { "frame": 0, "x": 1234.5, "y": 200.0, "button": 1, "p2": false, "down": true },
    { "frame": 0, "x": 1289.1, "y": 215.3, "button": 1, "p2": false, "down": false }
  ]
}
```

(Note: in the current alpha, `frame` is always 0. This will be fixed
in a future release. The `x` field is what actually matters for drawing.)

## Building from source

Requirements:
- [Geode SDK](https://geode-sdk.org/) 5.6.1
- [Geode CLI](https://docs.geode-sdk.org/getting-started/cli) 3.7+
- CMake 3.21+
- LLVM Clang (tested with 22.x via Scoop on Windows)
- MSVC 14.44 toolset (newer toolsets cause CMake compatibility issues)
- Ninja

On Windows:

```cmd
git clone https://github.com/R-Darter/Reclick.git
cd Reclick
build.bat
```

The script closes Geometry Dash if running, configures CMake on first
run, builds, and copies the `.geode` to your GD mods folder. Edit the
GD path at the bottom of `build.bat` if your install is elsewhere.

## License

MIT — see [LICENSE](LICENSE).

## Credits

Built with the Geode SDK by [geode-sdk](https://github.com/geode-sdk).
Inspired by xdBot's macro recording UX.
