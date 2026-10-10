#!/bin/bash
# Builds two release packages from build/reuncap.asi, tools/make_ini.py, third_party/UltimateASILoader and
# third_party/SeamlessHDProject (bio1hd.asi, for the Classic REbirth 1.1.3 folder):
#
#   release/REuncap-NONSTEAM/  (zip: REuncap-v<VER>-NONSTEAM.zip) - original PC releases
#     Resident Evil (Classic Rebirth 1.1.4)/...  copy the folder's contents into the folder with the game's .exe
#     Resident Evil (Classic Rebirth 1.1.3)/...  same, plus scripts/bio1hd.asi (1.1.3 still needs it; global.ini
#                                                only lets the loader load from the scripts folder)
#     Resident Evil 2/..., Resident Evil 3/...
#   release/REuncap-STEAM/     (zip: REuncap-v<VER>-STEAM.zip)    - Steam re-releases, one combined layout:
#     <game>/japanese/...        Classic REbirth's own Steam setup (it runs the game from "japanese")
#     <game>/rebirth/...         Mulderload's Steam Enhancement Pack (it runs the game from "rebirth"): RE1/RE2
#                                only scripts/ (the pack's own ASI loader loads it); RE3 also our loader
#                                (the pack's dinput8.dll is never loaded in RE3)
#     <game>/MulderConfig.save.json  (RE1/RE2) the pack's settings with dgVoodoo2 off (applied by running
#                                MulderConfig.exe once and saving)
#
# Game folder contents (the same in both packages):
#   dsound.dll                               Ultimate ASI Loader (loads the mod)
#   global.ini                               loader settings (see the comments inside)
#   REuncap-UncappedFPSMod-README.txt        short reminder of where everything is
#   scripts/reuncap.asi, reuncap.ini         the mod and its settings (reuncap.log is written here too)
#   scripts/UltimateASILoader-LICENSE.txt
set -e
cd "$(dirname "$0")/.."
VER="${1:-1.3}"
RN=release/REuncap-NONSTEAM
RS=release/REuncap-STEAM
rm -rf release/REuncap "$RN" "$RS"
mkdir -p "$RN" "$RS"
crlf() { sed 's/$/\r/'; }

global_ini() {   # $1 = file, $2 = 1 (scripts folder only), 0 (game folder + scripts folder), 113 (RE1 CR 1.1.3)
    if [ "$2" = 113 ]; then
        crlf > "$1" <<'EOF'
; Settings for Ultimate ASI Loader (dsound.dll), which loads REuncap from the "scripts" folder.
;
; Resident Evil with Classic REbirth 1.1.3: LoadFromScriptsOnly=1 makes the loader load only from the
; "scripts" folder. Classic REbirth 1.1.3 still needs Seamless HD Project's bio1hd.asi, so a copy of it
; is in the "scripts" folder and is loaded from there; a bio1hd.asi next to the game's .exe is not loaded
; (it can stay there or be deleted). Keep this at 1.
[GlobalSets]
LoadFromScriptsOnly=1
EOF
    elif [ "$2" = 1 ]; then
        crlf > "$1" <<'EOF'
; Settings for Ultimate ASI Loader (dsound.dll), which loads REuncap from the "scripts" folder.
;
; LoadFromScriptsOnly=1 makes the loader ignore .asi files next to the game's .exe. Older Seamless HD
; Project installs leave bio1hd.asi / bio3hd.asi there; current Classic REbirth versions have HD support
; built in and no longer use them, and if they are loaded they cause problems (blurry HD backgrounds in
; Resident Evil, a "dumping" message at start-up in Resident Evil 3). Keep this at 1.
[GlobalSets]
LoadFromScriptsOnly=1
EOF
    else
        crlf > "$1" <<'EOF'
; Settings for Ultimate ASI Loader (dsound.dll), which loads REuncap from the "scripts" folder.
;
; LoadFromScriptsOnly=0 lets the loader also load .asi files next to the game's .exe. Resident Evil 2
; needs that: Seamless HD Project's bio2hd.asi lives there and is still in use. Keep this at 0.
[GlobalSets]
LoadFromScriptsOnly=0
EOF
    fi
}

reminder() {   # $1 = file, $2 = game name, $3 = where-note, $4 = extra uninstall note
    crlf > "$1" <<EOF
REuncap v$VER - smooth high / uncapped frame rates for $2 (Classic REbirth)
Game logic stays at the original 30 ticks per second; only extra in-between frames are added.
$3
Everything the mod uses is in the "scripts" folder next to this file:
  scripts\\reuncap.asi    the mod
  scripts\\reuncap.ini    settings (frame rate cap, on/off key, ...) - each setting is explained inside
  scripts\\reuncap.log    written while playing (set DebugLog=1 in reuncap.ini for detailed stats)

It is loaded by dsound.dll (Ultimate ASI Loader) next to this file, which reads global.ini.

In game: Shift + = switches the mod on and off; = alone changes the frame rate cap (30/60/90/120/monitor/uncapped).

To uninstall: delete scripts\\reuncap.asi, scripts\\reuncap.ini, scripts\\reuncap.log and this file.
$4

Problems? Please report them with your reuncap.log (DebugLog=1) - see README.md in the download.
EOF
}

game_folder() {   # $1 = destination folder, $2 = game name, $3 = scripts-only (1/0), $4 = where-note, $5 = uninstall note
    mkdir -p "$1/scripts"
    cp third_party/UltimateASILoader/dsound.dll "$1/"
    cp third_party/UltimateASILoader/LICENSE.txt "$1/scripts/UltimateASILoader-LICENSE.txt"
    cp build/reuncap.asi "$1/scripts/"
    py -3 tools/make_ini.py "$1/scripts/reuncap.ini" --version "$VER"
    global_ini "$1/global.ini" "$3"
    reminder "$1/REuncap-UncappedFPSMod-README.txt" "$2" "$4" "$5"
}

DEL="Also delete dsound.dll and global.ini, unless another mod you use needs them."
DEL113="Also delete dsound.dll and global.ini, unless another mod you use needs them. scripts\\bio1hd.asi
belongs to Seamless HD Project: keep it if you keep playing on Classic REbirth 1.1.3 with HD backgrounds."
KEEP2="Keep dsound.dll if you use Seamless HD Project (it needs it too); global.ini can be deleted."
JPNOTE="(Steam version, Classic REbirth's own Steam setup: these files belong in the game's \"japanese\"
folder, next to the game's .exe.)
"
MLNOTE="(Steam version with Mulderload's Steam Enhancement Pack: these files belong in the game's
\"rebirth\" folder, next to the game's .exe.)
"
MLKEEP="Keep Mulderload's own files (its dinput8.dll / dsound.dll and the rest) - they belong to the pack."
NOTE113="(Classic REbirth 1.1.3: scripts\\bio1hd.asi is Seamless HD Project's HD loader, which 1.1.3 still needs.)
"

# Mulderload layout, RE1/RE2: only the scripts folder (the pack's own ASI loader loads it) + a reminder
ml_scripts() {   # $1 = rebirth folder, $2 = game name
    mkdir -p "$1/scripts"
    cp build/reuncap.asi "$1/scripts/"
    py -3 tools/make_ini.py "$1/scripts/reuncap.ini" --version "$VER"
    reminder "$1/REuncap-UncappedFPSMod-README.txt" "$2" "$MLNOTE(The pack already has an ASI loader that loads everything in the \"scripts\" folder.)
" "$MLKEEP"
    py -3 - "$1/REuncap-UncappedFPSMod-README.txt" <<'PYEOF'
import sys
p = sys.argv[1]; s = open(p, encoding='utf-8', newline='').read()
s = s.replace("It is loaded by dsound.dll (Ultimate ASI Loader) next to this file, which reads global.ini.",
              "It is loaded by the ASI loader that Mulderload's pack already installed here (dinput8.dll / dsound.dll).")
open(p, 'w', encoding='utf-8', newline='').write(s)
PYEOF
}
mulderconfig() {   # $1 = file, $2 = game title as MulderConfig names it
    crlf > "$1" <<MCEOF
{
  "$2": {
    "Executable": "Classic REbirth",
    "Misc": [
      "Enable HD textures"
    ],
    "MSAA Anti-Aliasing": "Off"
  }
}
MCEOF
}

# ---- non-Steam package ----------------------------------------------------------------------------
cp README.md "$RN/README.md"; cp CHANGELOG.md "$RN/CHANGELOG.md"
game_folder "$RN/Resident Evil (Classic Rebirth 1.1.4)" "Resident Evil" 1 "" "$DEL"
game_folder "$RN/Resident Evil (Classic Rebirth 1.1.3)" "Resident Evil" 113 "$NOTE113" "$DEL113"
cp third_party/SeamlessHDProject/bio1hd.asi "$RN/Resident Evil (Classic Rebirth 1.1.3)/scripts/"
game_folder "$RN/Resident Evil 2" "Resident Evil 2" 0 "" "$KEEP2"
game_folder "$RN/Resident Evil 3" "Resident Evil 3" 1 "" "$DEL"
crlf > "$RN/STEAM-USERS-README.txt" <<EOF
REuncap v$VER - this is the package for the ORIGINAL (non-Steam) PC versions.

Playing the STEAM version of Resident Evil, Resident Evil 2 or Resident Evil 3?
Then you probably downloaded the wrong package: please download REuncap-v$VER-STEAM.zip instead.
It is laid out for the Steam versions, so you can simply drag and drop it.

Resident Evil: use the folder that matches your Classic REbirth version - "Resident Evil (Classic
Rebirth 1.1.4)" or "Resident Evil (Classic Rebirth 1.1.3)". Not sure which you have? Right-click
ddraw.dll in the game folder > Properties > Details shows the version.
EOF

# ---- Steam package (one combined layout) ----------------------------------------------------------
cp README.md "$RS/README.md"; cp CHANGELOG.md "$RS/CHANGELOG.md"
game_folder "$RS/Resident Evil/japanese"   "Resident Evil"   1 "$JPNOTE" "$DEL"
game_folder "$RS/Resident Evil 2/japanese" "Resident Evil 2" 0 "$JPNOTE" "$DEL"
game_folder "$RS/Resident Evil 3/japanese" "Resident Evil 3" 1 "$JPNOTE" "$DEL"
ml_scripts  "$RS/Resident Evil/rebirth"    "Resident Evil"
ml_scripts  "$RS/Resident Evil 2/rebirth"  "Resident Evil 2"
game_folder "$RS/Resident Evil 3/rebirth"  "Resident Evil 3" 1 "$MLNOTE" "$DEL"
mulderconfig "$RS/Resident Evil/MulderConfig.save.json"   "Resident Evil (Steam)"
mulderconfig "$RS/Resident Evil 2/MulderConfig.save.json" "Resident Evil 2 (Steam)"
crlf > "$RS/STEAM-USERS-README.txt" <<EOF
REuncap v$VER - package for the STEAM versions of Resident Evil, Resident Evil 2 and Resident Evil 3

The Steam versions need Classic REbirth. This package works with both common ways to set it up:
  - Mulderload's Steam Enhancement Pack (mulderland.com) - the game runs from the "rebirth" folder
  - Classic REbirth's own Steam setup (for Resident Evil 2 e.g. this guide:
    https://steamcommunity.com/sharedfiles/filedetails/?id=3701562809) - the game runs from "japanese"
The same files cover both; the part your setup doesn't use is simply ignored.

Drag and drop: copy the contents of the folder named after your game ("Resident Evil",
"Resident Evil 2" or "Resident Evil 3") into the game's main Steam folder - the one with the
Steam launcher in it - and allow it to replace files if asked:

  Resident Evil    ->  steamapps\\common\\4249100_Biohazard
  Resident Evil 2  ->  steamapps\\common\\4249110_Biohazard2
  Resident Evil 3  ->  steamapps\\common\\4249120_Biohazard3

("steamapps" is inside your Steam library folder, e.g. C:\\Program Files (x86)\\Steam\\steamapps)

Mulderload's Steam Enhancement Pack users - recommended: turn dgVoodoo2 off. With it on, REuncap
reaches only about 60 fps in Resident Evil and about 90 fps in Resident Evil 2. The package includes
MulderConfig.save.json with dgVoodoo2 already off (MSAA off too, as it needs dgVoodoo2; HD textures on).
To apply it, run MulderConfig.exe in the game's Steam folder once and save.
Note: according to Mulderload's readme, the Steam Overlay needs dgVoodoo2 in Classic REbirth (Steam
play time tracking works without it).

Everything else (settings, the hotkeys, uninstalling, G-Sync/FreeSync tips) is described in README.md.
EOF

# ---- zips: the package folder's CONTENTS at the zip root (no extra top-level folder to click into) ----
py -3 - "$VER" <<'PYEOF'
import os, sys, zipfile
ver = sys.argv[1]
for folder, name in (('REuncap-NONSTEAM', 'NONSTEAM'), ('REuncap-STEAM', 'STEAM')):
    src = os.path.join('release', folder)
    out = os.path.join('release', f'REuncap-v{ver}-{name}.zip')
    with zipfile.ZipFile(out, 'w', zipfile.ZIP_DEFLATED) as z:
        for root, dirs, files in os.walk(src):
            dirs.sort()
            rel = os.path.relpath(root, src).replace(os.sep, '/')
            if rel != '.':
                z.write(root, rel + '/')                      # keep folder entries (e.g. empty-looking japanese/)
            for f in sorted(files):
                z.write(os.path.join(root, f), (f if rel == '.' else rel + '/' + f))
    print('zip:', out)
PYEOF

echo "packaged v$VER:"; find "$RN" "$RS" -type f | sort
