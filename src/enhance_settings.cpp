// !REuncap-RE-Enhance.asi - Resident Evil 2 with RE:Enhance 4.0 only.
//
// Two RE:Enhance options stop REuncap from working:
//   Gameplay60FPS=1  RE:Enhance's own 60 FPS interpolation (REuncap pauses while it is on)
//   FramePacing=1    BETTER FRAME PACING: on 60/120/180/240 Hz monitors it holds every frame back to a 60 Hz slot,
//                    which limits REuncap to about 30-60 fps
// RE:Enhance (bio2hd.asi, next to the game's .exe) reads RE_ENHANCE.ini once while it loads, and the ASI loader
// loads the game folder's ASIs before the "scripts" folder - so REuncap itself (scripts\reuncap.asi) is always too
// late to change them for the current session. This file sits next to bio2hd.asi with a name that sorts before it
// ("!"), so the loader runs it first: it switches both options off in RE_ENHANCE.ini before RE:Enhance reads them.
//
// It only does that while REuncap is installed and enabled (scripts\reuncap.asi exists, Enabled=1) and
// RE2EnhanceAutoSettings=1 (default) in scripts\reuncap.ini. Without RE_ENHANCE.ini it does nothing.
// What it did goes to REuncap (same process) in the environment variable REUNCAP_ENH_HELPER:
//   "off"                                   disabled by reuncap.ini
//   "fps60=<0|1> pacing=<0|1> late=<0|1>"   1 = that option was on and is now off; late = RE:Enhance had already
//                                           loaded (its settings then only change from the next start)
// REuncap logs it and tells the player.
#include <windows.h>
#include <stdio.h>
#include <string.h>

// Finds "key=" (any section, case-insensitive) and returns its value (-1 = not found); *section gets its section.
static int ReadKey(const char* ini, const char* key, char* section, size_t secSize) {
    FILE* f = 0; char line[512], cur[128] = ""; int v = -1;
    if (fopen_s(&f, ini, "r") != 0 || !f) return -1;
    size_t n = strlen(key);
    while (fgets(line, sizeof line, f)) {
        char* p = line; while (*p == ' ' || *p == '\t') p++;
        if (*p == '[') {
            char* e = strchr(p, ']');
            if (e) { size_t len = (size_t)(e - p - 1); if (len >= sizeof cur) len = sizeof cur - 1; memcpy(cur, p + 1, len); cur[len] = 0; }
            continue;
        }
        if (_strnicmp(p, key, n)) continue;
        char* q = p + n; while (*q == ' ' || *q == '\t') q++;
        if (*q != '=') continue;
        v = atoi(q + 1) ? 1 : 0;
        strcpy_s(section, secSize, cur);
        break;
    }
    fclose(f);
    return v;
}

// 1 if key was 1 and is now 0
static int SwitchOff(const char* ini, const char* key) {
    char section[128] = "";
    if (ReadKey(ini, key, section, sizeof section) != 1 || !section[0]) return 0;
    return WritePrivateProfileStringA(section, key, "0", ini) ? 1 : 0;
}

static void Run() {
    char dir[MAX_PATH], ini[MAX_PATH], asi[MAX_PATH], cfg[MAX_PATH];
    if (!GetModuleFileNameA(0, dir, MAX_PATH)) return;
    char* sl = strrchr(dir, '\\'); if (!sl) return;
    sl[1] = 0;
    sprintf_s(ini, "%sRE_ENHANCE.ini", dir);
    sprintf_s(asi, "%sscripts\\reuncap.asi", dir);
    sprintf_s(cfg, "%sscripts\\reuncap.ini", dir);
    if (GetFileAttributesA(ini) == INVALID_FILE_ATTRIBUTES) return;   // no RE:Enhance
    if (GetFileAttributesA(asi) == INVALID_FILE_ATTRIBUTES) return;   // REuncap not installed (any more)
    if (!GetPrivateProfileIntA("REuncap", "Enabled", 1, cfg) || !GetPrivateProfileIntA("REuncap", "RE2EnhanceAutoSettings", 1, cfg)) {
        SetEnvironmentVariableA("REUNCAP_ENH_HELPER", "off");
        return;
    }
    int late = GetModuleHandleA("bio2hd.asi") != 0;
    int fps60 = SwitchOff(ini, "Gameplay60FPS");
    int pacing = SwitchOff(ini, "FramePacing");
    char msg[64]; sprintf_s(msg, "fps60=%d pacing=%d late=%d", fps60, pacing, late);
    SetEnvironmentVariableA("REUNCAP_ENH_HELPER", msg);
}

BOOL WINAPI DllMain(HMODULE h, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) { DisableThreadLibraryCalls(h); Run(); }
    return TRUE;
}
