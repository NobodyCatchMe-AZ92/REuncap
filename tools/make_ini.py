# Writes reuncap.ini in the house layout: each setting first, its explanation below,
# two blank lines between entries, CRLF line endings.
# usage: py -3 tools/make_ini.py <out.ini> [Key=Value ...]   (overrides the defaults below)
import sys

ENTRIES = [
    ("Enabled", "1", [
        "1 = high-framerate presentation on, 0 = off (vanilla 30 fps)",
        "You can also switch the mod on and off while playing with a hotkey: F12 by default,",
        "changed with ToggleKey below.",
    ]),
    ("FpsCap", "-1", [
        "Frame rate cap while playing. Game logic always stays at the original 30 ticks/s.",
        "  -1  = your monitor refresh rate (default)",
        "   0  = uncapped (as fast as the display path allows)",
        "  60, 120, 144, 165, 190, 240 ... = that many fps (any number from 31 up)",
        "With G-Sync/FreeSync, a cap a few fps below the refresh rate gives the most even pacing.",
    ]),
    ("ToggleKey", "123", [
        "virtual-key code of the in-game on/off toggle (123 = F12)",
    ]),
    ("DebugLog", "0", [
        "1 = write fps / pacing / tick-rate stats to reuncap.log every 2 s",
        "Please set this to 1 and attach reuncap.log when reporting a problem.",
    ]),
    ("MaxMovePerTick", "2500", [
        "view-space distance per 30 Hz tick above which a model is treated as teleported (not blended)",
    ]),
    ("RE2Pacing", "0", [
        "Resident Evil 2 only. How frames are spaced out.",
        "  0 = run at your FpsCap exactly (default)",
        "  1 = run at the nearest multiple of 30 at or below your FpsCap (e.g. 190 becomes 180),",
        "      with every frame exactly the same length apart",
        "Set to 1 if you notice slight stutter or uneven motion in Resident Evil 2, or if you",
        "don't use G-Sync/FreeSync. If your FpsCap is already a multiple of 30 (60, 90, 120,",
        "150, 180...), both settings behave the same.",
    ]),
    ("RE2SmoothCameraCuts", "1", [
        "Resident Evil 2 only. RE2 briefly freezes the picture at every camera angle change.",
        "  1 = shorter freeze: the mod trims the vanilla hitch to the part the game really needs (default)",
        "  0 = original behaviour: the full vanilla hitch, which is longer than with 1",
        "The freeze cannot be removed completely: on a camera change RE2 loads the new background and",
        "the first moment of the new angle has no earlier frame to blend from, so a short hitch always",
        "remains.",
    ]),
]


def render(overrides):
    lines = ["[REuncap]"]
    for i, (key, val, comments) in enumerate(ENTRIES):
        if i:
            lines += ["", ""]
        lines.append(f"{key}={overrides.get(key, val)}")
        lines += ["; " + c for c in comments]
    return "\r\n".join(lines) + "\r\n"


if __name__ == "__main__":
    out = sys.argv[1]
    ov = dict(a.split("=", 1) for a in sys.argv[2:])
    with open(out, "w", newline="") as f:
        f.write(render(ov))
