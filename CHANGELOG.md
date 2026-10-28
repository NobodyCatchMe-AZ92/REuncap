# REuncap changelog

## v1.4 (2026-10-28)
**New**
- **Resident Evil 2 works together with RE:Enhance 4.0.** This compatibility is based specifically on the
  **currently available pre-release RE2 version of RE:Enhance 4.0**; compatibility with further updates
  of RE:Enhance 4.0 will be ensured as they come. See "RE:Enhance 4.0" in `README.md`.
  - **Recommended for RE:Enhance 4.0 users: play in windowed mode** for the most stable frame rate. In
    RE:Enhance's fullscreen mode the frame rate is less stable (lowering RE:Enhance's antialiasing helps).
  - REuncap replaces RE:Enhance's own 60 FPS with any frame rate. While RE:Enhance's 60 FPS is on,
    REuncap pauses; switching it on or off in RE:Enhance's in-game menu pauses or resumes REuncap right
    away, with a short message.
  - RE:Enhance's 16:9 mode (pan and scan) pans smoothly at your frame rate (new setting
    `RE2EnhancePanSmooth`, on by default).
  - The Resident Evil 2 folder includes a `DDrawCompat.ini` (`PresentDelay = off`). RE:Enhance shows the
    game through DDrawCompat, whose default let only an uneven ~60 frames per second reach the screen.
  - Resident Evil and Resident Evil 3: REuncap stays inactive (with a short message) if RE:Enhance's
    60 FPS is on, ready for the upcoming RE:Enhance 4.0 versions of these games.
- **Door transitions** (the door opening animation between rooms) are now smooth in all three games.
- **Resident Evil 2:** room objects such as pushable statues and blocks and item boxes are now smooth,
  and so is the mirror reflection in the interrogation room.
- **Resident Evil:** an examined item in the inventory now turns smoothly as well.
- The frame rate hotkey (`=`) now also includes your own `FpsCap` if it is another number (e.g. 144).
- If your `reuncap.ini` is older than the settings of the REuncap version you run, a short message in the
  game says so (it keeps working).
- `reuncap.log` keeps the logs of the four sessions before as `reuncap.1.log` (previous) to
  `reuncap.4.log`.

**Fixes**
- **Resident Evil 2 and 3:** `FpsCap=-1` (your monitor's refresh rate) could stay at 60 fps unless
  `DebugLog` was on.

**Upgrading:** replace `scripts\reuncap.asi`. Your old `reuncap.ini` keeps working; copy the new one over
it for the new setting and the updated descriptions. Resident Evil 2 with RE:Enhance 4.0: also copy
`DDrawCompat.ini` next to the game's `.exe`.

## v1.3 (2026-10-11)
**Fixes**
- **Resident Evil 3:** text in interaction popups no longer slides around while it is written out
  (text and interface graphics are no longer interpolated).
- **Resident Evil 3:** fixed a crash during the boss encounter in the tram.
- **Resident Evil (Classic REbirth 1.1.4):** skipping a cutscene with Classic REbirth's cutscene skip no
  longer leaves the screen black.
- **Resident Evil:** REuncap now stays inactive when the game is started without Classic REbirth (e.g.
  a launcher's "original executable" option with REuncap's files still in the folder).

**New**
- **Hotkeys:** Shift + `=` switches REuncap on and off (was F12); `=` on its own changes the frame rate
  cap while playing: 30, 60, 90, 120, your monitor's refresh rate, uncapped. Numpad `+` works the same
  as `=`. The key only works while the game window is active. `ToggleKey=123` in `reuncap.ini` brings
  back F12.
- **On-screen messages** for the hotkeys, e.g. "Disabled REuncap with hotkey." or "FPS set to 60 with
  hotkey.", shown for 4 seconds in the lower right corner. `ToggleMessage=0` turns them off.
- `FpsCap=30` is now allowed (no in-between frames).
- **Basic support for older Classic REbirth versions:** Resident Evil 2 on 1.0.9 and 1.0.8 (no more
  flickering typewriter/save screens), Resident Evil 3 on 1.0.2 and 1.0.1. This support is very basic
  and no further support will be given for these versions: any bugs or issues on them are the user's
  responsibility. Please upgrade to the latest Classic REbirth for Resident Evil 2 (1.0.9.1) and
  Resident Evil 3 (1.0.3).

**Packaging**
- **Resident Evil** now has two folders in the non-Steam download: one for Classic REbirth 1.1.4 and one
  for 1.1.3. The 1.1.3 folder includes Seamless HD Project's `bio1hd.asi` in `scripts`, so it installs by
  drag and drop like the others.
- **Steam:** the package now works with **Mulderload's Steam Enhancement Pack** (the game runs from
  `rebirth`) as well as with Classic REbirth's own Steam setup (`japanese`), from the same three folders.
  For Mulderload's pack, turning dgVoodoo2 off is recommended (with it on, REuncap reaches only about 60 fps
  in Resident Evil and about 90 in Resident Evil 2); a `MulderConfig.save.json` with dgVoodoo2 off is
  included - run `MulderConfig.exe` once and save to apply it.

**Upgrading:** replace `scripts\reuncap.asi`. Your old `reuncap.ini` keeps working, but it still sets
`ToggleKey=123` (F12) explicitly - copy the new `reuncap.ini` over it (or change that line to
`ToggleKey=187`) to get the new `=` hotkeys and the descriptions of the new settings.

## v1.2 (2026-10-07)
**Fixes**
- **All three games:** after a door, a skipped stair/door animation or another short hitch, a camera
  angle could look like 30 fps (the frame counter still said 60/120) until the next camera change or
  toggling the mod with F12. This happened at frame caps that are a multiple of 30 (60, 90, 120...).
  The in-between frames now always stay evenly spaced.
- **Resident Evil 3:** zombie limbs flickering, stretching or disappearing while zombies moved or
  bunched up together. Polygons of identical-looking zombies could be mixed up with each other between
  frames; each enemy is now only ever blended with itself.
- **Resident Evil 2:** the first moments of a new camera angle no longer look like 30 fps.

**New**
- **`RE2CutCatchUp`** (Resident Evil 2 only, on by default): a new camera angle now starts smoothly.
  The old angle stays up a moment longer (about as long as in the original game), then the new one
  plays from its very first image and catches up with the game within ~130 ms. Set it to `0` to get
  the previous behaviour.
- The first line of `reuncap.ini` now shows which REuncap version it came with.
- `CHANGELOG.md` (this file) is included in the download.

**Upgrading:** replace `scripts\reuncap.asi` with the new one. Your old `reuncap.ini` keeps working
(new settings use their defaults); copy the new `reuncap.ini` over it if you want the version line and
the description of the new setting.

## v1.1 (2026-10-05)
- The **Steam versions** of Resident Evil, Resident Evil 2 and Resident Evil 3 are supported (with
  Classic REbirth set up for Steam).
- Two downloads: `REuncap-v1.1-NONSTEAM.zip` for the original PC releases and `REuncap-v1.1-STEAM.zip`,
  laid out for the Steam versions' `japanese` folders (drag and drop into the game's Steam folder).
- The zips contain the game folders directly, with no extra folder to click through.
- README: Steam instructions and a G-Sync / FreeSync tip.

## v1.0.1 (2026-10-05)
- The mod installs the same way in all three games: it is loaded by the bundled Ultimate ASI Loader
  (`dsound.dll` + `global.ini`) from a `scripts` folder, so there is nothing to tick in Classic
  REbirth's Mod Selection window, and it works together with any Classic REbirth mod.
- `global.ini` stops leftover Seamless HD Project `.asi` files from being loaded in Resident Evil and
  Resident Evil 3, where they caused blurry backgrounds / a "dumping" message.
- A short reminder text file in each game folder shows where everything is.

## v1.0 (2026-10-05)
- First release: smooth high or uncapped frame rates for Resident Evil, Resident Evil 2 and Resident
  Evil 3 with Classic REbirth. Game logic stays at the original 30 ticks per second; extra in-between
  frames are drawn for characters, enemies and other 3D models, shadows, blood pools and effects.
- `FpsCap` (monitor refresh rate, uncapped or any number), F12 on/off toggle, `RE2Pacing` and
  `RE2SmoothCameraCuts` (shorter freeze at Resident Evil 2's camera changes).
