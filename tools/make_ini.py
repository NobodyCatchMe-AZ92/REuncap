# Writes reuncap.ini in the house layout: each setting first, its explanation below,
# two blank lines between entries, CRLF line endings.
# usage: py -3 tools/make_ini.py <out.ini> [--version X] [Key=Value ...]   (overrides the defaults below)
# --version puts a "; REuncap vX" line at the very top, so users can see which release their ini came with.
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
        "The freeze cannot be removed completely: on a camera change RE2 loads the new background, so a",
        "short freeze always remains.",
    ]),
    ("RE2CutCatchUp", "1", [
        "Resident Evil 2 only. Smooth start of a new camera angle.",
        "  1 = the new angle starts smoothly: the old angle stays up a moment longer, then the new one",
        "      plays from its first image and catches up with the game within ~130 ms (default)",
        "  0 = the new angle's first image is shown twice (one short 30 fps step), which is the vanilla",
        "      game's behaviour but doesn't feel as natural as REuncap's CutCatchUp solution",
    ]),
]


def render(overrides, version=None):
    lines = []
    if version:
        lines += [f"; REuncap v{version}",
                  "; (the REuncap version these settings came with - newest release:",
                  ";  https://github.com/NobodyCatchMe-AZ92/REuncap/releases)",
                  ""]
    lines.append("[REuncap]")
    for i, (key, val, comments) in enumerate(ENTRIES):
        if i:
            lines += ["", ""]
        lines.append(f"{key}={overrides.get(key, val)}")
        lines += ["; " + c for c in comments]
    return "\r\n".join(lines) + "\r\n"


if __name__ == "__main__":
    out = sys.argv[1]
    args = sys.argv[2:]
    version = None
    if len(args) >= 2 and args[0] == "--version":
        version, args = args[1], args[2:]
    ov = dict(a.split("=", 1) for a in args)
    with open(out, "w", newline="") as f:
        f.write(render(ov, version))
