#!/bin/bash
# Builds two release packages from build/reuncap.asi, tools/make_ini.py and third_party/UltimateASILoader:
#
#   release/REuncap-NONSTEAM/  (zip: REuncap-v<VER>-NONSTEAM.zip) - original PC releases
#     <game>/...                 copy the contents of <game> into the folder with the game's .exe
#   release/REuncap-STEAM/     (zip: REuncap-v<VER>-STEAM.zip)    - Steam re-releases
#     <game>/japanese/...        copy the contents of <game> into the game's main Steam folder; the files
#                                land in its "japanese" folder, where Classic REbirth runs the game
#
# Game folder contents (the same in both packages):
#   dsound.dll                               Ultimate ASI Loader (loads the mod)
#   global.ini                               loader settings (see the comments inside)
#   REuncap-UncappedFPSMod-README.txt        short reminder of where everything is
#   scripts/reuncap.asi, reuncap.ini         the mod and its settings (reuncap.log is written here too)
#   scripts/UltimateASILoader-LICENSE.txt
set -e
cd "$(dirname "$0")/.."
VER="${1:-1.2}"
RN=release/REuncap-NONSTEAM
RS=release/REuncap-STEAM
rm -rf release/REuncap "$RN" "$RS"
mkdir -p "$RN" "$RS"
crlf() { sed 's/$/\r/'; }

global_ini() {   # $1 = file, $2 = 1 (scripts folder only) or 0 (game folder + scripts folder)
    if [ "$2" = 1 ]; then
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

In game: F12 switches the mod on and off.

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
KEEP2="Keep dsound.dll if you use Seamless HD Project (it needs it too); global.ini can be deleted."
STEAMNOTE="(Steam version: these files belong in the game's \"japanese\" folder, next to the game's .exe.)
"

# ---- non-Steam package ----------------------------------------------------------------------------
cp README.md "$RN/README.md"; cp CHANGELOG.md "$RN/CHANGELOG.md"
game_folder "$RN/Resident Evil"   "Resident Evil"   1 "" "$DEL"
game_folder "$RN/Resident Evil 2" "Resident Evil 2" 0 "" "$KEEP2"
game_folder "$RN/Resident Evil 3" "Resident Evil 3" 1 "" "$DEL"
crlf > "$RN/STEAM-USERS-README.txt" <<EOF
REuncap v$VER - this is the package for the ORIGINAL (non-Steam) PC versions.

Playing the STEAM version of Resident Evil, Resident Evil 2 or Resident Evil 3?
Then you probably downloaded the wrong package: please download REuncap-v$VER-STEAM.zip instead.
It is laid out for the Steam versions' "japanese" folders, so you can simply drag and drop it.
EOF

# ---- Steam package --------------------------------------------------------------------------------
cp README.md "$RS/README.md"; cp CHANGELOG.md "$RS/CHANGELOG.md"
game_folder "$RS/Resident Evil/japanese"   "Resident Evil"   1 "$STEAMNOTE" "$DEL"
game_folder "$RS/Resident Evil 2/japanese" "Resident Evil 2" 0 "$STEAMNOTE" "$DEL"
game_folder "$RS/Resident Evil 3/japanese" "Resident Evil 3" 1 "$STEAMNOTE" "$DEL"
crlf > "$RS/STEAM-USERS-README.txt" <<EOF
REuncap v$VER - package for the STEAM versions of Resident Evil, Resident Evil 2 and Resident Evil 3

1) Set up Classic REbirth for your Steam game first:
     Resident Evil / Resident Evil 3:  follow Classic REbirth's own Steam instructions.
     Resident Evil 2:                  follow this Steam guide:
                                       https://steamcommunity.com/sharedfiles/filedetails/?id=3701562809

2) Drag and drop: copy the contents of the folder named after your game ("Resident Evil",
   "Resident Evil 2" or "Resident Evil 3") into the game's main Steam folder - the one with the
   Steam launcher in it:

     Resident Evil    ->  steamapps\\common\\4249100_Biohazard
     Resident Evil 2  ->  steamapps\\common\\4249110_Biohazard2
     Resident Evil 3  ->  steamapps\\common\\4249120_Biohazard3

   ("steamapps" is inside your Steam library folder, e.g. C:\\Program Files (x86)\\Steam\\steamapps)

   That is all. The files go into the game's "japanese" folder automatically, because that is where
   Classic REbirth runs the game: everything REuncap uses (dsound.dll, global.ini and the "scripts"
   folder with reuncap.asi, reuncap.ini and reuncap.log) is located in that "japanese" folder.

Everything else (settings, F12 toggle, uninstalling, G-Sync/FreeSync tips) is described in README.md.
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
