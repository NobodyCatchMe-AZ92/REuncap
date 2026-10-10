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
Menus, inventory and file screens are left as they are. Download it from the [Releases](https://github.com/NobodyCatchMe-AZ92/REuncap/releases) page.

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
`reuncap.log` and changes nothing.

**Older Classic REbirth versions:** from v1.3, REuncap also runs on Classic REbirth 1.0.9 and 1.0.8
for Resident Evil 2 and on Classic REbirth 1.0.2 and 1.0.1 for Resident Evil 3. This support is very
basic, and no further support will be given for these versions: any bugs or issues on them are the
user's responsibility. Please upgrade to the latest Classic REbirth for Resident Evil 2 (1.0.9.1) and
Resident Evil 3 (1.0.3). **Running the mod on any other Classic Rebirth version older than the ones
listed above will probably cause issues.**

**The Steam versions of RE1, RE2 and RE3 are supported, starting from REuncap v1.1**. Make sure Classic 
REbirth is set up for the Steam versions first - either with Mulderload's Steam Enhancement Pack or with Classic REbirth's own Steam setup, see [Steam versions](#steam-versions) 
below.

## Install
There are two downloads - pick the one for your version of the games from the [Releases](https://github.com/NobodyCatchMe-AZ92/REuncap/releases) page:
- **`REuncap-v1.3-NONSTEAM.zip`** - the original PC releases (instructions below).
- **`REuncap-v1.3-STEAM.zip`** - the Steam re-releases (see [Steam versions](#steam-versions)).

1. Open the folder for your game in the download (`Resident Evil (Classic Rebirth 1.1.4)`,
   `Resident Evil (Classic Rebirth 1.1.3)`, `Resident Evil 2` or `Resident Evil 3`). For Resident
   Evil, pick the folder that matches your Classic REbirth version (right-click `ddraw.dll` in the game
   folder > Properties > Details shows it).
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
`scripts\reuncap.ini`. While playing, **Shift + `=`** switches the mod on and off - handy for comparing
with the original 30 fps - and **`=`** on its own changes the frame rate cap on the fly (30, 60, 90,
120, your monitor's refresh rate, uncapped). Numpad `+` works the same as `=`. A short message in the
lower right corner confirms each change.

What gets installed:

| File | What it is |
|---|---|
| `scripts\reuncap.asi` | the mod |
| `scripts\reuncap.ini` | its settings (each setting is explained inside) |
| `scripts\reuncap.log` | written while playing |
| `dsound.dll` | Ultimate ASI Loader - loads everything in the `scripts` folder |
| `global.ini` | the loader's settings |
| `REuncap-UncappedFPSMod-README.txt` | a short reminder of where everything is |
| `scripts\bio1hd.asi` | only in the Resident Evil Classic REbirth 1.1.3 folder: Seamless HD Project's HD loader, which 1.1.3 still needs |

**About `global.ini`:** in Resident Evil and Resident Evil 3 it tells the loader to load only from
the `scripts` folder. Older Seamless HD Project installs leave `bio1hd.asi` / `bio3hd.asi` next to
the game's `.exe`; current Classic REbirth versions have HD support built in and no longer use them,
and if a loader picks them up they cause problems (blurry HD backgrounds in Resident Evil, a
"dumping" message at start-up in Resident Evil 3). Classic REbirth 1.1.3 for Resident Evil still needs
`bio1hd.asi`, so the 1.1.3 folder brings a copy of it inside `scripts`, where the loader picks it up.
In Resident Evil 2 the setting is off, because Seamless HD Project's `bio2hd.asi` there is still in use.

To uninstall, delete `scripts\reuncap.asi`, `scripts\reuncap.ini`, `scripts\reuncap.log` and
`REuncap-UncappedFPSMod-README.txt`. In Resident Evil and Resident Evil 3 also delete `dsound.dll` and
`global.ini` (unless another mod needs them). In Resident Evil 2 keep `dsound.dll` if you use
Seamless HD Project - it needs it too.

**Resident Evil with Classic REbirth 1.1.3**: use the `Resident Evil (Classic Rebirth 1.1.3)` folder
and install it like the others. The `bio1hd.asi` next to `Biohazard.exe` is no longer loaded (the
copy in `scripts` is) - it can stay there or be deleted.

## Steam versions
Use **`REuncap-v1.3-STEAM.zip`**. Classic REbirth has to be set up for the Steam versions first, in one
of two ways - the package works with both:
- **Mulderload's Steam Enhancement Pack** (mulderland.com, the most popular installer): the game runs
  from the game's `rebirth` folder.
- **Classic REbirth's own Steam setup** (for Resident Evil 2 e.g. this Steam guide:
  https://steamcommunity.com/sharedfiles/filedetails/?id=3701562809): the game runs from the game's
  `japanese` folder.

Then drag and drop: copy the contents of the folder named after your game into the game's **main
Steam folder** (the one with the Steam launcher in it) and allow it to replace files if asked:

| Game | Copy the contents of | into |
|---|---|---|
| Resident Evil | `Resident Evil` | `steamapps\common\4249100_Biohazard` |
| Resident Evil 2 | `Resident Evil 2` | `steamapps\common\4249110_Biohazard2` |
| Resident Evil 3 | `Resident Evil 3` | `steamapps\common\4249120_Biohazard3` |

(`steamapps` is inside your Steam library folder, e.g. `C:\Program Files (x86)\Steam\steamapps`.)

Each game folder contains a `rebirth` part (for Mulderload's pack) and a `japanese` part (for Classic
REbirth's own setup); the part your setup doesn't use is simply ignored. Settings are in
`rebirth\scripts\reuncap.ini` or `japanese\scripts\reuncap.ini` respectively. Everything else - the
hotkeys, uninstalling - works as described above, inside that folder. With Mulderload's pack in
Resident Evil and Resident Evil 2, REuncap is loaded by the pack's own ASI loader, so only the
`scripts` folder is added there.

**Mulderload's Steam Enhancement Pack - recommended: turn dgVoodoo2 off.** With dgVoodoo2 on, REuncap
reaches only about 60 fps in Resident Evil and about 90 fps in Resident Evil 2. The package includes a
`MulderConfig.save.json` for both games with dgVoodoo2 already off (and MSAA off, which needs dgVoodoo2;
HD textures stay on). To apply it, run `MulderConfig.exe` in the game's Steam folder once and save.
According to Mulderload's readme, the Steam Overlay needs dgVoodoo2 in Classic REbirth (Steam play time
tracking works without it).

## Settings (`reuncap.ini`)
Each setting is explained in the ini itself. The main ones:
- **FpsCap**: `-1` = your monitor's refresh rate (default), `0` = uncapped, or any number such as
  `60`, `120`, `144`, `165`, `190` (from `30` up; `30` = no in-between frames). With G-Sync/FreeSync, a cap a few fps below the refresh rate
  gives the most even pacing (e.g. 190 on a 200 Hz monitor).
- **ToggleKey**: the in-game hotkey, `=` by default (numpad `+` works too; `123` = F12). Shift + key
  switches the mod on and off; the key alone steps the frame rate cap through 30, 60, 90, 120, your
  monitor's refresh rate and uncapped while playing (until the game is closed - `FpsCap` is what the
  game starts with).
- **ToggleMessage**: `1` (default) shows a short message in the lower right corner when the hotkey is
  pressed (e.g. "FPS set to 60 with hotkey."); `0` turns it off.
- **RE2Pacing** (Resident Evil 2 only): `0` runs at your FpsCap exactly; `1` rounds it down to a
  multiple of 30 with perfectly even frame spacing. Use `1` if you see slight stutter in RE2 or if
  you don't use G-Sync/FreeSync.
- **RE2SmoothCameraCuts** (Resident Evil 2 only, default `1`): RE2 freezes the picture for two ticks
  (~100 ms) at every camera change, a PlayStation leftover. `1` shortens that to the one tick the
  game really needs; `0` keeps the original behaviour.
- **RE2CutCatchUp** (Resident Evil 2 only, default `1`): the new camera angle starts smoothly - the
  old angle stays up a moment longer, then the new one plays from its first image and catches up with
  the game within ~130 ms. `0` shows the new angle's first image twice (a short 30 fps step).

The first line of `reuncap.ini` shows which REuncap version it came with (from v1.2 on). What changed
in each version is listed in `CHANGELOG.md`.

## Good to know
- **Game speed never depends on the frame rate.** An in-between frame is only drawn if it can be
  finished before the next game tick is due, so on a slower PC, or during a dip, you simply get
  fewer in-between frames (e.g. 120 -> 90 fps) while the game keeps its 30 ticks per second.
- **Camera cuts** still show a short hold in all three games, while the game loads the new
  background. In RE2 the new angle then starts smoothly (see `RE2CutCatchUp`). In RE3 the hold is
  Classic REbirth loading the new HD background (~90 ms), exactly as without the mod.
- **G-Sync / FreeSync not kicking in?** Graphics drivers decide per program whether a game gets
  variable refresh, and these old games often are not recognised automatically - for example the
  Steam versions' executables, or a game started through another executable name such as a
  lossless-music launcher. Add the game's `.exe` as a program in your graphics settings (NVIDIA
  Control Panel > Manage 3D settings > Program Settings, or the NVIDIA app / AMD Software) and enable
  G-Sync / FreeSync for it - including windowed mode if you play in a window. The frame rate itself
  does not depend on this; only whether your monitor follows it.
- 2D effects are positioned on the games' 320x240 grid, so at very high frame rates their
  in-between positions move in whole game pixels.
- The hotkey message is a small window on top of the game: it shows in windowed and borderless
  fullscreen, but not in exclusive fullscreen, and not in OBS "game capture" recordings.
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
   `scripts\reuncap.log` in the game's folder (on Steam: in the `rebirth` folder with Mulderload's pack,
   in the `japanese` folder with Classic REbirth's own setup).
2. Which game, which version (Steam or not), which Classic REbirth version and which other mods you
   use (e.g. Seamless HD Project, BioRand).
3. What you saw, where it happens, and whether it goes away when you press **Shift + `=`** (mod off).
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
  text fixes. Their `bio1hd.asi` is included in the Resident Evil Classic REbirth 1.1.3 folder.
- **Mulderload's Steam Enhancement Pack** (mulderland.com) - the Steam package is laid out for it.
- **Ultimate ASI Loader** by ThirteenAG (github.com/ThirteenAG/Ultimate-ASI-Loader, MIT licence) -
  bundled as `dsound.dll`; it loads REuncap in all three games. Its licence is included as
  `scripts\UltimateASILoader-LICENSE.txt`.
- Built with AI assistance (Claude) together with the mod author. Resident Evil 2 and 3 were reverse
  engineered from the game executables and Classic REbirth's runtime behaviour.
- Resident Evil is a trademark of Capcom Co., Ltd. This is an unofficial fan project; no game files
  are included.
