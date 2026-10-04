#!/bin/bash
# Assembles release/REuncap from build/reuncap.asi and tools/make_ini.py.
# Each game folder mirrors that game's install folder, so installing is always
# "copy the contents of your game's folder into the game's install folder".
#   release/REuncap/README.md
#   release/REuncap/Resident Evil/mod_REuncap/{manifest.txt, description.txt, reuncap.asi, reuncap.ini}
#   release/REuncap/Resident Evil 2/{reuncap.asi, reuncap.ini}
#   release/REuncap/Resident Evil 3/mod_REuncap/{manifest.txt, description.txt, reuncap.asi, reuncap.ini}
# Manifests deliberately have no UseRdtXml key: it would override other mods' text (e.g. Seamless HD Project).
set -e
cd "$(dirname "$0")/.."
VER="${1:-1.0}"
R=release/REuncap
RE1="$R/Resident Evil/mod_REuncap"
RE2="$R/Resident Evil 2"
RE3="$R/Resident Evil 3/mod_REuncap"
rm -rf "$R/mod_REuncap" "$R/Resident Evil" "$R/Resident Evil 2" "$R/Resident Evil 3"
mkdir -p "$RE1" "$RE2" "$RE3"
cp README.md "$R/README.md"

for d in "$RE1" "$RE2" "$RE3"; do
    cp build/reuncap.asi "$d/"
    py -3 tools/make_ini.py "$d/reuncap.ini"
done

manifest() {   # $1 = folder, $2 = minimum Classic REbirth version
    printf '[MOD]\r\nName = REuncap\r\nTitle =\r\nModule = reuncap.asi\r\nVersion = %s\r\nSavePath =\r\n' "$2" > "$1/manifest.txt"
    printf 'REuncap v%s - smooth high / uncapped frame rates (60, 120, 144, 190, refresh rate...)\r\nwith game logic kept at the original 30 ticks per second.\r\nSettings: reuncap.ini in this folder. F12 toggles in game.\r\n' "$VER" > "$1/description.txt"
}
manifest "$RE1" 1.1.4
manifest "$RE3" 1.0.3

echo "packaged v$VER:"; find "$R" -type f | sort
