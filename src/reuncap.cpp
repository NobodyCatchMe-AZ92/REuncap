// REuncap - high / uncapped frame-rate presentation for Resident Evil 1, 2 and 3 (PC) with Classic REbirth.
//
// Game logic keeps running at its native 30 ticks/s. Between ticks we present as many extra frames as
// the frame cap allows (FpsCap: monitor refresh, uncapped, or a fixed number), each showing every 3D
// object, shadow and effect sprite at the matching point in time between the previous tick and the
// current one. Nothing the game simulates is touched, and the next tick is never delayed.
//
// Addresses are for Biohazard.exe md5 6cbf17329d634433818270e7bb79a308.
#include <windows.h>
#include <mmsystem.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>
#include <share.h>
#include <intrin.h>

#pragma comment(lib, "winmm.lib")

#define REUNCAP_VERSION "1.2"

// ---- Biohazard.exe ---------------------------------------------------------------------------
#define A_MARNI_PTR      0x004CFD38  // CMarni* (thiscall renderer object)
#define A_TICK_MODE      0x004B3290  // 1 = in-game 30 Hz limiter, 0 = 60 Hz (menus/title)
#define A_TICK_PERIOD    0x004B329C  // limiter period in ms when A_TICK_MODE == 1 (33)
#define A_GOV_CALLSITE   0x00484ADE  // main_loop: call FrameRateGovernor
#define A_GOVERNOR       0x00416FC0
#define A_TRANSFORM      0x00436930  // CMarniDirect3DTMD::Transform(marni, depth, float m[16], set) thiscall
#define A_SCREEN_READY   0x004AC438  // dword
#define A_RENDER_READY   0x004AC468  // byte
#define A_STAGE_ROOM_CUT 0x00C386F0  // bytes: stage, room, camera (cut) - changes => camera cut
#define A_SKIP_SCENE     0x004AC3EC  // dword: governor skips walk/present when != 0
#define F_CLEAR          0x0047DCA0  // thiscall Marni::Clear()
#define F_BG_INSERT      0x00433E50  // cdecl  InsertRoomBackground(OT*)
#define F_WALK           0x0047B9D0  // thiscall Marni::RenderOT()  (walks OT at +0x18)
#define F_PRESENT        0x0047B780  // thiscall Marni::Present()
#define A_DBGTXT_CALL    0x004171A3  // call rel32 -> text console draw (patched by Classic REbirth)
#define A_DBGTXT_IDX     0x004B32AC
#define A_DBGTXT_BASE    0x004CFCE8

// Marni object offsets
#define M_OT1            0x18        // primary ordering table
#define M_OT2            0x2C        // secondary OT (filled during the walk)
#define M_SCENE_COUNT    0x1AF8
#define M_DBGTXT_ARG     0x155C
// OT header: +8 count, +0xC bucket count, +0x10 buckets (12 bytes each)
// TMD object: +0x4C0 object count, +0x4D0 objData[2][16] stride 0x84, matrix at objData+8
#define TMD_COUNT        0x4C0
#define TMD_OBJ0         0x4D0
#define TMD_OBJ_STRIDE   0x84
#define TMD_MTX          0x08

typedef int  (__fastcall *ThisFn0)(void* self, void* edx);
typedef int  (__fastcall *ThisFn1)(void* self, void* edx, void* a);
typedef void (__cdecl   *CdeclFn1)(void* a);
typedef int  (__fastcall *TransformFn)(void* tmd, void* edx, void* marni, int depth, float* m, int set);
typedef void (__cdecl   *VoidFn)(void);
typedef int  (__fastcall *Ins2Fn)(void* self, void* edx, char* prim, int depth);

// ---- config / log ----------------------------------------------------------------------------
static FILE* g_log;
static int   g_enabled = 1;
static int   g_hotkey = VK_F12;
static int   g_debug = 0;
static int   g_bench = 0;
static int   g_fpsCap = -1;
static int   g_simLoadMs = 0;   // test only: extra ms of work per in-between frame (simulates slow hardware)   // -1 = monitor refresh, 0 = uncapped, >0 = fps     // >0: render this many extra frames per tick back-to-back and log timings
static float g_maxMove = 2500.0f;   // view-space units per tick beyond which we treat it as a cut
static float g_minDot = 0.5f;       // axis dot below which (>60 deg in one tick) we don't blend

static void Log(const char* fmt, ...) {
    if (!g_log) return;
    va_list ap; va_start(ap, fmt); vfprintf(g_log, fmt, ap); va_end(ap);
    fputc('\n', g_log); fflush(g_log);
}

// ---- matrix records --------------------------------------------------------------------------
struct Rec { void* tmd; int set; int dup; float m[16]; };
#define MAX_REC 2048
static Rec  g_recA[MAX_REC], g_recB[MAX_REC];
static Rec* g_cur = g_recA; static int g_curN;
static Rec* g_prev = g_recB; static int g_prevN;
static int  g_prevValid;
static int  g_overflow;
static int  g_cutHold;      // ticks left during which we must not blend (camera cut)
static DWORD g_lastCut = 0xFFFFFFFF;

static Rec* Find(Rec* arr, int n, void* tmd, int set) {
    for (int i = 0; i < n; i++) if (arr[i].tmd == tmd && arr[i].set == set) return &arr[i];
    return 0;
}

static TransformFn g_TransformTramp;

static DWORD g_diagRet[64]; static int g_diagRetN[64];
static int __fastcall Transform_Hook(void* tmd, void* edx, void* marni, int depth, float* m, int set) {
    int s = set ? 1 : 0;
    if (g_debug >= 2) {
        DWORD ra = (DWORD)_ReturnAddress();
        for (int i = 0; i < 64; i++) { if (g_diagRet[i] == ra || !g_diagRet[i]) { g_diagRet[i] = ra; g_diagRetN[i]++; break; } }
    }
    if (m) {
        Rec* r = Find(g_cur, g_curN, tmd, s);
        if (r) { r->dup = 1; memcpy(r->m, m, 64); }
        else if (g_curN < MAX_REC) { r = &g_cur[g_curN++]; r->tmd = tmd; r->set = s; r->dup = 0; memcpy(r->m, m, 64); }
        else g_overflow = 1;
    }
    return g_TransformTramp(tmd, edx, marni, depth, m, set);
}

static float* ObjMatrix(void* tmd, int set, int i) {
    return (float*)((char*)tmd + TMD_OBJ0 + (set * 16 + i) * TMD_OBJ_STRIDE + TMD_MTX);
}

// Blend rotation (memory rows 0..2 of the 4x4 hold the three basis vectors) and translation.
// Returns 0 if the two poses are too far apart to blend (camera cut, teleport, slot reuse).
static int Blend(const float* a, const float* b, float t, float* o) {
    float dx = b[12] - a[12], dy = b[13] - a[13], dz = b[14] - a[14];
    if (dx * dx + dy * dy + dz * dz > g_maxMove * g_maxMove) return 0;
    for (int r = 0; r < 3; r++) {
        const float* va = a + r * 4; const float* vb = b + r * 4;
        float la = sqrtf(va[0] * va[0] + va[1] * va[1] + va[2] * va[2]);
        float lb = sqrtf(vb[0] * vb[0] + vb[1] * vb[1] + vb[2] * vb[2]);
        if (la < 1e-6f || lb < 1e-6f) return 0;
        float d = (va[0] * vb[0] + va[1] * vb[1] + va[2] * vb[2]) / (la * lb);
        if (d < g_minDot) return 0;
    }
    memcpy(o, b, 64);
    // Non-orthogonal bases (flattened / sheared prims such as ground shadows and blood pools) are blended
    // entry by entry: re-orthonormalising them would remove their shear and make them visibly narrower.
    for (int e = 0; e < 2; e++) {
        const float* m = e ? b : a;
        for (int r = 0; r < 3; r++) for (int q = r + 1; q < 3; q++) {
            const float* u = m + r * 4; const float* w = m + q * 4;
            float lu = sqrtf(u[0] * u[0] + u[1] * u[1] + u[2] * u[2]), lw = sqrtf(w[0] * w[0] + w[1] * w[1] + w[2] * w[2]);
            if (fabsf(u[0] * w[0] + u[1] * w[1] + u[2] * w[2]) > 0.02f * lu * lw) {
                for (int i = 0; i < 12; i++) if ((i & 3) != 3) o[i] = a[i] + (b[i] - a[i]) * t;
                o[12] = a[12] + dx * t; o[13] = a[13] + dy * t; o[14] = a[14] + dz * t;
                return 1;
            }
        }
    }
    float v[3][3], len[3];
    for (int r = 0; r < 3; r++) {
        const float* va = a + r * 4; const float* vb = b + r * 4;
        float la = sqrtf(va[0] * va[0] + va[1] * va[1] + va[2] * va[2]);
        float lb = sqrtf(vb[0] * vb[0] + vb[1] * vb[1] + vb[2] * vb[2]);
        len[r] = la + (lb - la) * t;
        for (int k = 0; k < 3; k++) v[r][k] = va[k] / la + (vb[k] / lb - va[k] / la) * t;
    }
    // Gram-Schmidt on the blended directions keeps the basis orthogonal (no shrink/shear mid-turn).
    // Only applied when the endpoints are themselves orthogonal, so mirrored/sheared bases pass through.
    float n0 = sqrtf(v[0][0] * v[0][0] + v[0][1] * v[0][1] + v[0][2] * v[0][2]);
    for (int k = 0; k < 3; k++) v[0][k] /= n0;
    float p = v[1][0] * v[0][0] + v[1][1] * v[0][1] + v[1][2] * v[0][2];
    for (int k = 0; k < 3; k++) v[1][k] -= p * v[0][k];
    float n1 = sqrtf(v[1][0] * v[1][0] + v[1][1] * v[1][1] + v[1][2] * v[1][2]);
    for (int k = 0; k < 3; k++) v[1][k] /= n1;
    float p0 = v[2][0] * v[0][0] + v[2][1] * v[0][1] + v[2][2] * v[0][2];
    float p1 = v[2][0] * v[1][0] + v[2][1] * v[1][1] + v[2][2] * v[1][2];
    for (int k = 0; k < 3; k++) v[2][k] -= p0 * v[0][k] + p1 * v[1][k];
    float n2 = sqrtf(v[2][0] * v[2][0] + v[2][1] * v[2][1] + v[2][2] * v[2][2]);
    for (int k = 0; k < 3; k++) v[2][k] /= n2;
    for (int r = 0; r < 3; r++) for (int k = 0; k < 3; k++) o[r * 4 + k] = v[r][k] * len[r];
    o[12] = a[12] + dx * t; o[13] = a[13] + dy * t; o[14] = a[14] + dz * t;
    return 1;
}


// ---- free-standing 3D prims (shadows, effects) ---------------------------------------------------
// Marni::InsertPrim (0x47a9b0) thiscall(marni, prim, depth). Type-0x90 prims built outside
// Transform (AddFadePoly shadows at 0x443326, effect polys, sorted sub-objects) carry their own
// 4x4 model->view matrix at prim+8, laid out exactly like a TMD objData. Their slots are rebuilt and
// re-sorted every tick, so we match them to last tick by (call site, handle, texture) + nearest position.
#define F_MARNI_INSERT   0x0047A9B0
#define PRIM_HANDLE      0x54
#define PRIM_TEX         0x58
static const DWORD kPrimInsertSites[] = {
    0x43a2eb,0x43b18a,0x43b91b,0x440e1e,0x4410fd,0x441457,0x441904,0x441de5,0x4422bc,0x4428aa,
    0x442b66,0x442e91,0x443326,0x44371a,0x443c13,0x443c28,0x443e1a,0x44401a };
struct PRec { char* prim; DWORD site; DWORD handle; DWORD tex; int used; float m[16]; };
#define MAX_PREC 1024
static PRec g_pA[MAX_PREC], g_pB[MAX_PREC];
static PRec* g_pcur = g_pA; static int g_pcurN;
static PRec* g_pprev = g_pB; static int g_pprevN;
static float g_maxMovePrim = 800.0f;

static int __fastcall MarniInsert_Hook(void* marni, void* edx, char* prim, int depth) {
    if (prim && *(unsigned char*)(prim + 4) == 0x90 && *(float*)(prim + 8 + 15 * 4) == 1.0f && g_pcurN < MAX_PREC) {
        PRec* r = &g_pcur[g_pcurN++];
        r->prim = prim; r->site = (DWORD)_ReturnAddress(); r->used = 0;
        r->handle = *(DWORD*)(prim + PRIM_HANDLE); r->tex = *(DWORD*)(prim + PRIM_TEX);
        memcpy(r->m, prim + 8, 64);
    }
    return ((Ins2Fn)F_MARNI_INSERT)(marni, edx, prim, depth);
}

static int BlendPrims(int allow, float t) {
    int nb = 0;
    if (!allow) return 0;
    for (int i = 0; i < g_pprevN; i++) g_pprev[i].used = 0;
    for (int i = 0; i < g_pcurN; i++) {
        PRec* c = &g_pcur[i];
        PRec* best = 0; float bd = g_maxMovePrim * g_maxMovePrim;
        for (int k = 0; k < g_pprevN; k++) {
            PRec* p = &g_pprev[k];
            if (p->used || p->site != c->site || p->handle != c->handle || p->tex != c->tex) continue;
            float dx = p->m[12] - c->m[12], dy = p->m[13] - c->m[13], dz = p->m[14] - c->m[14];
            float d = dx * dx + dy * dy + dz * dz;
            if (d <= bd) { bd = d; best = p; }
        }
        float o[16];
        if (best && Blend(best->m, c->m, t, o)) { best->used = 1; memcpy(c->prim + 8, o, 64); nb++; }
    }
    return nb;
}
static void RestorePrims() {
    for (int i = 0; i < g_pcurN; i++) memcpy(g_pcur[i].prim + 8, g_pcur[i].m, 64);
}


// ---- 2D effect sprites (shell casings, sparks, blood...) ----------------------------------------
// EffectActor_UpdateAndRender (0x473d80) projects each pool effect's world position to the screen
// once per tick and builds a 2D sprite via AddSprite (0x440a30, cdecl, 10 args). The sprite prim
// (type 0xA0) lives at 0x68f600 + (half*300 + idx)*0x34 with its screen rect as shorts at +8..+0xE.
// For the extra frame we move each effect sprite halfway back toward last tick's screen position.
#define F_ADDSPRITE      0x00440A30
#define A_SPR_COUNT      0x004B27F8
#define A_PRIM_HALF      0x004B32A8
#define A_SPR_PRIMS      0x0068F600
#define SPR_STRIDE       0x34
#define A_EFF_INDEX      0x00C3F8FE  // byte: effect slot being processed
#define A_EFF_POOL       0x00C330B4  // 64 x 0x84; byte +0 = anim id (0 = free)
static const DWORD kEffSpriteSites[] = { 0x474474, 0x474871 };
typedef int (__cdecl *AddSpriteFn)(int, int, int, int, int, int, int, int, int, int);
struct SRec { DWORD key; short* prim; short x, y; };
#define MAX_SREC 256
static SRec g_sA[MAX_SREC], g_sB[MAX_SREC];
static SRec* g_scur = g_sA; static int g_scurN;
static SRec* g_sprev = g_sB; static int g_sprevN;
static int g_maxSprMove = 48;   // logical (320x240) pixels per tick

static int __cdecl EffSprite_Hook(int a0, int a1, int a2, int a3, int a4, int a5, int a6, int a7, int a8, int a9) {
    int before = *(int*)A_SPR_COUNT, half = *(int*)A_PRIM_HALF;
    int r = ((AddSpriteFn)F_ADDSPRITE)(a0, a1, a2, a3, a4, a5, a6, a7, a8, a9);
    if (*(int*)A_SPR_COUNT == before + 1 && g_scurN < MAX_SREC) {
        int slot = *(unsigned char*)A_EFF_INDEX;
        DWORD site = (DWORD)_ReturnAddress();
        SRec* s = &g_scur[g_scurN++];
        s->key = (DWORD)slot | ((DWORD)*(unsigned char*)(A_EFF_POOL + slot * 0x84) << 8) | (site == kEffSpriteSites[1] + 5 ? 0x10000u : 0u);
        s->prim = (short*)(A_SPR_PRIMS + (half * 300 + before) * SPR_STRIDE);
        s->x = s->prim[4]; s->y = s->prim[5];   // +8, +0xA
    }
    return r;
}

static int BlendSprites(int allow, float t) {
    int nb = 0;
    if (!allow) return 0;
    for (int i = 0; i < g_scurN; i++) {
        SRec* c = &g_scur[i];
        for (int k = 0; k < g_sprevN; k++) {
            SRec* p = &g_sprev[k];
            if (p->key != c->key) continue;
            int dx = p->x - c->x, dy = p->y - c->y;
            if (dx > g_maxSprMove || dx < -g_maxSprMove || dy > g_maxSprMove || dy < -g_maxSprMove) break;
            float w = 1.0f - t;   // fraction of the way still to go from prev to cur
            short hx = (short)floorf(dx * w + 0.5f), hy = (short)floorf(dy * w + 0.5f);
            c->prim[4] += hx; c->prim[6] += hx;   // x0, x1
            c->prim[5] += hy; c->prim[7] += hy;   // y0, y1
            c->x = hx; c->y = hy;                  // remember the shift to undo it
            c->key |= 0x80000000u;
            nb++;
            break;
        }
    }
    return nb;
}
static void RestoreSprites() {
    for (int i = 0; i < g_scurN; i++) {
        SRec* c = &g_scur[i];
        if (!(c->key & 0x80000000u)) continue;
        c->prim[4] -= c->x; c->prim[6] -= c->x; c->prim[5] -= c->y; c->prim[7] -= c->y;
        c->key &= 0x7FFFFFFFu;
        c->x = c->prim[4]; c->y = c->prim[5];
    }
}

// ---- OT snapshot ------------------------------------------------------------------------------
struct OTSnap { char* hdr; int count; int nb; void* buckets; char* copy; };
static char* g_snapBuf[2]; static int g_snapCap[2];

static void SnapOT(OTSnap* s, char* hdr, int which) {
    s->hdr = hdr; s->count = *(int*)(hdr + 8); s->nb = *(int*)(hdr + 0xC); s->buckets = *(void**)(hdr + 0x10);
    int bytes = s->nb * 12;
    if (bytes > g_snapCap[which]) { free(g_snapBuf[which]); g_snapBuf[which] = (char*)malloc(bytes); g_snapCap[which] = bytes; }
    s->copy = g_snapBuf[which];
    if (s->buckets && bytes > 0) memcpy(s->copy, s->buckets, bytes);
}
static void RestoreOT(OTSnap* s) {
    *(int*)(s->hdr + 8) = s->count;
    if (s->buckets && s->nb > 0) memcpy(s->buckets, s->copy, s->nb * 12);
}

// ---- timing -----------------------------------------------------------------------------------
static LARGE_INTEGER g_qpf;
static double NowMs() { LARGE_INTEGER c; QueryPerformanceCounter(&c); return (double)c.QuadPart * 1000.0 / (double)g_qpf.QuadPart; }
static void WaitUntil(double t) {
    for (;;) {
        double r = t - NowMs();
        if (r <= 0) return;
        if (r > 2.5) Sleep(1); else if (r > 0.3) Sleep(0); else YieldProcessor();
    }
}

// ---- the extra frame ---------------------------------------------------------------------------
static ThisFn1 g_dbgTextFn;
static double g_tStart, g_tDraw, g_tAll;
static int g_frames, g_blended, g_held;
static int g_dN, g_dDup, g_dNoPrev, g_dCut, g_dFail, g_dSame; static double g_dT, g_dMove;
static double g_statT;

static void RenderIntermediate(char* marni, float t) {
    // Swap in blended matrices.
    static float saved[MAX_REC][16];
    int nb = 0, nh = 0;
    for (int i = 0; i < g_curN; i++) {
        Rec* c = &g_cur[i];
        Rec* p = (c->dup || !g_prevValid || g_cutHold) ? 0 : Find(g_prev, g_prevN, c->tmd, c->set);
        float o[16];
        int ok = p && !p->dup && Blend(p->m, c->m, t, o);
        if (c->dup) g_dDup++; else if (g_cutHold) g_dCut++; else if (!g_prevValid || !p) g_dNoPrev++; else if (!ok) g_dFail++;
        if (ok) {
            float dx = c->m[12] - p->m[12], dy = c->m[13] - p->m[13], dz = c->m[14] - p->m[14];
            double mv = sqrt(dx * dx + dy * dy + dz * dz); g_dMove += mv;
            if (!memcmp(p->m, c->m, 48)) g_dSame++;
        }
        if (!ok) { nh++; continue; }
        int cnt = *(int*)((char*)c->tmd + TMD_COUNT);
        if (cnt > 16) cnt = 16;
        for (int k = 0; k < cnt; k++) memcpy(ObjMatrix(c->tmd, c->set, k), o, 64);
        nb++;
    }
    g_dT += t; g_dN++;
    nb += BlendPrims(g_prevValid && !g_cutHold, t);
    nb += BlendSprites(g_prevValid && !g_cutHold, t);
    g_blended += nb; g_held += nh;

    g_tStart = NowMs();
    OTSnap s1, s2;
    SnapOT(&s1, marni + M_OT1, 0);
    SnapOT(&s2, marni + M_OT2, 1);
    int sceneCount = *(int*)(marni + M_SCENE_COUNT);

    ((ThisFn0)F_CLEAR)(marni, 0);
    ((CdeclFn1)F_BG_INSERT)(marni + M_OT1);
    if (*(int*)A_SKIP_SCENE == 0) {
        ((ThisFn0)F_WALK)(marni, 0);
        // Read the call target at render time: Classic REbirth redirects this call, and when we are
        // loaded as a CR 1.1.4 mod module that may happen after our Init.
        BYTE* dc = (BYTE*)A_DBGTXT_CALL;
        g_dbgTextFn = dc[0] == 0xE8 ? (ThisFn1)(A_DBGTXT_CALL + 5 + *(int*)(dc + 1)) : 0;
        if (g_dbgTextFn) {
            int idx = *(int*)A_DBGTXT_IDX;
            g_dbgTextFn((void*)(A_DBGTXT_BASE + idx * 12), 0, marni + M_DBGTXT_ARG);
        }
        g_tDraw = NowMs() - g_tStart;
        ((ThisFn0)F_PRESENT)(marni, 0);
    }
    g_tAll = NowMs() - g_tStart;

    RestoreOT(&s1);
    RestoreOT(&s2);
    *(int*)(marni + M_SCENE_COUNT) = sceneCount;

    RestorePrims();
    RestoreSprites();
    // Put the tick's real matrices back for the real frame.
    for (int i = 0; i < g_curN; i++) {
        Rec* c = &g_cur[i];
        int cnt = *(int*)((char*)c->tmd + TMD_COUNT);
        if (cnt > 16) cnt = 16;
        for (int k = 0; k < cnt; k++) memcpy(ObjMatrix(c->tmd, c->set, k), c->m, 64);
    }
    (void)saved;
}


// ---- diagnostic: who inserts what into the OTs ------------------------------------------------
struct InsStat { DWORD ra; int type; int n; char* sample; };
static InsStat g_ins[128]; static int g_insN;
static void InsRecord(DWORD ra, char* prim) {
    int t = prim ? *(unsigned char*)(prim + 4) : -1;
    for (int i = 0; i < g_insN; i++) if (g_ins[i].ra == ra && g_ins[i].type == t) { g_ins[i].n++; g_ins[i].sample = prim; return; }
    if (g_insN < 128) { g_ins[g_insN].ra = ra; g_ins[g_insN].type = t; g_ins[g_insN].n = 1; g_ins[g_insN].sample = prim; g_insN++; }
}
static int __fastcall MarniIns_Diag(void* self, void* edx, char* prim, int depth) {
    InsRecord((DWORD)_ReturnAddress(), prim);
    return ((Ins2Fn)0x0047A9B0)(self, edx, prim, depth);
}
static int __fastcall OTIns_Diag(void* self, void* edx, char* prim, int depth) {
    InsRecord((DWORD)_ReturnAddress(), prim);
    return ((Ins2Fn)0x00462A10)(self, edx, prim, depth);
}
static void DumpIns() {
    for (int i = 0; i < g_insN; i++) {
        InsStat* st = &g_ins[i];
        char hex[3 * 0x60 + 1] = {0};
        if (st->type == 0xA0 || st->type == 0x70 || st->type == 0x80 || st->type == 0x60)
            for (int k = 0; k < 0x60; k++) sprintf_s(hex + k * 3, 4, "%02X ", *(unsigned char*)(st->sample + k));
        Log("  ins from %08X type %02X x%d  %p %s", st->ra - 5, st->type, st->n, st->sample, hex);
    }
    g_insN = 0;
}

// Histogram of OT primitive types (byte at prim+4) - diagnostic only.
static void DiagOT(char* marni) {
    static double last; double now = NowMs();
    if (now - last < 3000.0) return;
    last = now;
    char* hdr = marni + M_OT1;
    int nb = *(int*)(hdr + 0xC); char* b = *(char**)(hdr + 0x10);
    int hist[256] = {0}; void* sample[256] = {0};
    for (int i = 0; i < nb; i++) {
        char* bk = b + i * 12; short c = *(short*)(bk + 4); char* pr = *(char**)bk;
        for (int k = 0; k < c && pr; k++) {
            unsigned t = *(unsigned char*)(pr + 4); hist[t]++; if (!sample[t]) sample[t] = pr;
            pr = *(char**)pr;
        }
    }
    for (int t = 0; t < 256; t++) if (hist[t]) Log("  OT type 0x%02X x%d  e.g. %p", t, hist[t], sample[t]);
    DumpIns();
    for (int i = 0; i < 64 && g_diagRet[i]; i++) { Log("  Transform from %08X x%d", g_diagRet[i], g_diagRetN[i]); g_diagRet[i] = 0; g_diagRetN[i] = 0; }
}


// ---- tick timing / frame pacing ---------------------------------------------------------------
#define A_MAINLOOP_CALLSITE 0x0044959A   // message pump: call main_loop
#define F_MAIN_LOOP         0x00484130
static double g_tickStart, g_tickPeriod = 1000.0 / 30.0, g_lastPresent, g_presentCost = 1.0, g_costPeak = 1.0, g_frameCostPeak = 1.0;
static const double kTickMs = 1000.0 / 30.0;   // Classic REbirth's in-game limiter period
static int    g_ticks;
static double g_ivSum, g_ivSq, g_ivMax; static int g_ivN, g_ivLong;
static int    g_refresh = 60;

static double FrameIntervalMs() {
    int cap = g_fpsCap;
    if (cap < 0) cap = g_refresh;
    if (cap == 0) return 0.0;
    if (cap < 31) cap = 31;
    return 1000.0 / cap;
}
static void NotePresent(double at) {
    if (g_lastPresent > 0) {
        double iv = at - g_lastPresent;
        if (iv > 0 && iv < 100) {
            g_ivSum += iv; g_ivSq += iv * iv; g_ivN++; if (iv > g_ivMax) g_ivMax = iv;
            double f = FrameIntervalMs(); if (f > 0 && iv > f * 1.5) g_ivLong++;
        }
    }
    g_lastPresent = at;
}
typedef char (__cdecl *MainLoopFn)(void);
static MainLoopFn g_MainLoopTramp;
static char __cdecl MainLoop_Hook(void) {
    double now = NowMs();
    if (g_tickStart > 0) {
        double d = now - g_tickStart;
        if (d > 25.0 && d < 45.0) g_tickPeriod = g_tickPeriod * 0.95 + d * 0.05;
    }
    g_tickStart = now;
    g_ticks++;
    return g_MainLoopTramp();
}
static void UpdateRefresh() {
    DEVMODEA dm; memset(&dm, 0, sizeof dm); dm.dmSize = sizeof dm;
    HWND w = GetActiveWindow();
    HMONITOR mon = MonitorFromWindow(w ? w : GetDesktopWindow(), MONITOR_DEFAULTTOPRIMARY);
    MONITORINFOEXA mi; mi.cbSize = sizeof mi;
    if (GetMonitorInfoA(mon, &mi) && EnumDisplaySettingsA(mi.szDevice, ENUM_CURRENT_SETTINGS, &dm) && dm.dmDisplayFrequency > 1)
        g_refresh = (int)dm.dmDisplayFrequency;
}

// ---- shared pacing (RE1 + RE2) ------------------------------------------------------------------
// Frames are spread over the tick on a grid of 1/cap. A frame shown at time tau displays
// t = (tau - tickStart + f + guard) / P, so the tick's real pose (t = 1) is presented at about
// tickStart + P - f - guard and the timeline is continuous across ticks. Classic REbirth starts the
// next tick a fixed period after we hand control back, so we always return before it is due: the
// guard is the real frame's recent worst-case cost, and an in-between frame is only started if it can
// finish in time (on slow hardware we simply show fewer of them). Game speed never changes.
typedef void (*RenderAtFn)(float t);
// Phase lock: when the frame cap is a whole multiple of the tick rate (60, 90, 120 ... fps), the same
// frame pattern repeats every tick and the grid above has no preferred alignment - after a hitch (a room
// load, a door) it can settle where the in-between frames sit late in the tick (e.g. t = 0.85 instead of
// 0.5 at 60 fps), which looks like 30 fps until something resets it. So in that case every slot is pulled
// toward the ideal grid (real frame at t = 1, in-between frames evenly spaced before it), by at most a
// quarter frame per frame, outside a small dead zone. With other caps the alignment drifts from tick to
// tick by itself.
static double g_paceErrSum; static int g_paceErrN, g_paceNudged;
// Returns the (possibly nudged) slot time; *slotK = index of the nearest ideal slot (0 = the real frame's).
static double PaceSlot(double cont, double anchor, double f, int locked, int* slotK) {
    if (slotK) *slotK = 1;
    if (!locked || f <= 0) return cont;
    double k = floor((anchor - cont) / f + 0.5);
    if (slotK) *slotK = (int)k;
    double err = (anchor - k * f) - cont;          // ideal slot nearest to the continuous one
    g_paceErrSum += fabs(err); g_paceErrN++;
    // dead zone: the tick start itself wobbles by a millisecond or so; leave small offsets alone so the
    // frames stay evenly spaced, and only pull back a real lock-in
    double dz = 3.0; if (dz > f * 0.3) dz = f * 0.3;
    if (fabs(err) <= dz) return cont;
    err = err > 0 ? err - dz : err + dz;
    double lim = f * 0.25;
    if (err > lim) err = lim; else if (err < -lim) err = -lim;
    g_paceNudged++;
    return cont + err;
}
static void PaceExtraFrames(double P, RenderAtFn renderAt) {
    double f = FrameIntervalMs();
    double fe = f > 0 ? f : g_presentCost;            // uncapped: frame time = what present allows
    double guard = g_costPeak + 0.3; if (guard > P * 0.5) guard = P * 0.5;
    double deadline = g_tickStart + P;
    double anchor = g_tickStart + P - fe - guard;     // where the tick's real frame (t = 1) belongs
    double ratio = f > 0 ? P / f : 0;
    int locked = f > 0 && fabs(ratio - floor(ratio + 0.5)) < 0.05;
    for (;;) {
        double now = NowMs(), tau = now;
        if (f > 0 && g_lastPresent + f > tau) tau = g_lastPresent + f;
        int k;
        tau = PaceSlot(tau, anchor, f, locked, &k);
        // the next slot is the real frame's own: no in-between frame there (it would only repeat the real
        // pose, which looks like 30 fps) - this is what pulls a mis-aligned grid back into rhythm
        if (k < 1) break;
        if (tau < now) tau = now;
        double t = (tau - g_tickStart + fe + guard) / P;
        if (t >= 0.999) break;               // next grid slot belongs to the tick's real frame
        if (tau + g_frameCostPeak + guard > deadline) break;
        if (t < 0.0) t = 0.0;
        WaitUntil(tau);
        double a = NowMs();
        renderAt((float)t);
        if (g_simLoadMs > 0) WaitUntil(a + g_simLoadMs);
        double c = NowMs() - a;
        g_frameCostPeak = c > g_frameCostPeak ? c : g_frameCostPeak * 0.98 + c * 0.02;
        g_presentCost = g_presentCost * 0.9 + c * 0.1;
        NotePresent(a);
        g_frames++;
    }
    // The real frame's slot is in [tickStart + P - f - guard, tickStart + P - guard).
    double tau = g_lastPresent + f;
    tau = PaceSlot(tau, anchor, f, locked, 0);
    if (f > 0 && tau + guard < deadline) WaitUntil(tau);
}
static char* g_re1Marni;
static void Re1RenderAt(float t) { RenderIntermediate(g_re1Marni, t); }

static void __cdecl Governor_Hook(void) {
    static int keyWas;
    int key = (GetAsyncKeyState(g_hotkey) & 0x8000) != 0;
    if (key && !keyWas) { g_enabled = !g_enabled; Log("toggled: %s", g_enabled ? "ON" : "OFF"); }
    keyWas = key;

    char* marni = *(char**)A_MARNI_PTR;
    int inGame = *(int*)A_TICK_MODE == 1;
    DWORD cut = *(DWORD*)A_STAGE_ROOM_CUT & 0x00FFFFFF;
    if (cut != g_lastCut) { g_cutHold = 2; g_lastCut = cut; }

    int active = g_enabled && inGame && marni && *(int*)A_SCREEN_READY && *(unsigned char*)A_RENDER_READY;
    if (active && g_bench > 0) {
        static double sum, mx, mn = 1e9, dsum, dmx; static int n, ticks;
        for (int k = 0; k < g_bench; k++) {
            double a = NowMs();
            RenderIntermediate(marni, 0.5f);
            double d = NowMs() - a;
            dsum += g_tDraw; if (g_tDraw > dmx) dmx = g_tDraw;
            sum += d; n++; if (d > mx) mx = d; if (d < mn) mn = d;
            g_frames++;
        }
        if (++ticks >= 60) {
            Log("bench: %d extra frames/tick, per frame avg %.3f ms min %.3f max %.3f  (=> max ~%.0f fps)  draw-only avg %.3f max %.3f", g_bench, sum / n, mn, mx, 1000.0 / (sum / n), dsum / n, dmx);
            sum = mx = dsum = dmx = 0; mn = 1e9; n = ticks = 0;
        }
        goto real_frame;
    }
    if (active) {
        g_re1Marni = marni;
        PaceExtraFrames(kTickMs, Re1RenderAt);
    }

    if (g_debug >= 2 && marni) DiagOT(marni);
real_frame:
    {
        double a = NowMs();
        ((VoidFn)A_GOVERNOR)();
        if (active) {
            double c = NowMs() - a;   // cost of the game's own render + present (the tick's real frame)
            // Spikes during camera cuts / room loads (blending is held there anyway) are ignored.
            if (!g_cutHold) g_costPeak = c > g_costPeak ? c : g_costPeak * 0.9 + c * 0.1;
            NotePresent(a);
        }
    }
    g_frames++;

    // Rotate records: this tick becomes "previous".
    Rec* t = g_prev; g_prev = g_cur; g_cur = t;
    g_prevN = g_curN; g_curN = 0;
    { SRec* st = g_sprev; g_sprev = g_scur; g_scur = st; g_sprevN = g_scurN; g_scurN = 0; }
    { PRec* pt = g_pprev; g_pprev = g_pcur; g_pcur = pt; g_pprevN = g_pcurN; g_pcurN = 0; }
    g_prevValid = inGame && !g_overflow;
    g_overflow = 0;
    if (g_cutHold > 0) g_cutHold--;

    if (g_debug) {
        double now = NowMs();
        if (now - g_statT >= 2000.0) {
            double secs = (now - g_statT) / 1000.0;
            double mean = g_ivN ? g_ivSum / g_ivN : 0, sd = g_ivN ? sqrt(g_ivSq / g_ivN - mean * mean) : 0;
            Log("guard %.2f ms  ", g_costPeak + 0.3);
            Log("fps %.1f  ticks/s %.2f (P %.2f ms)  cap %.0f  frame ms avg %.2f sd %.2f max %.2f long %d  blends/tick %.1f  mode %d",
                g_frames / secs, g_ticks / secs, g_tickPeriod, FrameIntervalMs() > 0 ? 1000.0 / FrameIntervalMs() : 0.0,
                mean, sd, g_ivMax, g_ivLong, (double)g_blended / (g_ticks ? g_ticks : 1), *(int*)A_TICK_MODE);
            g_ticks = 0; g_ivSum = g_ivSq = g_ivMax = 0; g_ivN = g_ivLong = 0;
            UpdateRefresh();
            if (g_debug >= 2) for (int i = 0; i < g_pprevN && i < 12; i++)
                Log("   prim %p site %08X h %08X tex %08X t=(%.0f %.0f %.0f)", g_pprev[i].prim, g_pprev[i].site - 5, g_pprev[i].handle, g_pprev[i].tex, g_pprev[i].m[12], g_pprev[i].m[13], g_pprev[i].m[14]);
            if (g_debug >= 2)
                Log("   DIAG extra frames %d avg t %.3f | objects blended %d (identical prev/cur %d, avg move %.1f) held: dup %d no-prev %d cut %d blend-fail %d | prevValid %d cutHold %d cut %06X",
                    g_dN, g_dN ? g_dT / g_dN : 0.0, g_blended, g_dSame, g_blended ? g_dMove / g_blended : 0.0, g_dDup, g_dNoPrev, g_dCut, g_dFail,
                    g_prevValid, g_cutHold, *(DWORD*)A_STAGE_ROOM_CUT & 0xFFFFFF);
            if (g_debug >= 2) Log("   PACE grid error avg %.2f ms over %d slots, nudged %d", g_paceErrN ? g_paceErrSum / g_paceErrN : 0.0, g_paceErrN, g_paceNudged);
            g_paceErrSum = 0; g_paceErrN = g_paceNudged = 0;
            g_dN = g_dDup = g_dNoPrev = g_dCut = g_dFail = g_dSame = 0; g_dT = g_dMove = 0;
            g_frames = g_blended = g_held = 0; g_statT = now;
        }
    }
}

// ---- patching ---------------------------------------------------------------------------------
static int PatchCall(DWORD at, void* to) {
    DWORD old;
    if (*(BYTE*)at != 0xE8) return 0;
    VirtualProtect((void*)at, 5, PAGE_EXECUTE_READWRITE, &old);
    *(int*)(at + 1) = (int)((DWORD)to - (at + 5));
    VirtualProtect((void*)at, 5, old, &old);
    FlushInstructionCache(GetCurrentProcess(), (void*)at, 5);
    return 1;
}

// Detour a function whose first n (>=5) bytes are position-independent whole instructions.
static void* DetourN(DWORD at, void* to, const BYTE* expect, int n) {
    if (memcmp((void*)at, expect, n) != 0) return 0;
    BYTE* tr = (BYTE*)VirtualAlloc(0, 32, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    memcpy(tr, (void*)at, n);
    tr[n] = 0xE9; *(int*)(tr + n + 1) = (int)((at + n) - ((DWORD)tr + n + 5));
    DWORD old;
    VirtualProtect((void*)at, 5, PAGE_EXECUTE_READWRITE, &old);
    *(BYTE*)at = 0xE9; *(int*)(at + 1) = (int)((DWORD)to - (at + 5));
    VirtualProtect((void*)at, 5, old, &old);
    FlushInstructionCache(GetCurrentProcess(), (void*)at, 5);
    return tr;
}

static void* Detour5(DWORD at, void* to, const BYTE* expect) { return DetourN(at, to, expect, 5); }

static char g_iniPath[MAX_PATH];
static void LoadConfig(const char* dir) {
    char* ini = g_iniPath; sprintf_s(g_iniPath, "%sreuncap.ini", dir);
    g_enabled = GetPrivateProfileIntA("REuncap", "Enabled", 1, ini);
    g_hotkey  = GetPrivateProfileIntA("REuncap", "ToggleKey", VK_F12, ini);
    g_debug   = GetPrivateProfileIntA("REuncap", "DebugLog", 0, ini);
    g_bench   = GetPrivateProfileIntA("REuncap", "BenchFrames", 0, ini);
    g_fpsCap  = GetPrivateProfileIntA("REuncap", "FpsCap", -1, ini);
    g_simLoadMs = GetPrivateProfileIntA("REuncap", "SimulateLoadMs", 0, ini);
    char buf[32];
    GetPrivateProfileStringA("REuncap", "MaxMovePerTick", "2500", buf, sizeof buf, ini); g_maxMove = (float)atof(buf);
}

static int PatchCallIfTarget(DWORD site, DWORD expect, void* to) {
    BYTE* b = (BYTE*)site;
    if (b[0] != 0xE8 || (DWORD)(site + 5 + *(int*)(b + 1)) != expect) return 0;
    return PatchCall(site, to);
}
#include "re2.inl"
#include "re3_base.inl"
#include "re3.inl"
static void Re2Init() { Re2InitInterp(); }

// Folder holding reuncap.ini. Loaded as an ASI, Windows tells us. Classic REbirth 1.1.4 maps mod
// modules manually in memory (GetModuleFileName fails), so then we look for the mod_* folder next to
// the exe that contains us.
static void FindOwnDir(HMODULE self, char* dir) {
    if (GetModuleFileNameA(self, dir, MAX_PATH)) {
        char* sl = strrchr(dir, '\\'); if (sl) sl[1] = 0;
        return;
    }
    char exe[MAX_PATH]; GetModuleFileNameA(0, exe, MAX_PATH);
    char* es = strrchr(exe, '\\'); if (es) es[1] = 0;
    strcpy_s(dir, MAX_PATH, exe);
    char pat[MAX_PATH]; sprintf_s(pat, "%smod_*", exe);
    WIN32_FIND_DATAA fd; HANDLE fh = FindFirstFileA(pat, &fd);
    if (fh == INVALID_HANDLE_VALUE) return;
    do {
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) continue;
        char cand[MAX_PATH]; sprintf_s(cand, "%s%s\\reuncap.asi", exe, fd.cFileName);
        if (GetFileAttributesA(cand) != INVALID_FILE_ATTRIBUTES) { sprintf_s(dir, MAX_PATH, "%s%s\\", exe, fd.cFileName); break; }
    } while (FindNextFileA(fh, &fd));
    FindClose(fh);
}

static void Init(HMODULE self) {
    // The same file can be loaded twice (as an ASI and as a Classic REbirth 1.1.4 mod module);
    // only the first copy patches the game.
    char evName[64]; sprintf_s(evName, "Local\\REuncap_%lu", GetCurrentProcessId());
    HANDLE ev = CreateEventA(0, TRUE, FALSE, evName);
    if (!ev || GetLastError() == ERROR_ALREADY_EXISTS) return;
    char dir[MAX_PATH];
    FindOwnDir(self, dir);
    char logp[MAX_PATH]; sprintf_s(logp, "%sreuncap.log", dir);
    g_log = _fsopen(logp, "w", _SH_DENYNO);
    Log("REuncap v" REUNCAP_VERSION);
    LoadConfig(dir);
    QueryPerformanceFrequency(&g_qpf);
    g_statT = NowMs();
    timeBeginPeriod(1);   // 1 ms Sleep granularity for frame pacing (both games)
    if (IsRe2()) { Re2Init(); return; }
    if (IsRe3()) { Re3Init(); return; }

    // Only patch the exact build we reverse-engineered.
    static const BYTE kTransform[5] = { 0x83, 0xEC, 0x04, 0x53, 0x56 };
    BYTE* gc = (BYTE*)A_GOV_CALLSITE;
    if (gc[0] != 0xE8 || (DWORD)(A_GOV_CALLSITE + 5 + *(int*)(gc + 1)) != A_GOVERNOR) {
        Log("unsupported Biohazard.exe (governor call site mismatch) - not patching"); return;
    }
    BYTE* dc = (BYTE*)A_DBGTXT_CALL;
    if (dc[0] == 0xE8) g_dbgTextFn = (ThisFn1)(A_DBGTXT_CALL + 5 + *(int*)(dc + 1));
    g_TransformTramp = (TransformFn)Detour5(A_TRANSFORM, (void*)Transform_Hook, kTransform);
    if (!g_TransformTramp) { Log("unsupported Biohazard.exe (Transform prologue mismatch) - not patching"); return; }
    PatchCall(A_GOV_CALLSITE, (void*)Governor_Hook);
    {
        // main_loop is called by Classic REbirth's own loop (not the game's pump), so hook its entry.
        static const BYTE kMainLoop[7] = { 0x80, 0x3D, 0xAC, 0x30, 0xC3, 0x00, 0x00 };   // cmp byte [0xc330ac], 0
        g_MainLoopTramp = (MainLoopFn)DetourN(F_MAIN_LOOP, (void*)MainLoop_Hook, kMainLoop, 7);
        if (!g_MainLoopTramp) Log("main_loop prologue mismatch - pacing falls back to estimates");
    }
    UpdateRefresh();
    Log("monitor refresh %d Hz, FpsCap %d", g_refresh, g_fpsCap);
    int hooked = 0;
    for (DWORD a : kPrimInsertSites) {
        BYTE* b = (BYTE*)a;
        if (b[0] == 0xE8 && (DWORD)(a + 5 + *(int*)(b + 1)) == F_MARNI_INSERT) hooked += PatchCall(a, (void*)MarniInsert_Hook);
    }
    for (DWORD a : kEffSpriteSites) {
        BYTE* b = (BYTE*)a;
        if (b[0] == 0xE8 && (DWORD)(a + 5 + *(int*)(b + 1)) == F_ADDSPRITE) hooked += PatchCall(a, (void*)EffSprite_Hook);
    }
    Log("prim insert sites hooked: %d/%d", hooked, (int)(sizeof kPrimInsertSites / sizeof kPrimInsertSites[0]) + 2);
    Log("reuncap loaded: enabled=%d toggle=0x%X dbgtext=%p", g_enabled, g_hotkey, g_dbgTextFn);
}

BOOL WINAPI DllMain(HMODULE h, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) { DisableThreadLibraryCalls(h); Init(h); }
    return TRUE;
}
