# REuncap changelog

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
