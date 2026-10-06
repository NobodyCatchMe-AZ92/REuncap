// RE2 (bio2 1.10.exe, Sourcenext 1.10 + Classic REbirth 1.0.9) - frame interpolation.
// Included from reuncap.cpp after the shared pacing code.
//
// Like RE1, RE2 draws 3D models from its ordering table at present time: every model piece is a
// type-0x158 primitive built by 0x444ea0(object, flag, depth) that carries a float model->view matrix
// at prim+0xC (row-major, translation in the 4th column). Classic REbirth runs its own main loop and
// calls the game's tick (0x441880), Clear (0x404d20), RenderOT (0x402bc0) and Present (0x402a80)
// through jump thunks, so we detour those entries.

#define R2_PRESENT_CALLSITE 0x004424C3   // main loop: call Marni::RenderOT+Present (0x402bc0)
#define R2_RENDER_OT        0x00402BC0
#define R2_MARNI_INSERT     0x00402210   // thiscall(marni, prim, depth) ret 8
#define R2_OT_INSERT        0x00416500   // thiscall(ot, prim, depth) ret 8

static int IsRe2() {
    BYTE* b = (BYTE*)R2_PRESENT_CALLSITE;
    __try {
        return b[0] == 0xE8 && (DWORD)(R2_PRESENT_CALLSITE + 5 + *(int*)(b + 1)) == R2_RENDER_OT;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
}

#define R2_TICK          0x00441880   // game tick (thunk to 0x4114e0)
#define R2_SUBMIT        0x00444EA0   // cdecl(object, flag, depth): builds + inserts the model prims
#define R2_SUBMIT_INS1   0x00444FB3   // call Marni::Insert(prim)  - main prim
#define R2_SUBMIT_INS2   0x004450A9   // call Marni::Insert(prim)  - second prim (copy of the main one)
#define R2_CLEAR         0x00404D20   // thiscall Marni::Clear()
#define R2_RENDEROT      0x00402BC0   // thiscall Marni::RenderOT()  (walks the OTs, cursor at OT+0xC)
#define R2_PRESENT       0x00402A80   // thiscall Marni::Present()
#define R2_MARNI_PTR     0x006805B8
#define R2_OT_FIRST      0x008C7024   // five OT headers of 0x14 bytes: {count, buckets, valid, cursor, ?}
#define R2_OT_COUNT      5
#define R2_FRAME_COUNT   0x008C8318   // marni frame counter (RenderOT increments, Present reads)
#define R2_STAGE_ROOM    0x0098EB14   // words: stage, room, camera (CR prints them in its debug overlay)
#define R2_PRIM_MTX      0x0C
#define R2_OT_RESET      0x00402290   // thiscall Marni::ResetOTs() - first game call after CR's tick timer fires

// per-tick trace (DebugLog >= 2): tick interval, Clear offset, extra frames, real-present end offset
struct R2Trace { float dt, clearAt, presentEnd, presentCall, realClear, drawMax, presMax; short extras; short presents; short cut; short unshown; short supp, repl; float lag; short nb, hCut, hPrev, hFail, hDup, same; float tsum, tmin, tmax; };
static R2Trace g_r2tr[64]; static int g_r2trN; static R2Trace g_r2trCur; static double g_r2trTickT;

static int g_sprTagN, g_r2RenderCalls;
static double g_r2AnyTick = 1000.0 / 30.0;
static int g_r2Pacing = 0;   // 0 = continuous grid (exact cap, like RE1), 1 = whole frames per tick   // per tick: tagged sprites, real RenderOT calls

struct R2Rec { void* obj; int site; char* prim; int dup; float m[16]; };
#define R2_MAX_REC 1024
static R2Rec g_r2A[R2_MAX_REC], g_r2B[R2_MAX_REC];
static R2Rec* g_r2cur = g_r2A; static int g_r2curN;
static R2Rec* g_r2prev = g_r2B; static int g_r2prevN;
static int g_r2prevValid, g_r2Overflow;
static void* g_r2SubmitObj;
static double g_r2LastTick;
static int g_r2InGame, g_r2DidFrames, g_r2InExtra;  // g_tickStart is set by R2Reset_Hook
static DWORD g_r2LastCut = 0xFFFFFFFF; static int g_r2CutHold;
static unsigned g_r2TickN, g_r2ResetTicks;   // game ticks run; value at the last ResetOTs call

typedef void (__cdecl *R2SubmitFn)(void* obj, int flag, int depth);
static ThisFn0 g_r2ResetTramp;
static int g_r2Armed = 1;    // next ResetOTs call marks the start of a tick
static R2SubmitFn g_r2SubmitTramp;
static VoidFn     g_r2TickTramp;
static ThisFn0    g_r2ClearTramp, g_r2PresentTramp;

// RE2 rows are [r0 r1 r2 t]; convert to the RE1 layout used by Blend (basis rows + translation at 12..14).
static void R2ToL(const float* m, float* L) {
    memset(L, 0, 64);
    for (int r = 0; r < 3; r++) { L[r * 4 + 0] = m[r * 4 + 0]; L[r * 4 + 1] = m[r * 4 + 1]; L[r * 4 + 2] = m[r * 4 + 2]; L[12 + r] = m[r * 4 + 3]; }
    L[15] = 1.0f;
}
static void LToR2(const float* L, const float* like, float* m) {
    memcpy(m, like, 64);
    for (int r = 0; r < 3; r++) { m[r * 4 + 0] = L[r * 4 + 0]; m[r * 4 + 1] = L[r * 4 + 1]; m[r * 4 + 2] = L[r * 4 + 2]; m[r * 4 + 3] = L[12 + r]; }
}

static R2Rec* R2Find(R2Rec* a, int n, void* obj, int site) {
    for (int i = 0; i < n; i++) if (a[i].obj == obj && a[i].site == site) return &a[i];
    return 0;
}

static void R2Record(int site, char* prim) {
    if (!prim || !g_r2SubmitObj || g_r2InExtra) return;
    R2Rec* r = R2Find(g_r2cur, g_r2curN, g_r2SubmitObj, site);
    if (r) { r->dup = 1; r->prim = prim; memcpy(r->m, prim + R2_PRIM_MTX, 64); return; }
    if (g_r2curN >= R2_MAX_REC) { g_r2Overflow = 1; return; }
    r = &g_r2cur[g_r2curN++];
    r->obj = g_r2SubmitObj; r->site = site; r->prim = prim; r->dup = 0;
    memcpy(r->m, prim + R2_PRIM_MTX, 64);
}
static int __fastcall R2Ins1_Hook(void* marni, void* edx, char* prim, int depth) {
    int ret = ((Ins2Fn)R2_MARNI_INSERT)(marni, edx, prim, depth);
    R2Record(1, prim);
    return ret;
}
static int __fastcall R2Ins2_Hook(void* marni, void* edx, char* prim, int depth) {
    int ret = ((Ins2Fn)R2_MARNI_INSERT)(marni, edx, prim, depth);
    R2Record(2, prim);
    return ret;
}
// Shadows: type-0x58 prims built in fixed per-character slots (0x66c0c0 / 0x66bee0 + i*0x60) by
// 0x43b530 / 0x43b730, with the same model->view matrix layout at +0xC. The slot address is the key.
static const DWORD kR2ShadowSites[] = { 0x43B729, 0x43B938, 0x43B945 };
static int __fastcall R2Shadow_Hook(void* marni, void* edx, char* prim, int depth) {
    int ret = ((Ins2Fn)R2_MARNI_INSERT)(marni, edx, prim, depth);
    if (prim && *(float*)(prim + R2_PRIM_MTX + 15 * 4) == 1.0f) {
        void* saved = g_r2SubmitObj;
        g_r2SubmitObj = prim;
        R2Record(3, prim);
        g_r2SubmitObj = saved;
    }
    return ret;
}

static void __cdecl R2Submit_Hook(void* obj, int flag, int depth) {
    g_r2SubmitObj = obj;
    g_r2SubmitTramp(obj, flag, depth);
    g_r2SubmitObj = 0;
}

static void R2EndTick() {
    R2Rec* t = g_r2prev; g_r2prev = g_r2cur; g_r2cur = t;
    g_r2prevN = g_r2curN; g_r2curN = 0;
    g_r2prevValid = g_r2InGame && !g_r2Overflow;
    g_r2Overflow = 0;
    if (g_r2CutHold > 0) g_r2CutHold--;
}

static void __cdecl R2Tick_Hook(void) {
    // A new tick starts: what was recorded since the last one belongs to the previous tick.
    g_ticks++; g_r2TickN++;
    g_r2DidFrames = 0;
    g_r2TickTramp();
}

// Classic REbirth's timer fires on a fixed QPC grid; the first thing it does afterwards is reset the
// ordering tables. That call is our tick start (the game's own tick function runs some ms later).
static int __fastcall R2Reset_Hook(void* marni, void* edx) {
    // A game tick ran since the last reset but nothing was presented: the tick was never shown (Classic
    // REbirth skips drawing the tick of a camera cut). Its recorded data is discarded - left in place it
    // mixed with the next tick's (old and new camera in one set), which garbled the first blended frame.
    int unshown = !g_r2Armed && !g_r2InExtra && g_r2TickN != g_r2ResetTicks;
    if (unshown) { g_r2curN = 0; g_r2Overflow = 0; g_r2trCur.unshown = 1; g_r2Armed = 1; }
    if (!g_r2InExtra) g_r2ResetTicks = g_r2TickN;
    if (g_r2Armed && !g_r2InExtra) {
        g_r2Armed = 0;
        double now = NowMs();
        if (g_r2LastTick > 0) {
            double d = now - g_r2LastTick;
            if (d > 25.0 && d < 45.0) g_tickPeriod = g_tickPeriod * 0.98 + d * 0.02;   // long-run mean of CR's grid
            if (d > 10.0 && d < 45.0) g_r2AnyTick = g_r2AnyTick * 0.9 + d * 0.1;      // whatever the current loop rate is
            // 30 Hz gameplay; menus/title/doors tick at 60. A single long tick (the new background loading
            // after a camera cut, any other stall) keeps the current state - treating it as "left gameplay"
            // cost the next two ticks their in-between frames (30 fps-looking motion after every cut).
            if (d <= 28.0) g_r2InGame = 0;
            else if (d < 40.0) g_r2InGame = 1;
        }
        if (g_r2trTickT > 0 && g_r2trN < 64) g_r2tr[g_r2trN++] = g_r2trCur;
        memset(&g_r2trCur, 0, sizeof g_r2trCur);
        g_r2trCur.dt = (float)(g_r2trTickT > 0 ? now - g_r2trTickT : 0); g_r2trTickT = now;
        g_r2LastTick = now;
        g_tickStart = now;
        g_r2RenderCalls = 0;
        g_sprTagN = 0;
        g_r2DidFrames = 0;
    }
    return g_r2ResetTramp(marni, edx);
}

// ---- camera cuts ------------------------------------------------------------------------------------
// RE2's cut function (0x4c4460) asks the main loop to skip rendering for 2 ticks (SetSkipRender
// 0x441870) and then loads the new background synchronously. The skip is a PlayStation leftover; on
// PC it just freezes the last frame for 100 ms at every cut, which is very visible at high frame rates.
// SmoothCameraCuts turns that particular request (n == 2 from the cut function) into 1 (the cut tick itself must stay hidden). Game logic is
// untouched - the game simply presents the ticks it was already computing.
#define R2_SET_SKIP          0x00441870
#define R2_CUT_SKIP_CALL     0x004C451A
typedef void (__cdecl *R2SetSkipFn)(int n);
static int g_r2SmoothCuts = 1;
static void __cdecl R2CutSkip_Hook(int n) {
    // Skip exactly the cut tick: its models were submitted with the old camera before the cut and the
    // background is mid-replacement, so that one frame must not be shown. The second skipped tick is
    // redundant on PC.
    if (g_r2SmoothCuts && g_enabled && n == 2) n = 1;
    ((R2SetSkipFn)R2_SET_SKIP)(n);
}

// ---- pass 2 (effect sprites etc.) -----------------------------------------------------------------
// After drawing the ordering tables once, Classic REbirth resets them, runs three more builders
// (0x4dd3b0 effect sprites, 0x4cd090, 0x4c8cca - the last one writes game state, so they cannot be
// re-run) and draws again. In-between frames replay a copy of the previous tick's pass 2, with effect
// sprites extrapolated from their last two positions (pass 2 of the current tick does not exist yet).
#define R2_SPR_PTR       0x00524E1C   // packet pointer used by the 2D sprite builders (0x24 bytes each)
#define R2_SPR_CALL_A    0x004DD4D6   // effect flush -> 0x4407f0
#define R2_SPR_CALL_B    0x00508617   // -> 0x440a20
#define R2_SPR_A         0x004407F0
#define R2_SPR_B         0x00440A20
#define P2_COPY          0x100
#define P2_MAX           768
struct P2Prim { int ot, depth; DWORD desc; short bx[4]; int spr; BYTE data[P2_COPY]; };
static P2Prim* g_p2;            // P2_MAX entries (heap)
static int g_p2N, g_p2Valid;
struct P2Pos { DWORD desc; short x, y; };
static P2Pos g_p2posA[P2_MAX], g_p2posB[P2_MAX];
static P2Pos* g_p2pos = g_p2posA; static int g_p2posN;      // positions of the captured tick
static P2Pos* g_p2posPrev = g_p2posB; static int g_p2posPrevN;
struct SprTag { char* prim; DWORD desc; };
static SprTag g_sprTag[P2_MAX];
static ThisFn0 g_r2RenderTramp2;

typedef int (__cdecl *R2SprFn)(void* desc, int page, int z);
static int R2SprTag(R2SprFn fn, void* desc, int page, int z) {
    char* before = *(char**)R2_SPR_PTR;
    int r = fn(desc, page, z);
    if (*(char**)R2_SPR_PTR == before + 0x24 && g_sprTagN < P2_MAX) { g_sprTag[g_sprTagN].prim = before; g_sprTag[g_sprTagN].desc = (DWORD)desc; g_sprTagN++; }
    return r;
}
// 0x508180 is the 3D billboard-sprite renderer (casings, flashes...): cdecl, 8 args, arg 2 = the effect
// object. Its sprite descriptors are scratch buffers, so sprites it emits are keyed by (object, n).
#define R2_BILLBOARD     0x00508180
static const DWORD kR2BillboardCalls[] = { 0x508055, 0x5080E2, 0x508123 };
typedef int (__cdecl *R2BillFn)(int, void*, int, int, int, int, int, int);
static void* g_r2EffObj; static int g_r2EffIdx;
static int __cdecl R2Billboard_Hook(int a1, void* obj, int a3, int a4, int a5, int a6, int a7, int a8) {
    void* savedObj = g_r2EffObj; int savedIdx = g_r2EffIdx;
    g_r2EffObj = obj; g_r2EffIdx = 0;
    int r = ((R2BillFn)R2_BILLBOARD)(a1, obj, a3, a4, a5, a6, a7, a8);
    g_r2EffObj = savedObj; g_r2EffIdx = savedIdx;
    return r;
}

static int __cdecl R2SprA_Hook(void* desc, int page, int z) { return R2SprTag((R2SprFn)R2_SPR_A, desc, page, z); }
static int __cdecl R2SprB_Hook(void* desc, int page, int z) {
    if (!g_r2EffObj) return R2SprTag((R2SprFn)R2_SPR_B, desc, page, z);
    // key = (effect object, n-th sprite of this object this tick)
    void* key = (void*)((DWORD)g_r2EffObj ^ ((DWORD)(++g_r2EffIdx) * 0x9E3779B1u));
    char* before = *(char**)R2_SPR_PTR;
    int r = ((R2SprFn)R2_SPR_B)(desc, page, z);
    if (*(char**)R2_SPR_PTR == before + 0x24 && g_sprTagN < P2_MAX) { g_sprTag[g_sprTagN].prim = before; g_sprTag[g_sprTagN].desc = (DWORD)key; g_sprTagN++; }
    return r;
}

static DWORD R2FindTag(char* prim) {
    for (int i = 0; i < g_sprTagN; i++) if (g_sprTag[i].prim == prim) return g_sprTag[i].desc;
    return 0;
}

static void R2CapturePass2(char* marni) {
    if (!g_p2) g_p2 = (P2Prim*)malloc(sizeof(P2Prim) * P2_MAX);
    { P2Pos* t = g_p2posPrev; g_p2posPrev = g_p2pos; g_p2pos = t; g_p2posPrevN = g_p2posN; g_p2posN = 0; }
    g_p2N = 0; g_p2Valid = 0;
    for (int i = 0; i < R2_OT_COUNT; i++) {
        DWORD* h = (DWORD*)(marni + R2_OT_FIRST + i * 0x14);
        int cnt = (int)h[0]; char* b = (char*)h[1];
        if (!b || cnt <= 0 || cnt > 0x10000) continue;
        char* lo = b; char* hi = b + cnt * 8;
        for (int k = 0; k < cnt; k++) {
            char* list[256]; int n = 0;
            char* pr = *(char**)(b + k * 8);
            while (pr && !(pr >= lo && pr < hi) && n < 256) { list[n++] = pr; pr = *(char**)pr; }
            for (int j = n - 1; j >= 0; j--) {          // oldest first = original insertion order
                if (g_p2N >= P2_MAX) return;
                P2Prim* c = &g_p2[g_p2N++];
                c->ot = i; c->depth = cnt - 1 - k;
                memcpy(c->data, list[j], P2_COPY);
                DWORD type = *(DWORD*)(c->data + 4) & 0xFFFFF;
                c->spr = (type == 0x1002C || type == 0x1002D);
                c->desc = c->spr ? R2FindTag(list[j]) : 0;
                memcpy(c->bx, c->data + 0x14, 8);
                if (c->spr && c->desc && g_p2posN < P2_MAX) { g_p2pos[g_p2posN].desc = c->desc; g_p2pos[g_p2posN].x = c->bx[0]; g_p2pos[g_p2posN].y = c->bx[1]; g_p2posN++; }
            }
        }
    }
    g_p2Valid = 1;
}

static void R2ReplayPass2(char* marni, float t) {
    if (!g_p2Valid || !g_p2N) return;
    g_r2ResetTramp(marni, 0);
    for (int i = 0; i < g_p2N; i++) {
        P2Prim* c = &g_p2[i];
        memcpy(c->data + 0x14, c->bx, 8);
        if (c->spr && c->desc) {
            for (int k = 0; k < g_p2posPrevN; k++) {
                if (g_p2posPrev[k].desc != c->desc) continue;
                int vx = c->bx[0] - g_p2posPrev[k].x, vy = c->bx[1] - g_p2posPrev[k].y;
                if (vx > 24 || vx < -24 || vy > 24 || vy < -24) break;
                short dx = (short)floorf(vx * t + 0.5f), dy = (short)floorf(vy * t + 0.5f);
                short* q = (short*)(c->data + 0x14);
                q[0] += dx; q[2] += dx; q[1] += dy; q[3] += dy;
                break;
            }
        }
        ((Ins2Fn)R2_OT_INSERT)(marni + R2_OT_FIRST + c->ot * 0x14, 0, (char*)c->data, c->depth);
    }
    ((ThisFn0)R2_RENDEROT)(marni, 0);
}

// Pass-1 sprites (in game the effect flush runs inside the tick): each tagged sprite is shifted toward
// last tick's screen position of the same source entry, like RE1's effect sprites. Static sprites
// (room masks) do not move, so their shift is zero.
static P2Pos g_s1A[P2_MAX], g_s1B[P2_MAX];
static P2Pos* g_s1prev = g_s1A; static int g_s1prevN;
static P2Pos* g_s1cur = g_s1B;
struct S1Shift { short* q; short dx, dy; };
static S1Shift g_s1sh[P2_MAX]; static int g_s1shN;
static int g_s1Shifted;

static void R2SpritesEndTick() {
    int n = 0;
    for (int i = 0; i < g_sprTagN && n < P2_MAX; i++) {
        short* q = (short*)(g_sprTag[i].prim + 0x14);
        g_s1cur[n].desc = g_sprTag[i].desc; g_s1cur[n].x = q[0]; g_s1cur[n].y = q[1]; n++;
    }
    P2Pos* t = g_s1prev; g_s1prev = g_s1cur; g_s1cur = t; g_s1prevN = n;
}
static void R2SpritesShift(float t) {
    g_s1shN = 0;
    float w = 1.0f - t;
    for (int i = 0; i < g_sprTagN; i++) {
        DWORD desc = g_sprTag[i].desc;
        short* q = (short*)(g_sprTag[i].prim + 0x14);
        int dup = 0;
        for (int j = 0; j < i; j++) if (g_sprTag[j].desc == desc) { dup = 1; break; }
        if (dup) continue;
        for (int k = 0; k < g_s1prevN; k++) {
            if (g_s1prev[k].desc != desc) continue;
            int vx = g_s1prev[k].x - q[0], vy = g_s1prev[k].y - q[1];
            if (vx > 48 || vx < -48 || vy > 48 || vy < -48 || (!vx && !vy)) break;
            short dx = (short)floorf(vx * w + 0.5f), dy = (short)floorf(vy * w + 0.5f);
            q[0] += dx; q[2] += dx; q[1] += dy; q[3] += dy;
            g_s1sh[g_s1shN].q = q; g_s1sh[g_s1shN].dx = dx; g_s1sh[g_s1shN].dy = dy; g_s1shN++;
            break;
        }
    }
    g_s1Shifted += g_s1shN;
}
static void R2SpritesRestore() {
    for (int i = 0; i < g_s1shN; i++) { short* q = g_s1sh[i].q; q[0] -= g_s1sh[i].dx; q[2] -= g_s1sh[i].dx; q[1] -= g_s1sh[i].dy; q[3] -= g_s1sh[i].dy; }
    g_s1shN = 0;
}

static int __fastcall R2RenderOT_Hook2(void* marni, void* edx) {
    if (!g_r2InExtra) {
        g_r2RenderCalls++;
        if (g_r2RenderCalls == 2 && g_r2InGame) R2CapturePass2((char*)marni);
    }
    return g_r2RenderTramp2(marni, edx);
}

// ---- OT snapshot ----------------------------------------------------------------------------------
struct R2OTSnap { DWORD hdr[5]; void* buckets; int bytes; };
#define R2_VTX_PTR 0x00543A14   // walk writes vertices here; ResetOTs rewinds it
static DWORD g_r2VtxSaved;
static R2OTSnap g_r2ot[R2_OT_COUNT];
static char* g_r2otBuf[R2_OT_COUNT]; static int g_r2otCap[R2_OT_COUNT];

static void R2Snap(char* marni) {
    g_r2VtxSaved = *(DWORD*)R2_VTX_PTR;
    for (int i = 0; i < R2_OT_COUNT; i++) {
        DWORD* h = (DWORD*)(marni + R2_OT_FIRST + i * 0x14);
        R2OTSnap* s = &g_r2ot[i];
        memcpy(s->hdr, h, 0x14);
        s->buckets = (void*)h[1];
        int cnt = (int)h[0];
        s->bytes = (s->buckets && cnt > 0 && cnt <= 0x10000) ? cnt * 8 : 0;
        if (s->bytes > g_r2otCap[i]) { free(g_r2otBuf[i]); g_r2otBuf[i] = (char*)malloc(s->bytes); g_r2otCap[i] = s->bytes; }
        if (s->bytes) memcpy(g_r2otBuf[i], s->buckets, s->bytes);
    }
}
static void R2Restore(char* marni) {
    *(DWORD*)R2_VTX_PTR = g_r2VtxSaved;
    for (int i = 0; i < R2_OT_COUNT; i++) {
        R2OTSnap* s = &g_r2ot[i];
        memcpy(marni + R2_OT_FIRST + i * 0x14, s->hdr, 0x14);
        if (s->bytes) memcpy(s->buckets, g_r2otBuf[i], s->bytes);
    }
}

static char* g_r2Marni;
static int g_r2Force;   // debug: run extra frames outside gameplay too (to measure present behaviour)
static double g_r2PresSum, g_r2PresMax, g_r2DrawSum; static int g_r2PresN, g_r2ExtraN;
static void R2RenderAt(float t) {
    char* marni = g_r2Marni;
    int nb = 0;
    float L0[16], L1[16], Lo[16], o[16];
    for (int i = 0; i < g_r2curN; i++) {
        R2Rec* c = &g_r2cur[i];
        R2Rec* p = (c->dup || !g_r2prevValid || g_r2CutHold) ? 0 : R2Find(g_r2prev, g_r2prevN, c->obj, c->site);
        if (!p || p->dup) {
            if (c->dup || (p && p->dup)) g_r2trCur.hDup++; else if (g_r2CutHold) g_r2trCur.hCut++; else g_r2trCur.hPrev++;
            continue;
        }
        R2ToL(p->m, L0); R2ToL(c->m, L1);
        if (!memcmp(p->m, c->m, 64)) g_r2trCur.same++;
        if (!Blend(L0, L1, t, Lo)) { g_r2trCur.hFail++; continue; }
        LToR2(Lo, c->m, o);
        memcpy(c->prim + R2_PRIM_MTX, o, 64);
        nb++;
    }
    g_blended += nb;
    g_r2trCur.nb += (short)nb; g_r2trCur.tsum += t;
    if (!g_r2trCur.extras || t < g_r2trCur.tmin) g_r2trCur.tmin = t;
    if (t > g_r2trCur.tmax) g_r2trCur.tmax = t;

    if (g_r2prevValid && !g_r2CutHold) R2SpritesShift(t);
    R2Snap(marni);
    int frameCount = *(int*)(marni + R2_FRAME_COUNT);
    g_r2InExtra = 1;
    double t0 = NowMs();
    g_r2ClearTramp(marni, 0);
    ((ThisFn0)R2_RENDEROT)(marni, 0);
    if (!g_r2CutHold) R2ReplayPass2(marni, t);
    double t1 = NowMs();
    g_r2PresentTramp(marni, 0);
    double t2 = NowMs();
    g_r2DrawSum += t1 - t0; g_r2PresSum += t2 - t1; if (t2 - t1 > g_r2PresMax) g_r2PresMax = t2 - t1; g_r2PresN++; g_r2ExtraN++; g_r2trCur.extras++;
    if (t1 - t0 > g_r2trCur.drawMax) g_r2trCur.drawMax = (float)(t1 - t0);
    if (t2 - t1 > g_r2trCur.presMax) g_r2trCur.presMax = (float)(t2 - t1);
    g_r2InExtra = 0;
    *(int*)(marni + R2_FRAME_COUNT) = frameCount;
    R2Restore(marni);
    R2SpritesRestore();

    for (int i = 0; i < g_r2curN; i++) memcpy(g_r2cur[i].prim + R2_PRIM_MTX, g_r2cur[i].m, 64);
}

// RE2 pacing: a whole number of evenly spaced frames per tick (N = floor(P / f)), the real frame in
// the last slot. RE2's real frame is heavier than RE1's (CR overlays + a second render pass), so a
// sliding grid would periodically land it next to an extra frame; a fixed per-tick schedule keeps
// every frame exactly P/N apart. Late slots are dropped rather than shifting the schedule.
static double g_r2CostAvg = 2.5;
static void R2Pace(double P, RenderAtFn renderAt) {
    double f = FrameIntervalMs();
    if (f <= 0) f = g_presentCost > 1.0 ? g_presentCost : 1.0;   // uncapped: as fast as present allows
    int N = (int)floor(P / f + 0.02); if (N < 1) N = 1; if (N > 16) N = 16;
    double sp = P / N;
    double T = g_tickStart;
    double guard = g_costPeak + 0.3; if (guard > sp) guard = sp;
    for (int k = 0; k < N - 1; k++) {
        double slot = T + k * sp;
        double now = NowMs();
        if (now > slot + sp * 0.5) continue;                        // missed this slot
        if (slot + g_r2CostAvg > T + (N - 1) * sp) break;           // would collide with the real frame
        WaitUntil(slot);
        double a = NowMs();
        renderAt((float)(k + 1) / N);
        double c = NowMs() - a;
        g_r2CostAvg = g_r2CostAvg * 0.9 + c * 0.1;
        g_presentCost = g_presentCost * 0.9 + c * 0.1;
        NotePresent(a);
        g_frames++;
    }
    double realSlot = T + (N - 1) * sp;
    if (realSlot + guard < T + P) WaitUntil(realSlot);
}

// ---- Classic REbirth's own text (save/load screen etc.) ------------------------------------------
// CR 1.0.9 draws some UI text itself, directly into the frame after the game's draw, from a list at
// [CR+0x4369e4] (+0x6ca0 = item count). In-between frames cannot include it, so while CR has text up
// we present only the game's own frames (the screen then runs at its normal rate, without flicker).
#define R2_CR_PATCHED_JMP   0x00441ED0   // CR replaces WinMain's loop with a jmp into its DLL
#define R2_CR_TEXT_CODE     0x00072CD9   // in CR: mov esi,[CR+0x4369e4]; xor edi,edi; mov eax,[esi+0x6ca0]
#define R2_CR_TEXT_PTR_OFF  0x004369E4
static char** g_r2CrTextList;
static void R2FindCrTextList() {
    BYTE* j = (BYTE*)R2_CR_PATCHED_JMP;
    if (j[0] != 0xE9) return;
    BYTE* target = j + 5 + *(int*)(j + 1);
    MEMORY_BASIC_INFORMATION mbi;
    if (!VirtualQuery(target, &mbi, sizeof mbi)) return;
    BYTE* base = (BYTE*)mbi.AllocationBase;
    BYTE* code = base + R2_CR_TEXT_CODE;
    static const BYTE kHead[2] = { 0x8B, 0x35 };
    static const BYTE kTail[8] = { 0x33, 0xFF, 0x8B, 0x86, 0xA0, 0x6C, 0x00, 0x00 };
    __try {
        if (memcmp(code, kHead, 2) || memcmp(code + 6, kTail, 8)) return;
        DWORD imm = *(DWORD*)(code + 2);
        if (imm - (DWORD)base != R2_CR_TEXT_PTR_OFF) return;
        g_r2CrTextList = (char**)imm;
    } __except (EXCEPTION_EXECUTE_HANDLER) { g_r2CrTextList = 0; }
}
static int R2CrTextShown() {
    if (!g_r2CrTextList || !*g_r2CrTextList) return 0;
    return *(int*)(*g_r2CrTextList + 0x6CA0) > 0;
}

// ---- cut catch-up (RE2CutCatchUp) ----------------------------------------------------------------
// The first image of a new camera angle has no earlier image in that camera to blend from, so it would
// stay up for a whole tick (one repeated frame at 60 fps). Instead, the cut tick's frame is not presented
// (the old angle stays up one tick longer, as in vanilla), and from the next tick on every frame is drawn
// a little behind the game - the first one exactly at the cut tick's image - and that lag shrinks to
// zero over a few ticks (motion runs a bit fast for ~130 ms). While behind, the tick's real frame is
// replaced by an in-between frame at the lagged position.
static int g_r2CatchUp = 1, g_r2CuTicks = 4;
static int g_r2CuState;             // 0 off, 1 cut tick hidden - waiting for the first frame, 2 catching up
static int g_r2CuSuppress, g_r2SkipReal, g_r2LastPresentRet = 1;
static double g_r2CuLag0, g_r2CuT0;
static double R2CuLag(double now) {
    double x = (now - g_r2CuT0) / (g_r2CuTicks * g_tickPeriod);
    if (x >= 1.0) { g_r2CuState = 0; return 0; }
    return g_r2CuLag0 * (1.0 - x);
}
static void R2RenderAtCU(float t) {
    if (g_r2CuState == 1) { g_r2CuLag0 = t; g_r2CuT0 = NowMs(); g_r2CuState = 2; }
    if (g_r2CuState == 2) {
        double lag = R2CuLag(NowMs());
        if (lag > g_r2trCur.lag) g_r2trCur.lag = (float)lag;
        t = (float)(t - lag); if (t < 0.f) t = 0.f;
    }
    R2RenderAt(t);
}

static double g_r2RealStart;
static int __fastcall R2Clear_Hook(void* marni, void* edx) {
    static int keyWas;
    int key = (GetAsyncKeyState(g_hotkey) & 0x8000) != 0;
    if (key && !keyWas) { g_enabled = !g_enabled; Log("toggled: %s", g_enabled ? "ON" : "OFF"); }
    keyWas = key;

    DWORD cut = *(DWORD*)R2_STAGE_ROOM ^ ((DWORD)*(WORD*)(R2_STAGE_ROOM + 4) * 0x9E3779B1u);
    if (cut != g_r2LastCut) {
        // No in-between frames on the tick that shows the new camera: last tick's data belongs to the old
        // one, nothing can be blended across the cut. Blending resumes on the next tick.
        g_r2CutHold = 1; g_r2LastCut = cut; g_r2trCur.cut = 1;
        g_r2CuState = 0;
        if (g_r2CatchUp && g_enabled && g_r2InGame && !g_r2InExtra && !R2CrTextShown()) { g_r2CuSuppress = 1; g_r2CuState = 1; }
        if (g_debug >= 2) Log("    cut: words %04X %04X %04X", *(WORD*)R2_STAGE_ROOM, *(WORD*)(R2_STAGE_ROOM + 2), *(WORD*)(R2_STAGE_ROOM + 4));
    }

    if (!g_r2InExtra && !g_r2DidFrames && g_enabled && (g_r2InGame || g_r2Force) && !g_r2CutHold && !R2CrTextShown() && marni == *(void**)R2_MARNI_PTR) {
        g_r2DidFrames = 1;
        g_r2Marni = (char*)marni;
        g_r2trCur.clearAt = (float)(NowMs() - g_tickStart);
        double P = g_r2InGame ? g_tickPeriod : g_r2AnyTick;
        RenderAtFn ra = g_r2CuState ? R2RenderAtCU : R2RenderAt;
        if (g_r2Pacing == 1) R2Pace(P, ra); else PaceExtraFrames(P, ra);
        if (g_r2CuState == 1 || (g_r2CuState == 2 && R2CuLag(NowMs()) > 0.0)) {
            // still behind the game: show the real frame's slot at the lagged position instead
            double a = NowMs();
            R2RenderAtCU(1.0f);
            NotePresent(a); g_frames++; g_r2SkipReal = 1; g_r2trCur.repl = 1;
        }
        g_r2RealStart = g_r2SkipReal ? 0 : NowMs();
        g_r2trCur.realClear = (float)(g_r2RealStart - g_tickStart);
    } else if (!g_r2InExtra && !g_r2DidFrames && !g_r2CutHold) g_r2CuState = 0;   // mod off / left gameplay
    return g_r2ClearTramp(marni, edx);
}
static int __fastcall R2Present_Hook(void* marni, void* edx) {
    if (g_r2InExtra) return g_r2PresentTramp(marni, edx);
    g_r2trCur.presentCall = (float)(NowMs() - g_tickStart);
    int r;
    if (g_r2CuSuppress || g_r2SkipReal) { r = g_r2LastPresentRet; g_r2trCur.supp += g_r2CuSuppress; }   // catch-up: not shown
    else r = g_r2LastPresentRet = g_r2PresentTramp(marni, edx);
    g_r2CuSuppress = g_r2SkipReal = 0;
    R2SpritesEndTick();
    R2EndTick();   // everything submitted up to this present belongs to the frame just shown
    g_r2Armed = 1;
    g_r2trCur.presents++; g_r2trCur.presentEnd = (float)(NowMs() - g_tickStart);
    if (g_r2DidFrames && g_r2RealStart > 0) {
        double c = NowMs() - g_r2RealStart;           // the tick's real frame: Clear .. Present
        if (!g_r2CutHold && c < 30.0) g_costPeak = c > g_costPeak ? c : g_costPeak * 0.9 + c * 0.1;
        NotePresent(g_r2RealStart);
        g_r2RealStart = 0;
    }
    g_frames++;
    if (g_debug) {
        double now = NowMs();
        if (now - g_statT >= 2000.0) {
            double secs = (now - g_statT) / 1000.0;
            double mean = g_ivN ? g_ivSum / g_ivN : 0, sd = g_ivN ? sqrt(g_ivSq / g_ivN - mean * mean) : 0;
            Log("RE2 fps %.1f  ticks/s %.2f (P %.2f ms)  cap %.0f  frame ms avg %.2f sd %.2f max %.2f  blends/tick %.1f  recs %d  ingame %d  guard %.2f",
                g_frames / secs, g_ticks / secs, g_tickPeriod, FrameIntervalMs() > 0 ? 1000.0 / FrameIntervalMs() : 0.0,
                mean, sd, g_ivMax, (double)g_blended / (g_ticks ? g_ticks : 1), g_r2prevN, g_r2InGame, g_costPeak + 0.3);
            if (g_debug >= 2) Log("    sprites: %d tagged/tick, %d shifts in window; pass2: %d prims", g_s1prevN, g_s1Shifted, g_p2N);
            g_s1Shifted = 0;
            if (g_debug >= 2 && g_r2trN) {
                char line[8192]; int o = 0;
                for (int i = 0; i < g_r2trN && o < 8000; i++)
                    o += sprintf_s(line + o, sizeof line - o, " [%s%s%s%.1f x%d t%.2f b%d s%d h%d/%d/%d/%d L%.2f%s]", g_r2tr[i].unshown ? "UNSHOWN " : "", g_r2tr[i].cut ? "CUT " : "", g_r2tr[i].supp ? "HID " : "", g_r2tr[i].dt, g_r2tr[i].extras,
                        g_r2tr[i].extras ? g_r2tr[i].tsum / g_r2tr[i].extras : 0.f, g_r2tr[i].nb, g_r2tr[i].same, g_r2tr[i].hCut, g_r2tr[i].hPrev, g_r2tr[i].hFail, g_r2tr[i].hDup, g_r2tr[i].lag, g_r2tr[i].repl ? "R" : "");
                Log("    trace:%s", line);
                g_r2trN = 0;
            }
            if (g_r2PresN) Log("    extra frames/tick %.2f  draw avg %.2f ms  present avg %.2f max %.2f ms", (double)g_r2ExtraN / (g_ticks ? g_ticks : 1), g_r2DrawSum / g_r2PresN, g_r2PresSum / g_r2PresN, g_r2PresMax);
            g_r2PresSum = g_r2PresMax = g_r2DrawSum = 0; g_r2PresN = g_r2ExtraN = 0;
            g_frames = g_blended = 0; g_ticks = 0; g_ivSum = g_ivSq = g_ivMax = 0; g_ivN = g_ivLong = 0; g_statT = now;
            UpdateRefresh();
        }
    }
    return r;
}

static void Re2InitInterp() {
    g_r2Force = g_debug >= 4;
    g_r2Pacing = GetPrivateProfileIntA("REuncap", "RE2Pacing", 0, g_iniPath);
    g_r2CatchUp = GetPrivateProfileIntA("REuncap", "RE2CutCatchUp", 1, g_iniPath);
    g_r2CuTicks = GetPrivateProfileIntA("REuncap", "RE2CutCatchUpTicks", 4, g_iniPath);
    if (g_r2CuTicks < 1) g_r2CuTicks = 1; if (g_r2CuTicks > 30) g_r2CuTicks = 30;
    static const BYTE kTick[5]    = { 0xB9, 0x60, 0x05, 0x68, 0x00 };                   // mov ecx, 0x680560
    static const BYTE kSubmit[6]  = { 0x53, 0x55, 0x8B, 0x6C, 0x24, 0x0C };             // push ebx; push ebp; mov ebp,[esp+0xc]
    static const BYTE kClear[6]   = { 0x83, 0xEC, 0x10, 0x56, 0x8B, 0xF1 };             // sub esp,0x10; push esi; mov esi,ecx
    static const BYTE kPresent[6] = { 0x83, 0xEC, 0x74, 0x56, 0x8B, 0xF1 };             // sub esp,0x74; push esi; mov esi,ecx
    g_r2TickTramp    = (VoidFn)DetourN(R2_TICK, (void*)R2Tick_Hook, kTick, 5);
    g_r2SubmitTramp  = (R2SubmitFn)DetourN(R2_SUBMIT, (void*)R2Submit_Hook, kSubmit, 6);
    g_r2ClearTramp   = (ThisFn0)DetourN(R2_CLEAR, (void*)R2Clear_Hook, kClear, 6);
    g_r2PresentTramp = (ThisFn0)DetourN(R2_PRESENT, (void*)R2Present_Hook, kPresent, 6);
    static const BYTE kReset[9] = { 0x56, 0x8B, 0xF1, 0x8D, 0x8E, 0x24, 0x70, 0x8C, 0x00 };  // push esi; mov esi,ecx; lea ecx,[esi+0x8c7024]
    g_r2ResetTramp = (ThisFn0)DetourN(R2_OT_RESET, (void*)R2Reset_Hook, kReset, 9);
    int i1 = PatchCallIfTarget(R2_SUBMIT_INS1, R2_MARNI_INSERT, (void*)R2Ins1_Hook);
    int i2 = PatchCallIfTarget(R2_SUBMIT_INS2, R2_MARNI_INSERT, (void*)R2Ins2_Hook);
    int sh = 0;
    for (DWORD a : kR2ShadowSites) sh += PatchCallIfTarget(a, R2_MARNI_INSERT, (void*)R2Shadow_Hook);
    Log("RE2 shadow sites hooked: %d/3", sh);
    int sa = PatchCallIfTarget(R2_SPR_CALL_A, R2_SPR_A, (void*)R2SprA_Hook);
    int sb = PatchCallIfTarget(R2_SPR_CALL_B, R2_SPR_B, (void*)R2SprB_Hook);
    static const BYTE kRender[9] = { 0x56, 0x8B, 0xF1, 0x8B, 0x86, 0xE0, 0x7E, 0x8C, 0x00 };
    g_r2RenderTramp2 = (ThisFn0)DetourN(R2_RENDEROT, (void*)R2RenderOT_Hook2, kRender, 9);
    int bb = 0;
    for (DWORD a : kR2BillboardCalls) bb += PatchCallIfTarget(a, R2_BILLBOARD, (void*)R2Billboard_Hook);
    Log("RE2 pass-2 replay: sprite tags %d/2, renderOT hook %d, billboard calls %d/3", sa + sb, g_r2RenderTramp2 != 0, bb);
    g_r2SmoothCuts = GetPrivateProfileIntA("REuncap", "RE2SmoothCameraCuts",
                         GetPrivateProfileIntA("REuncap", "SmoothCameraCuts", 1, g_iniPath), g_iniPath);  // old name as fallback
    int sc = PatchCallIfTarget(R2_CUT_SKIP_CALL, R2_SET_SKIP, (void*)R2CutSkip_Hook);
    R2FindCrTextList();
    Log("RE2 Classic REbirth text list: %s", g_r2CrTextList ? "found" : "not found (CR text screens may flicker)");
    Log("RE2 tick-start hook %d, cut-skip hook %d (RE2SmoothCameraCuts=%d)", g_r2ResetTramp != 0, sc, g_r2SmoothCuts);
    Log("RE2 interpolation: tick %d submit %d clear %d present %d ins %d/%d",
        g_r2TickTramp != 0, g_r2SubmitTramp != 0, g_r2ClearTramp != 0, g_r2PresentTramp != 0, i1, i2);
}
