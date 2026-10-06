# REuncap - Framerate uncapper for RE1, 2 and 3 Classic Rebirth

Smooth high frame rates (60, 120, your monitor's refresh rate, or uncapped) for
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

**Please help me to improve the mod by [Reporting Problems](#Reporting-problems) either here on
GitHub, in the moddb comments, or on the Reddit thread.**

## Requirements
| Game | Executable | Classic REbirth |
|---|---|---|
| Resident Evil | Japanese MediaKite `Biohazard.exe` (the one Classic REbirth uses) | 1.1.4 (recommended) or 1.1.3 |
| Resident Evil 2 | Sourcenext 1.10 executable (usually `bio2.exe` or `bio2 1.10.exe`) | 1.0.9.1 |
| Resident Evil 3 | Sourcenext 1.1.0 `BIOHAZARD(R) 3 PC.exe` | 1.0.3 |

The executable's file name does not matter (e.g. `bio2.exe` or `bio2 1.10.exe`): REuncap recognises
each game by its code, not by its name.

If the game or Classic REbirth version is not one of these, the mod writes that to
`reuncap.log` and changes nothing. **Running the mod on any version of Classic Rebirth that is older
than the versions listed above will probably cause issues** (for example, a bug for flickering
typewriter screens in RE2 was reported, but the issue was the user running less than 1.0.9.1
for their RE2 Classic Rebirth install).

**The Steam versions of RE1, RE2 and RE3 are supported, starting from REuncap v1.1**. Make sure Classic 
REbirth is set up for the Steam versions first - they have their own download, see [Steam versions](#steam-versions) 
below.

## Install
There are two downloads - pick the one for your version of the games:
- **`REuncap-v1.1-NONSTEAM.zip`** - the original PC releases (instructions below).
- **`REuncap-v1.1-STEAM.zip`** - the Steam re-releases (see [Steam versions](#steam-versions)).

1. Open the folder for your game in the download (`Resident Evil`, `Resident Evil 2` or
   `Resident Evil 3`).
2. Copy **everything inside it** into your game's install folder (the folder with the game's
   `.exe`). Allow it to replace `dsound.dll` if asked.
3. In the newly created `scripts` folder, open `reuncap.ini` and set your desired framerate with
   the `FpsCap=` option. For example: `FpsCap=60` for 60fps gameplay, or `FpsCap=120` for 120fps
   gameplay. Keep the default `FpsCap=-1` if you want the game to run at your monitor's refresh
   rate, or set `FpsCap=0` if you want to completely uncap the framerate above your monitor's refresh
   rate (`FpsCap=0` is not recommended as it will likely make the motion unsmooth on your monitor).
4. Start the game as usual. There is nothing to tick or select: REuncap loads automatically, so it
   works together with any Classic REbirth mod (e.g. Seamless HD Project, BioRand).

That's it. Everything the mod uses is in the `scripts` folder: settings are in
`scripts\reuncap.ini`, and **F12** switches the mod on and off while playing - handy for comparing
with the original 30 fps.

What gets installed:

| File | What it is |
|---|---|
| `scripts\reuncap.asi` | the mod |
| `scripts\reuncap.ini` | its settings (each setting is explained inside) |
| `scripts\reuncap.log` | written while playing |
| `dsound.dll` | Ultimate ASI Loader - loads everything in the `scripts` folder |
| `global.ini` | the loader's settings |
| `REuncap-UncappedFPSMod-README.txt` | a short reminder of where everything is |

**About `global.ini`:** in Resident Evil and Resident Evil 3 it tells the loader to load only from
the `scripts` folder. Older Seamless HD Project installs leave `bio1hd.asi` / `bio3hd.asi` next to
the game's `.exe`; current Classic REbirth versions have HD support built in and no longer use them,
and if a loader picks them up they cause problems (blurry HD backgrounds in Resident Evil, a
"dumping" message at start-up in Resident Evil 3). In Resident Evil 2 the setting is off, because
Seamless HD Project's `bio2hd.asi` there is still in use.

To uninstall, delete `scripts\reuncap.asi`, `scripts\reuncap.ini`, `scripts\reuncap.log` and
`REuncap-UncappedFPSMod-README.txt`. In Resident Evil and Resident Evil 3 also delete `dsound.dll` and
`global.ini` (unless another mod needs them). In Resident Evil 2 keep `dsound.dll` if you use
Seamless HD Project - it needs it too.

**Resident Evil with Classic REbirth 1.1.3** (older version): copy only the `scripts` folder. Your
existing ASI loader (Seamless HD Project's `dinput8.dll`) loads it from there, and `bio1hd.asi` is
still needed on 1.1.3, so do not copy `global.ini` or `dsound.dll`.

## Steam versions
Use **`REuncap-v1.1-STEAM.zip`**. Classic REbirth has to be set up for the Steam versions first:
- **Resident Evil / Resident Evil 3:** follow Classic REbirth's own Steam instructions.
- **Resident Evil 2:** follow this Steam guide:
  https://steamcommunity.com/sharedfiles/filedetails/?id=3701562809

Then drag and drop: copy the contents of the folder named after your game into the game's **main
Steam folder** (the one with the Steam launcher in it):

| Game | Copy the contents of | into |
|---|---|---|
| Resident Evil | `Resident Evil` | `steamapps\common\4249100_Biohazard` |
| Resident Evil 2 | `Resident Evil 2` | `steamapps\common\4249110_Biohazard2` |
| Resident Evil 3 | `Resident Evil 3` | `steamapps\common\4249120_Biohazard3` |

(`steamapps` is inside your Steam library folder, e.g. `C:\Program Files (x86)\Steam\steamapps`.)

The files land in the game's **`japanese`** folder, because on Steam that is where Classic REbirth runs
the game. Everything REuncap uses - `dsound.dll`, `global.ini` and the `scripts` folder - is located
there, e.g. settings are in `japanese\scripts\reuncap.ini`. Everything else - settings, F12,
uninstalling - works exactly as described above, inside the `japanese` folder.

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
- **G-Sync / FreeSync not kicking in?** Graphics drivers decide per program whether a game gets
  variable refresh, and these old games often are not recognised automatically - for example the
  Steam versions' executables, or a game started through another executable name such as a
  lossless-music launcher. Add the game's `.exe` as a program in your graphics settings (NVIDIA
  Control Panel > Manage 3D settings > Program Settings, or the NVIDIA app / AMD Software) and enable
  G-Sync / FreeSync for it - including windowed mode if you play in a window. The frame rate itself
  does not depend on this; only whether your monitor follows it.
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
   `scripts\reuncap.ini` so the log records frame rate, pacing and tick-rate statistics. The log is
   `scripts\reuncap.log` in the game's folder (in the `japanese` folder on Steam).
2. Which game, which version (Steam or not), which Classic REbirth version and which other mods you
   use (e.g. Seamless HD Project, BioRand).
3. What you saw, where it happens, and whether it goes away when you press **F12** (mod off).
   A screenshot or short clip helps a lot.

## Building from source
Requirements: Windows, Visual Studio 2019 or 2022 with "Desktop development with C++", Python 3 (`py`)
and a bash shell (e.g. Git Bash) for packaging.

```
build.bat                  # -> build/reuncap.asi (32-bit)
bash tools/package.sh      # -> release/REuncap-NONSTEAM/ and release/REuncap-STEAM/ (README, per-game folders)
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
- **Ultimate ASI Loader** by ThirteenAG (github.com/ThirteenAG/Ultimate-ASI-Loader, MIT licence) -
  bundled as `dsound.dll`; it loads REuncap in all three games. Its licence is included as
  `scripts\UltimateASILoader-LICENSE.txt`.
- Built with AI assistance (Claude) together with the mod author. Resident Evil 2 and 3 were reverse
  engineered from the game executables and Classic REbirth's runtime behaviour.
- Resident Evil is a trademark of Capcom Co., Ltd. This is an unofficial fan project; no game files
  are included.
