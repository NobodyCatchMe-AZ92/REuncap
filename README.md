# REuncap v1.0

Smooth high frame rates (60, 120, 144, 190, your monitor's refresh rate, or uncapped) for
**Resident Evil** (1996), **Resident Evil 2** (1998) and **Resident Evil 3** (1999) on PC with
Classic REbirth, without speeding the games up.

All three games keep running their logic at the original 30 ticks per second, so timing, physics,
AI, animation, door and cutscene speed are untouched. Between ticks the mod shows extra frames in
which everything that moves is drawn at the matching point in time between the previous tick and
the current one:

- characters, enemies and all other 3D models
- shadows and blood pools
- effects: shell casings, blood, muzzle flashes, sparks

One file, `reuncap.asi`, works for all three games; it detects which game it is running in.
Menus, inventory and file screens are left as they are.

## Requirements
| Game | Executable | Classic REbirth |
|---|---|---|
| Resident Evil | Japanese MediaKite `Biohazard.exe` (the one Classic REbirth uses) | 1.1.4 (recommended) or 1.1.3 |
| Resident Evil 2 | Sourcenext 1.10 `bio2 1.10.exe` | 1.0.9 |
| Resident Evil 3 | Sourcenext 1.1.0 `BIOHAZARD(R) 3 PC.exe` | 1.0.3 |

If the game or Classic REbirth version is not one of these, the mod writes that to
`reuncap.log` and changes nothing.

## Install
1. Open the folder for your game in this download (`Resident Evil`, `Resident Evil 2` or
   `Resident Evil 3`).
2. Copy **everything inside it** into your game's install folder (the folder with the game's
   `.exe`).
3. Start the game:
   - **Resident Evil / Resident Evil 3:** tick **REuncap** in the Mod Selection window.
     In Resident Evil you can tick it together with other mods (e.g. Seamless HD Project) and save
     the selection as a preset.
   - **Resident Evil 2:** nothing to tick - it loads automatically, so it also works together
     with a Classic REbirth mod such as BioRand. It needs an ASI loader; Seamless HD Project for
     RE2 includes one (`dsound.dll`).

That's it. Settings are in `reuncap.ini` (next to the mod), and **F12** switches the mod on and
off while playing - handy for comparing with the original 30 fps.

To uninstall, untick the mod or delete what you copied.

**Resident Evil with Classic REbirth 1.1.3** (older version, no Mod Selection modules): copy
`reuncap.asi` and `reuncap.ini` from `Resident Evil\mod_REuncap` directly next to
`Biohazard.exe` instead. This needs an ASI loader such as Ultimate ASI Loader (included with
Seamless HD Project as `dinput8.dll`).

## Settings (`reuncap.ini`)
Each setting is explained in the ini itself. The main ones:
- **FpsCap**: `-1` = your monitor's refresh rate (default), `0` = uncapped, or any number such as
  `60`, `120`, `144`, `165`, `190`. With G-Sync/FreeSync, a cap a few fps below the refresh rate
  gives the most even pacing (e.g. 190 on a 200 Hz monitor).
- **ToggleKey**: in-game on/off key, F12 by default.
- **RE2Pacing** (Resident Evil 2 only): `0` runs at your FpsCap exactly; `1` rounds it down to a
  multiple of 30 with perfectly even frame spacing. Use `1` if you see slight stutter in RE2 or if
  you don't use G-Sync/FreeSync.
- **RE2SmoothCameraCuts** (Resident Evil 2 only, default `1`): RE2 freezes the picture for two ticks
  (~100 ms) at every camera change, a PlayStation leftover. `1` shortens that to the one tick the
  game really needs; `0` keeps the original behaviour.

## Good to know
- **Game speed never depends on the frame rate.** An in-between frame is only drawn if it can be
  finished before the next game tick is due, so on a slower PC, or during a dip, you simply get
  fewer in-between frames (e.g. 120 -> 90 fps) while the game keeps its 30 ticks per second.
- **Camera cuts** still show a short hold in all three games. In RE2 the new camera's first tick
  cannot be interpolated (there is no earlier pose from that camera). In RE3 the hold is Classic
  REbirth loading the new HD background (~90 ms), exactly as without the mod.
- **Resident Evil 3 and FreeSync/G-Sync:** launch `BIOHAZARD(R) 3 PC.exe`. Variable refresh may not
  engage when the game is started through another executable name (e.g. a lossless-music launcher).
- 2D effects are positioned on the games' 320x240 grid, so at very high frame rates their
  in-between positions move in whole game pixels.
- Display latency is about the same as the original: the newest game state appears at the end of
  each tick, plus about a millisecond.

## How it works (short)
- **Resident Evil / Resident Evil 2** draw every 3D object from an ordering table at present time,
  each object carrying its own model->view matrix. Each tick the mod records those matrices plus
  shadow and effect positions, then redraws the ordering table as many times as the frame cap
  allows with blended matrices (rotation re-orthonormalised, translation interpolated) and shifted
  effect sprites, restoring all game state after every extra frame.
- **Resident Evil 3** (Classic REbirth 1.0.3) projects models to the screen during the tick. For
  skinned characters the mod blends the joint matrices and re-runs Classic REbirth's own
  projection; other models, shadows and effects are matched to the previous tick's polygons and
  blended on screen. Each extra frame is drawn by Classic REbirth's own renderer, after which
  everything is put back.
- The game's own frame is always presented last, before the next tick is due. Camera cuts and
  teleports are detected and never blended.

## Reporting problems
Please open an issue on GitHub and include:
1. **`reuncap.log`** from a session where the problem happened. Before playing, set `DebugLog=1` in
   `reuncap.ini` so the log records frame rate, pacing and tick-rate statistics. The log sits next to
   `reuncap.ini` (in `mod_REuncap` for Resident Evil / Resident Evil 3, next to the game's `.exe` for
   Resident Evil 2).
2. Which game, which Classic REbirth version and which other mods you use (e.g. Seamless HD Project,
   BioRand).
3. What you saw, where it happens, and whether it goes away when you press **F12** (mod off).
   A screenshot or short clip helps a lot.

## Building from source
Requirements: Windows, Visual Studio 2019 or 2022 with "Desktop development with C++", Python 3 (`py`)
and a bash shell (e.g. Git Bash) for packaging.

```
build.bat                  # -> build/reuncap.asi (32-bit)
bash tools/package.sh      # -> release/REuncap/ (README, per-game folders, default ini files)
```

`src/reuncap.cpp` holds the shared pacing/blending code and the Resident Evil 1 hooks;
`src/re2.inl` holds Resident Evil 2 support, `src/re3_base.inl` and `src/re3.inl` Resident Evil 3.

## Credits
- **Classic REbirth** by Gemini and its contributors - the community patch that runs Resident Evil 1-3
  on modern PCs; REuncap is built on top of it.
- **Resident Evil PC decompilation** by **ecruells** (github.com/ecruells/resident-evil-pc-decomp) -
  the reconstruction of the 1997 PC executable that the Resident Evil 1 reverse engineering was based on.
- **BioRand** (github.com/biorand/classic) - its source was used to make sure REuncap stays clear of
  BioRand's patches in Resident Evil 2.
- **Seamless HD Project** team - REuncap was developed and tested alongside their HD backgrounds and
  text fixes.
- **Ultimate ASI Loader** by ThirteenAG - loads REuncap in Resident Evil 2 (and Resident Evil with
  Classic REbirth 1.1.3).
- Built with AI assistance (Claude) together with the mod author. Resident Evil 2 and 3 were reverse
  engineered from the game executables and Classic REbirth's runtime behaviour.
- Resident Evil is a trademark of Capcom Co., Ltd. This is an unofficial fan project; no game files
  are included.
