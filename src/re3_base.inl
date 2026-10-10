// RE3 (BIOHAZARD(R) 3 PC.exe / OST_LOSSLESS.exe, Sourcenext 1.1.0 + Classic REbirth 1.0.3) - detection and
// the few helpers the interpolation in re3.inl needs. Included from reuncap.cpp.

#define R3_SIG_ADDR 0x0040F2DC   // WinMain: call [PeekMessageA] - specific to RE3 1.1.0
static int IsRe3() {
    static const BYTE kSig[6] = { 0xFF, 0x15, 0x74, 0xD2, 0x50, 0x00 };
    __try { return memcmp((void*)R3_SIG_ADDR, kSig, 6) == 0; } __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
}

static DWORD g_r3CrBase, g_r3CrEnd;     // Classic REbirth's module (it is mapped manually, so found via its code)
static void Re3InitInterp();

static int R3InCode(DWORD a) {
    return (a >= 0x401000 && a < 0x50D000) || (a >= g_r3CrBase && a < g_r3CrEnd);
}
static void* PatchVtbl(void** vt, int idx, void* fn) {
    DWORD old; void* orig = vt[idx];
    VirtualProtect(&vt[idx], 4, PAGE_EXECUTE_READWRITE, &old); vt[idx] = fn; VirtualProtect(&vt[idx], 4, old, &old);
    return orig;
}
static const char* R3Name(DWORD a, char* buf) {
    if (a >= g_r3CrBase && a < g_r3CrEnd) sprintf_s(buf, 16, "CR+%06X", a - g_r3CrBase); else sprintf_s(buf, 16, "%08X", a);
    return buf;
}

// Classic REbirth draws every packet through a handler table indexed by the GPU code (byte 7 of a packet).
#define R3_DISPATCH_OFF (g_r3L.dispatch)   // per Classic REbirth build, see R3Layout in re3.inl
typedef void (__cdecl *R3HandlerFn)(BYTE* pkt);
static R3HandlerFn g_r3Handlers[256];

static void Re3Init() {
    BYTE* j = (BYTE*)0x0040F150;                      // CR replaces WinMain with a jmp into its DLL
    if (j[0] == 0xE9) {
        BYTE* target = j + 5 + *(int*)(j + 1);
        MEMORY_BASIC_INFORMATION mbi;
        if (VirtualQuery(target, &mbi, sizeof mbi)) {
            g_r3CrBase = (DWORD)mbi.AllocationBase;
            IMAGE_DOS_HEADER* dos = (IMAGE_DOS_HEADER*)g_r3CrBase;
            IMAGE_NT_HEADERS* nt = (IMAGE_NT_HEADERS*)(g_r3CrBase + dos->e_lfanew);
            g_r3CrEnd = g_r3CrBase + nt->OptionalHeader.SizeOfImage;
        }
    }
    Log("RE3 detected: CR %08X-%08X", g_r3CrBase, g_r3CrEnd);
    if (!g_r3CrBase) { Log("RE3: Classic REbirth not found - mod inactive"); return; }
    Re3InitInterp();
}
