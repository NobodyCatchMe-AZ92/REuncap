// RE3 (BIOHAZARD(R) 3 PC.exe Sourcenext 1.1.0 + Classic REbirth 1.0.3) - render interpolation.
// Included from reuncap.cpp.
//
// How CR 1.0.3 draws RE3: the game logic runs one tick when a QPC frame counter crosses the next
// 30 Hz grid line (CR+0xC2090, polled continuously by CR's main loop). During the tick the game's
// per-part draw (0x438190) calls 0x438500, which CR replaces with its own float emitter (CR+0xB3150):
// it first projects every vertex of the part through the part's joint matrices and the camera
// (CR+0xB1D10 / CR+0xB1EC0 -> float screen xy array + SZ word array), then writes float polygon
// packets (code 0xD4 = textured tri, 0x40 bytes; 0xD8 = textured quad, 0x54 bytes) into CR's packet
// buffer and links them into the game's ordering tables. At the end of the tick CR's frame render
// (CR+0x7B730) walks the ordering tables and presents.
//
// Extra frames: we record, per projected part, the projection call's arguments, the joint matrices it
// read, and which vertex every packet corner came from (exact float match against the projection's
// output). Before the tick's real render we draw in-between frames: write blended joint matrices into
// the joint slots, re-run CR's own projection into scratch arrays, patch the packets' screen xy/z,
// run CR's frame render, then restore the joints, the packets and the game memory the render touches.

#define R3_TICK_CALL     0x000C2BB4   // CR main loop: call CR+0xC2090 (runs a tick when the grid line is crossed)
#define R3_TICK_FN       0x000C2090
#define R3_RENDER_CALL_  0x0007BE2A   // call CR+0x7B730 (frame render: OT walk + present)
#define R3_RENDER_FN     0x0007B730
#define R3_PROJ_A_CALL   0x000B32C7   // call CR+0xB1D10 (projection, part flag 0x400 clear)
#define R3_PROJ_A_FN     0x000B1D10
#define R3_PROJ_B_CALL   0x000B320B   // call CR+0xB1EC0 (projection, part flag 0x400 set)
#define R3_PROJ_B_FN     0x000B1EC0
// secondary passes (seam vertices: projected under a second joint and averaged into the same arrays)
static const DWORD kR3Pass2Call[4] = { 0x000B3245, 0x000B3282, 0x000B3301, 0x000B333A };
static const DWORD kR3Pass2Fn[4]   = { 0x000B29A0, 0x000B2660, 0x000B2420, 0x000B2100 };
#define R3_PKT_PTR       0x004FB11C   // CR: float packet allocation pointer
#define R3_ZDIV_OFF      0x003F6230   // CR: float, packet z = SZ / this
#define R3_ZDIV_CODE     0x000B2D46   // movss xmm1, [CR+0x3F6230] in the tri emitter
#define R3_VSYNC_CNT     0x00A67CD4   // byte: VSync count per tick (2 = 30 fps gameplay, 1 = 60 fps menus)
#define R3_CAMERA        0x0051F818   // PSX MATRIX: camera
#define R3_GTE_PTR       0x00539C18   // GTE object
#define R3_STATE_LO      0x00A5F000   // game memory touched by a frame render (OT heads, OT index, flags)
#define R3_STATE_HI      0x00A691B8   // = end of .data
#define R3_BG_FLAGS      0x00A6737C   // bit 0x40: background change check disabled
#define R3_BG_DRAWN      0x00A673CA   // word: cut whose background is loaded
#define R3_BG_CUT        0x00A6200E   // byte: current cut
#define R3_BG_RELOAD     0x00A67CD6   // byte: background reload pending
#define R3_JOINT_STRIDE  0xBC
#define R3_JOINT_MTX     0x40

struct R3Mtx { short m[3][3]; short pad; int t[3]; };            // PSX MATRIX (0x20 bytes)
struct R3Joint { DWORD obj; int j; R3Mtx m; };
struct R3Vtx { float* xy; float* z; int idx; float ox, oy, oz; };
#define R3_MAX_PASS 4
struct R3Part {
    int npass; DWORD fn[R3_MAX_PASS], ctx[R3_MAX_PASS][5], stream[R3_MAX_PASS]; int nv;
    BYTE *p0, *p1;
    int v0, vn;                       // range in the vertex map
    int j0, jn;                       // range in the joint list
};
#define R3_MAX_PART  512
#define R3_MAX_JOINT 2048
#define R3_MAX_VTX   65536
static R3Part g_r3Part[R3_MAX_PART]; static int g_r3PartN;
static R3Joint g_r3JA[R3_MAX_JOINT], g_r3JB[R3_MAX_JOINT];
static R3Joint *g_r3Jcur = g_r3JA, *g_r3Jprev = g_r3JB; static int g_r3JcurN, g_r3JprevN;
static R3Vtx g_r3Vtx[R3_MAX_VTX]; static int g_r3VtxN;
static BYTE* g_r3JointPk[R3_MAX_VTX]; static int g_r3JointPkN;   // packets with joint-mapped corners (ascending)
static int g_r3PartRef[R3_MAX_PART][64]; static int g_r3PartRefN[R3_MAX_PART];   // joint list indices per part
static float g_r3Xy[R3_MAX_PART][256][2]; static WORD g_r3Sz[R3_MAX_PART][256];  // projection output at capture
static float g_r3ZDiv = 1.0f;
static R3Mtx g_r3CamPrev, g_r3CamCur; static int g_r3HavePrev, g_r3Overflow;
static double g_r3Poll;
static int g_r3CntProj;
static int g_r3InExtra, g_r3Active;
static BYTE* g_r3StateSnap; static BYTE g_r3GteSnap[0x400]; static BYTE g_r3CrFlagSnap;
static int g_r3Matched, g_r3Unmatched, g_r3Extras, g_r3TickN, g_r3Why[6];
static int g_r3Partial, g_r3DevN, g_r3DevBig; static double g_r3DevSum, g_r3DevMax; static int g_r3JointPath = 1;
struct R3CutTr { float dt, render; int extras, bg, cam, reload; };
static R3CutTr g_r3Ct[16]; static int g_r3CtN, g_r3CtAfter = -1;
typedef HANDLE (WINAPI *R3CfwFn)(LPCWSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE);
typedef BOOL (WINAPI *R3RfFn)(HANDLE, LPVOID, DWORD, LPDWORD, LPOVERLAPPED);
static R3CfwFn g_r3oCfw; static R3RfFn g_r3oRf; static int g_r3InBg, g_r3BgOpens; static double g_r3BgOpenMs, g_r3BgReadMs; static DWORD g_r3BgBytes; static wchar_t g_r3BgName[3][96];
static HANDLE WINAPI R3Cfw(LPCWSTR n, DWORD a, DWORD b, LPSECURITY_ATTRIBUTES c, DWORD d, DWORD e, HANDLE f) {
    double t = NowMs(); HANDLE h = g_r3oCfw(n, a, b, c, d, e, f);
    if (g_r3InBg) { g_r3BgOpenMs += NowMs() - t; if (g_r3BgOpens < 3) wcsncpy_s(g_r3BgName[g_r3BgOpens], n, 95); g_r3BgOpens++; }
    return h;
}
static BOOL WINAPI R3Rf(HANDLE h, LPVOID b, DWORD n, LPDWORD r, LPOVERLAPPED o) {
    double t = NowMs(); BOOL ok = g_r3oRf(h, b, n, r, o);
    if (g_r3InBg) { g_r3BgReadMs += NowMs() - t; g_r3BgBytes += n; }
    return ok;
}
static int g_r3Lost, g_r3LostLate, g_r3LostAfterCut; static double g_r3PrevRealEnd, g_r3PrevStart; static int g_r3PrevCut, g_r3PrevExtras;
static double g_r3StatT, g_r3RealStart;

static DWORD R3Cr(DWORD off) { return g_r3CrBase + off; }

// Call CR's projection: ecx = xy array, edx = SZ array, stack: ctx, stream (caller cleans).
static void R3Project(DWORD fn, void* xy, void* sz, void* ctx, void* stream) {
    __asm {
        push stream
        push ctx
        mov ecx, xy
        mov edx, sz
        call fn
        add esp, 8
    }
}

static int R3StreamInfo(const BYTE* s, int* joints, int* nj, int maxj) {
    int nv = 0; *nj = 0;
    for (int guard = 0; guard < 256 && s[0] != 0xFF; guard++) {
        int j = s[0], n = s[1]; s += 2;
        if (*nj < maxj) joints[(*nj)++] = j;
        for (int k = 0; k < n * 3; k++) if (s[k] != 0xFF && s[k] + 1 > nv) nv = s[k] + 1;
        s += n * 3;
    }
    return nv;
}
static R3Mtx* R3JointPtr(DWORD obj, int j) { return (R3Mtx*)(*(DWORD*)(obj + 0x108) + R3_JOINT_MTX + j * R3_JOINT_STRIDE); }
static int R3FindJoint(R3Joint* list, int n, DWORD obj, int j) {
    for (int i = n - 1; i >= 0; i--) if (list[i].obj == obj && list[i].j == j) return i;
    return -1;
}

static int g_r3LastPartOk;
static void __cdecl R3ProjImpl(float* xy, WORD* sz, DWORD* ctx, BYTE* stream, DWORD fn, int secondary) {
    R3Project(fn, xy, sz, ctx, stream);
    if (g_r3InExtra) return;
    g_r3CntProj++;
    if (secondary) {             // belongs to the part whose primary pass just ran
        if (!g_r3LastPartOk || !g_r3PartN) return;
        __try {
            int idx = g_r3PartN - 1; R3Part& p = g_r3Part[idx];
            int js[64], nj;
            int nv = R3StreamInfo(stream, js, &nj, 64);
            if (p.npass >= R3_MAX_PASS || nv > 256) { g_r3LastPartOk = 0; g_r3PartN--; return; }
            p.fn[p.npass] = fn; memcpy(p.ctx[p.npass], ctx, 20); p.stream[p.npass] = (DWORD)stream; p.npass++;
            if (nv > p.nv) p.nv = nv;
            memcpy(g_r3Xy[idx], xy, p.nv * 8); memcpy(g_r3Sz[idx], sz, p.nv * 2);
            int refN = g_r3PartRefN[idx];
            for (int k = 0; k < nj && refN < 64; k++) {
                int ji = R3FindJoint(g_r3Jcur, g_r3JcurN, ctx[0], js[k]);
                if (ji < 0) {
                    if (g_r3JcurN >= R3_MAX_JOINT) { g_r3Overflow = 1; return; }
                    ji = g_r3JcurN++;
                    g_r3Jcur[ji].obj = ctx[0]; g_r3Jcur[ji].j = js[k]; g_r3Jcur[ji].m = *R3JointPtr(ctx[0], js[k]);
                }
                g_r3PartRef[idx][refN++] = ji;
            }
            g_r3PartRefN[idx] = refN;
        } __except (EXCEPTION_EXECUTE_HANDLER) {}
        return;
    }
    g_r3LastPartOk = 0;
    if (g_r3PartN >= R3_MAX_PART) { g_r3Overflow = 1; return; }
    __try {
        R3Part& p = g_r3Part[g_r3PartN];
        int js[64], nj;
        int nv = R3StreamInfo(stream, js, &nj, 64);
        if (nv <= 0 || nv > 256) return;
        p.npass = 1; p.fn[0] = fn; memcpy(p.ctx[0], ctx, 20); p.stream[0] = (DWORD)stream; p.nv = nv;
        p.p0 = *(BYTE**)R3Cr(R3_PKT_PTR); p.p1 = 0; p.vn = 0; p.v0 = 0;
        memcpy(g_r3Xy[g_r3PartN], xy, nv * 8); memcpy(g_r3Sz[g_r3PartN], sz, nv * 2);
        int refN = 0;
        for (int k = 0; k < nj; k++) {
            int ji = R3FindJoint(g_r3Jcur, g_r3JcurN, ctx[0], js[k]);
            if (ji < 0) {
                if (g_r3JcurN >= R3_MAX_JOINT) { g_r3Overflow = 1; return; }
                ji = g_r3JcurN++;
                g_r3Jcur[ji].obj = ctx[0]; g_r3Jcur[ji].j = js[k]; g_r3Jcur[ji].m = *R3JointPtr(ctx[0], js[k]);
            }
            g_r3PartRef[g_r3PartN][refN++] = ji;
        }
        g_r3PartRefN[g_r3PartN] = refN;
        g_r3PartN++;
        g_r3LastPartOk = 1;
    } __except (EXCEPTION_EXECUTE_HANDLER) {}
}
// Stubs for the projection call sites: ecx/edx + two stack args (caller cleans) -> R3ProjImpl.
#define R3_PROJ_STUB(name, fnoff, sec) \
static __declspec(naked) void name() { __asm { \
    __asm mov eax, esp \
    __asm push sec \
    __asm push fnoff \
    __asm push dword ptr [eax + 8] \
    __asm push dword ptr [eax + 4] \
    __asm push edx \
    __asm push ecx \
    __asm mov eax, g_r3CrBase \
    __asm add dword ptr [esp + 16], eax \
    __asm call R3ProjImpl \
    __asm add esp, 24 \
    __asm ret } }
R3_PROJ_STUB(R3ProjHookA, 0x000B1D10, 0)
R3_PROJ_STUB(R3ProjHookB, 0x000B1EC0, 0)
R3_PROJ_STUB(R3Proj2Hook0, 0x000B29A0, 1)
R3_PROJ_STUB(R3Proj2Hook1, 0x000B2660, 1)
R3_PROJ_STUB(R3Proj2Hook2, 0x000B2420, 1)
R3_PROJ_STUB(R3Proj2Hook3, 0x000B2100, 1)
static void (*const kR3Pass2Hook[4])() = { R3Proj2Hook0, R3Proj2Hook1, R3Proj2Hook2, R3Proj2Hook3 };

// After the tick's emission: close the packet ranges and map every packet corner to its vertex.
static void R3BuildVertexMap() {
    g_r3VtxN = 0; g_r3JointPkN = 0;
    BYTE* end = *(BYTE**)R3Cr(R3_PKT_PTR);
    for (int i = 0; i < g_r3PartN; i++) {
        R3Part& p = g_r3Part[i];
        p.p1 = i + 1 < g_r3PartN ? g_r3Part[i + 1].p0 : end;
        p.v0 = g_r3VtxN; p.vn = 0;
        // hash of the part's projected vertices by exact float bits
        static short hash[512]; memset(hash, -1, sizeof hash);
        static short next[256];
        for (int v = 0; v < p.nv; v++) {
            DWORD h = (*(DWORD*)&g_r3Xy[i][v][0] * 0x9E3779B1u ^ *(DWORD*)&g_r3Xy[i][v][1]) >> 23;
            next[v] = hash[h]; hash[h] = (short)v;
        }
        __try {
            for (BYTE* q = p.p0; q + 0x40 <= p.p1; ) {
                BYTE code = q[7]; int nc, sz; int xo[4], zo[4];
                if (code == 0xD4) { nc = 3; sz = 0x40; xo[0] = 8; xo[1] = 0x18; xo[2] = 0x28; zo[0] = 0x34; zo[1] = 0x38; zo[2] = 0x3C; }
                else if (code == 0xD8) { nc = 4; sz = 0x54; xo[0] = 8; xo[1] = 0x18; xo[2] = 0x28; xo[3] = 0x38; zo[0] = 0x44; zo[1] = 0x48; zo[2] = 0x4C; zo[3] = 0x50; }
                else break;
                if (q + sz > p.p1) break;
                // a packet belongs to the joint path only if every corner is one of this part's vertices;
                // anything else in the range (other objects' packets) is left to the generic path
                int vi[4], m = 0;
                for (int c = 0; c < nc; c++) {
                    float* xy = (float*)(q + xo[c]);
                    DWORD h = (*(DWORD*)&xy[0] * 0x9E3779B1u ^ *(DWORD*)&xy[1]) >> 23;
                    int v = hash[h];
                    while (v >= 0 && (*(DWORD*)&g_r3Xy[i][v][0] != *(DWORD*)&xy[0] || *(DWORD*)&g_r3Xy[i][v][1] != *(DWORD*)&xy[1])) v = next[v];
                    vi[c] = v; if (v >= 0) m++;
                }
                if (m == nc) {
                    if (g_r3VtxN + nc > R3_MAX_VTX) { g_r3Overflow = 1; return; }
                    if (g_r3JointPkN < R3_MAX_VTX) g_r3JointPk[g_r3JointPkN++] = q;
                    for (int c = 0; c < nc; c++) {
                        float* xy = (float*)(q + xo[c]);
                        R3Vtx& e = g_r3Vtx[g_r3VtxN++];
                        e.xy = xy; e.z = (float*)(q + zo[c]); e.idx = vi[c];
                        e.ox = xy[0]; e.oy = xy[1]; e.oz = *e.z;
                    }
                    p.vn += nc; g_r3Matched += nc;
                } else {
                    g_r3Unmatched += nc;
                    if (m > 0) {
                        g_r3Partial++;
                        static int dumps;
                        if (g_debug >= 2 && dumps < 6) {
                            dumps++;
                            char line[512]; int o = sprintf_s(line, "RE3 partial: part %d/%d passes %d nv %d code %02X pkt +%X:", i, g_r3PartN, p.npass, p.nv, code, (DWORD)(q - p.p0));
                            for (int c = 0; c < nc; c++) {
                                float* xy = (float*)(q + xo[c]); float best = 1e9f; int bi = -1;
                                for (int v = 0; v < p.nv; v++) { float dx = g_r3Xy[i][v][0] - xy[0], dy = g_r3Xy[i][v][1] - xy[1]; float d = dx * dx + dy * dy; if (d < best) { best = d; bi = v; } }
                                o += sprintf_s(line + o, sizeof line - o, " (%.3f,%.3f %s v%d d%.4f)", xy[0], xy[1], vi[c] >= 0 ? "hit" : "miss", bi, sqrtf(best));
                            }
                            Log("%s", line);
                        }
                    }
                }
                q += sz;
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) {}
    }
}

// ---- generic path: every other model packet (enemies, objects) -------------------------------------
// All CR emitters link their packets through the AddPrim thunk (CR+0x36CA -> [CR+0x36D0] -> 0x425580), so
// hooking its pointer yields every float model packet in emission order. At the end of a tick the list
// is aligned with the previous tick's (same polygon = same texture words, emitted in the same order);
// matched corners are interpolated in screen space. Packets the joint path re-projects are skipped.
#define R3_ADDPRIM_PTR   0x000036D0
#define R3_MAX_PK        12000
#define R3_MAXMOVE_PX    48.0f        // screen units (320x240 space) per tick beyond which we don't blend
struct R3Pk { BYTE* p; DWORD sig, tag; BYTE nc, joint, lerp; float cur[4][3], prev[4][3]; };
#define R3_CUR_OBJ       0x00A61CC4   // exe: object being processed (set by the object-list loops before drawing)
static R3Pk g_r3PkA[R3_MAX_PK], g_r3PkB[R3_MAX_PK];
static R3Pk *g_r3Pk = g_r3PkA, *g_r3PkPrev = g_r3PkB; static int g_r3PkN, g_r3PkPrevN, g_r3PkOverflow;
static int g_r3GenMatched, g_r3GenMapped, g_r3GenStatic, g_r3GenFar, g_r3OldCross, g_r3TagsMax;
typedef void (__cdecl *R3AddPrimFn)(DWORD, BYTE*);
static R3AddPrimFn g_r3AddPrim;
static const int kR3Xo[4] = { 8, 0x18, 0x28, 0x38 };
static void __cdecl R3AddPrimHook(DWORD ot, BYTE* p) {
    g_r3AddPrim(ot, p);
    if (g_r3InExtra) return;
    BYTE c = p[7];
    if (c != 0xD4 && c != 0xD6 && c != 0xD8 && c != 0xDA) return;
    if (g_r3PkN >= R3_MAX_PK) { g_r3PkOverflow = 1; return; }
    R3Pk& k = g_r3Pk[g_r3PkN++]; k.p = p; k.nc = c >= 0xD8 ? 4 : 3; k.tag = *(DWORD*)R3_CUR_OBJ;
}
static DWORD R3PkSig(const BYTE* p, int nc) {
    DWORD h = p[7] * 0x01000193u;
    h = (h ^ *(DWORD*)(p + 0x10)) * 0x01000193u; h = (h ^ *(DWORD*)(p + 0x20)) * 0x01000193u;
    h = (h ^ *(DWORD*)(p + 0x30)) * 0x01000193u; if (nc == 4) h = (h ^ *(DWORD*)(p + 0x40)) * 0x01000193u;
    return h;
}
static float* R3PkZ(BYTE* p, int nc, int c) { return (float*)(p + (nc == 4 ? 0x44 : 0x34) + c * 4); }
static int R3IsJointPk(BYTE* p);
#define R3_CMAP 16384
static DWORD g_r3MapX[R3_CMAP], g_r3MapY[R3_CMAP]; static float g_r3MapV[R3_CMAP][3]; static BYTE g_r3MapUsed[R3_CMAP];
static void R3MapPut(const float* cur, const float* prev) {
    DWORD kx = *(DWORD*)&cur[0], ky = *(DWORD*)&cur[1];
    DWORD h = ((kx * 0x9E3779B1u) ^ ky) >> 18;
    for (int n = 0; n < R3_CMAP; n++, h = (h + 1) & (R3_CMAP - 1)) {
        if (!g_r3MapUsed[h]) { g_r3MapUsed[h] = 1; g_r3MapX[h] = kx; g_r3MapY[h] = ky; memcpy(g_r3MapV[h], prev, 12); return; }
        if (g_r3MapX[h] == kx && g_r3MapY[h] == ky) return;
    }
}
static int R3MapGet(const float* cur, float* prev) {
    DWORD kx = *(DWORD*)&cur[0], ky = *(DWORD*)&cur[1];
    DWORD h = ((kx * 0x9E3779B1u) ^ ky) >> 18;
    for (int n = 0; n < R3_CMAP && g_r3MapUsed[h]; n++, h = (h + 1) & (R3_CMAP - 1))
        if (g_r3MapX[h] == kx && g_r3MapY[h] == ky) { memcpy(prev, g_r3MapV[h], 12); return 1; }
    return 0;
}
#define R3_PKH 32768
static DWORD g_r3HTag[R3_PKH], g_r3HSig[R3_PKH]; static BYTE g_r3HNc[R3_PKH]; static int g_r3HHead[R3_PKH]; static unsigned g_r3HStamp[R3_PKH], g_r3HCur;
static int g_r3PkNext[R3_MAX_PK]; static BYTE g_r3PkUsed[R3_MAX_PK];
static int R3PkSlot(DWORD tag, DWORD sig, BYTE nc, int create) {
    DWORD h = ((tag * 0x9E3779B1u) ^ (sig * 0x85EBCA6Bu) ^ nc) >> 17;
    for (int n = 0; n < R3_PKH; n++, h = (h + 1) & (R3_PKH - 1)) {
        if (g_r3HStamp[h] != g_r3HCur) {
            if (!create) return -1;
            g_r3HStamp[h] = g_r3HCur; g_r3HTag[h] = tag; g_r3HSig[h] = sig; g_r3HNc[h] = nc; g_r3HHead[h] = -1; return (int)h;
        }
        if (g_r3HTag[h] == tag && g_r3HSig[h] == sig && g_r3HNc[h] == nc) return (int)h;
    }
    return -1;
}
// DebugLog >= 2: how often the old order-only alignment paired packets of two different objects (and
// accepted them), plus the number of distinct objects drawing model packets this tick.
static void R3GenericOldCross() {
    DWORD tags[256]; int nt = 0;
    for (int i = 0; i < g_r3PkN && nt < 256; i++) {
        int k = 0; while (k < nt && tags[k] != g_r3Pk[i].tag) k++;
        if (k == nt) tags[nt++] = g_r3Pk[i].tag;
    }
    if (nt > g_r3TagsMax) g_r3TagsMax = nt;
    int j = 0;
    for (int i = 0; i < g_r3PkN; i++) {
        R3Pk& k = g_r3Pk[i];
        int f = -1;
        for (int q = j; q < g_r3PkPrevN && q < j + 24; q++) if (g_r3PkPrev[q].sig == k.sig && g_r3PkPrev[q].nc == k.nc) { f = q; break; }
        if (f < 0 && i + 2 < g_r3PkN) {
            DWORD s1 = g_r3Pk[i + 1].sig, s2 = g_r3Pk[i + 2].sig;
            for (int q = j; q + 2 < g_r3PkPrevN && q < j + 4096; q++)
                if (g_r3PkPrev[q].sig == k.sig && g_r3PkPrev[q + 1].sig == s1 && g_r3PkPrev[q + 2].sig == s2) { f = q; break; }
        }
        if (f < 0) continue;
        j = f + 1;
        R3Pk& o = g_r3PkPrev[f];
        if (o.tag == k.tag) continue;
        int ok = 1;
        for (int c = 0; c < k.nc && ok; c++) {
            float dx = k.cur[c][0] - o.cur[c][0], dy = k.cur[c][1] - o.cur[c][1];
            if (dx * dx + dy * dy > R3_MAXMOVE_PX * R3_MAXMOVE_PX) ok = 0;
        }
        if (ok) g_r3OldCross++;
    }
}
// Called at the end of a tick (before extra frames): fill positions, align with the previous tick.
static void R3GenericBuild() {
    for (int i = 0; i < g_r3PkN; i++) {
        R3Pk& k = g_r3Pk[i];
        k.sig = R3PkSig(k.p, k.nc); k.joint = (BYTE)R3IsJointPk(k.p); k.lerp = 0;
        for (int c = 0; c < k.nc; c++) { float* xy = (float*)(k.p + kR3Xo[c]); k.cur[c][0] = xy[0]; k.cur[c][1] = xy[1]; k.cur[c][2] = *R3PkZ(k.p, k.nc, c); }
    }
    memset(g_r3MapUsed, 0, sizeof g_r3MapUsed);
    if (g_debug >= 2) R3GenericOldCross();
    // Last tick's packets by (object, signature): the same polygon of the same object. Several zombies of
    // one kind have identical texture words, so matching by signature + draw order alone could pair a limb
    // with another zombie's (stretched / vanishing limbs when they were close together).
    g_r3HCur++;
    for (int q = g_r3PkPrevN - 1; q >= 0; q--) {
        R3Pk& o = g_r3PkPrev[q];
        int h = R3PkSlot(o.tag, o.sig, o.nc, 1);
        g_r3PkNext[q] = h >= 0 ? g_r3HHead[h] : -1; if (h >= 0) g_r3HHead[h] = q;
        g_r3PkUsed[q] = 0;
    }
    for (int i = 0; i < g_r3PkN; i++) {
        R3Pk& k = g_r3Pk[i];
        int h = R3PkSlot(k.tag, k.sig, k.nc, 0);
        if (h < 0) continue;
        int f = -1; float bd = 1e30f;
        for (int q = g_r3HHead[h]; q >= 0; q = g_r3PkNext[q]) {     // nearest unused twin (usually the only one)
            if (g_r3PkUsed[q]) continue;
            float dx = k.cur[0][0] - g_r3PkPrev[q].cur[0][0], dy = k.cur[0][1] - g_r3PkPrev[q].cur[0][1], d = dx * dx + dy * dy;
            if (d < bd) { bd = d; f = q; if (d == 0) break; }
        }
        if (f < 0) continue;
        R3Pk& o = g_r3PkPrev[f];
        int ok = 1;
        for (int c = 0; c < k.nc && ok; c++) {
            float dx = k.cur[c][0] - o.cur[c][0], dy = k.cur[c][1] - o.cur[c][1];
            if (dx * dx + dy * dy > R3_MAXMOVE_PX * R3_MAXMOVE_PX) ok = 0;
        }
        if (!ok) { g_r3GenFar++; continue; }
        g_r3PkUsed[f] = 1;
        k.lerp = 1; g_r3GenMatched++;
        for (int c = 0; c < k.nc; c++) { memcpy(k.prev[c], o.cur[c], 12); R3MapPut(k.cur[c], o.cur[c]); }
    }
    // unmatched packets: corners shared with a matched neighbour follow it
    for (int i = 0; i < g_r3PkN; i++) {
        R3Pk& k = g_r3Pk[i];
        if (k.lerp) continue;
        int found = 0;
        for (int c = 0; c < k.nc; c++) {
            memcpy(k.prev[c], k.cur[c], 12);
            if (R3MapGet(k.cur[c], k.prev[c])) found = 1;
        }
        if (found) { k.lerp = 1; g_r3GenMapped++; } else g_r3GenStatic++;
    }
}
static void R3GenericApply(float t) {
    for (int i = 0; i < g_r3PkN; i++) {
        R3Pk& k = g_r3Pk[i];
        if (!k.lerp || (k.joint && g_r3JointPath)) continue;
        for (int c = 0; c < k.nc; c++) {
            float* xy = (float*)(k.p + kR3Xo[c]);
            xy[0] = k.prev[c][0] + (k.cur[c][0] - k.prev[c][0]) * t;
            xy[1] = k.prev[c][1] + (k.cur[c][1] - k.prev[c][1]) * t;
            *R3PkZ(k.p, k.nc, c) = k.prev[c][2] + (k.cur[c][2] - k.prev[c][2]) * t;
        }
    }
}
static void R3GenericRestore() {
    for (int i = 0; i < g_r3PkN; i++) {
        R3Pk& k = g_r3Pk[i];
        if (!k.lerp || (k.joint && g_r3JointPath)) continue;
        for (int c = 0; c < k.nc; c++) { float* xy = (float*)(k.p + kR3Xo[c]); xy[0] = k.cur[c][0]; xy[1] = k.cur[c][1]; *R3PkZ(k.p, k.nc, c) = k.cur[c][2]; }
    }
}
static void R3GenericEndTick() {
    R3Pk* sw = g_r3PkPrev; g_r3PkPrev = g_r3Pk; g_r3Pk = sw;
    g_r3PkPrevN = g_r3PkOverflow ? 0 : g_r3PkN; g_r3PkN = 0; g_r3PkOverflow = 0;
}

static int R3IsJointPk(BYTE* p) {
    int lo = 0, hi = g_r3JointPkN - 1;          // parts are emitted in buffer order, so the list is ascending
    while (lo <= hi) { int m = (lo + hi) >> 1; if (g_r3JointPk[m] == p) return 1; if (g_r3JointPk[m] < p) lo = m + 1; else hi = m - 1; }
    return 0;
}
static void R3MtxToF(const R3Mtx& a, float* o) {   // rows of o = basis vectors (columns of the PSX matrix)
    memset(o, 0, 64);
    for (int r = 0; r < 3; r++) for (int k = 0; k < 3; k++) o[r * 4 + k] = a.m[k][r] / 4096.0f;
    o[12] = (float)a.t[0]; o[13] = (float)a.t[1]; o[14] = (float)a.t[2]; o[15] = 1.0f;
}
static void R3FToMtx(const float* f, R3Mtx& o) {
    for (int r = 0; r < 3; r++) for (int k = 0; k < 3; k++) {
        float v = f[r * 4 + k] * 4096.0f; v = v < -32768.f ? -32768.f : v > 32767.f ? 32767.f : v;
        o.m[k][r] = (short)(v < 0 ? v - 0.5f : v + 0.5f);
    }
    for (int k = 0; k < 3; k++) { float v = f[12 + k]; o.t[k] = (int)(v < 0 ? v - 0.5f : v + 0.5f); }
}

// ---- 2D path: integer PS1 packets (effects, sprites, shadows) ---------------------------------------
// Every packet is drawn through CR's handler table (CR+0x3EBEC0, indexed by the GPU code at byte 7). During
// the tick's real render we record the integer packets we understand; during an in-between frame each such
// packet is matched to the same-looking packet of the previous tick (same type + texture words, nearest
// position), moved to the blended position just for the draw call, and put back.
#define R3_FX_MAX     4096
#define R3_FX_RADIUS  24          // game pixels per tick beyond which we don't blend
struct R3FxLay { BYTE nv, xy[4], sig[4], nsig; };
static R3FxLay g_r3Lay[64];         // by (code & 0xFC) >> 2 ... codes 0x20..0x7F
static void R3InitLayouts() {
    memset(g_r3Lay, 0, sizeof g_r3Lay);
    auto L = [](int code, int nv, int x0, int x1, int x2, int x3, int s0, int s1, int s2, int s3, int ns) {
        R3FxLay& l = g_r3Lay[(code & 0xFC) >> 2 & 63]; l.nv = (BYTE)nv;
        l.xy[0] = (BYTE)x0; l.xy[1] = (BYTE)x1; l.xy[2] = (BYTE)x2; l.xy[3] = (BYTE)x3;
        l.sig[0] = (BYTE)s0; l.sig[1] = (BYTE)s1; l.sig[2] = (BYTE)s2; l.sig[3] = (BYTE)s3; l.nsig = (BYTE)ns;
    };
    L(0x20, 3, 8, 12, 16, 0,   4, 0, 0, 0, 1);       // F3
    L(0x24, 3, 8, 16, 24, 0,  12, 20, 28, 0, 3);     // FT3
    L(0x28, 4, 8, 12, 16, 20,  4, 0, 0, 0, 1);       // F4
    L(0x2C, 4, 8, 16, 24, 32, 12, 20, 28, 36, 4);    // FT4
    L(0x30, 3, 8, 16, 24, 0,   4, 0, 0, 0, 1);       // G3
    L(0x34, 3, 8, 20, 32, 0,  12, 24, 36, 0, 3);     // GT3
    L(0x38, 4, 8, 16, 24, 32,  4, 0, 0, 0, 1);       // G4
    L(0x3C, 4, 8, 20, 32, 44, 12, 24, 36, 48, 4);    // GT4
    L(0x60, 1, 8, 0, 0, 0,    12, 0, 0, 0, 1);       // TILE (sig: size)
    L(0x64, 1, 8, 0, 0, 0,    12, 16, 0, 0, 2);      // SPRT (sig: uv/clut + size)
    L(0x68, 1, 8, 0, 0, 0,     4, 0, 0, 0, 1);       // TILE1
    L(0x70, 1, 8, 0, 0, 0,     4, 0, 0, 0, 1);       // TILE8
    L(0x74, 1, 8, 0, 0, 0,    12, 0, 0, 0, 1);       // SPRT8
    L(0x78, 1, 8, 0, 0, 0,     4, 0, 0, 0, 1);       // TILE16
    L(0x7C, 1, 8, 0, 0, 0,    12, 0, 0, 0, 1);       // SPRT16
}
static const R3FxLay* R3Lay(BYTE code) {
    if (code < 0x20 || code >= 0x80 || (code >= 0x40 && code < 0x60)) return 0;
    const R3FxLay* l = &g_r3Lay[(code & 0xFC) >> 2 & 63];
    return l->nv ? l : 0;
}
struct R3Fx { DWORD sig, rsig; short xy[4][2]; float cx, cy; BYTE code, nv; };
static R3Fx g_r3FxA[R3_FX_MAX], g_r3FxB[R3_FX_MAX];
static R3Fx *g_r3Fx = g_r3FxA, *g_r3FxPrev = g_r3FxB; static int g_r3FxN, g_r3FxPrevN;
static int g_r3FxRecord;            // 1 during the real render
static float g_r3FxT; static int g_r3FxCursor;
static DWORD g_r3SealKey[2048], g_r3SealStamp[2048], g_r3SealFrame = 1; static short g_r3SealVal[2048][2];               // blend factor of the in-between frame being drawn
static int g_r3FxCnt[256], g_r3FxMoved[256], g_r3FxMiss[256];
// relaxed signature: same type, palette and texture page (animated sprites change uv every tick)
static DWORD R3FxRSig(const BYTE* p, const R3FxLay* l) {
    BYTE c = p[7] & 0xFC;
    DWORD h = (p[7] | 0x100) * 0x01000193u;
    if (c == 0x24 || c == 0x2C || c == 0x34 || c == 0x3C) { h = (h ^ *(WORD*)(p + l->sig[0] + 2)) * 0x01000193u; h = (h ^ *(WORD*)(p + l->sig[1] + 2)) * 0x01000193u; }
    else if (c == 0x64) { h = (h ^ *(WORD*)(p + 14)) * 0x01000193u; h = (h ^ *(DWORD*)(p + 16)) * 0x01000193u; }
    else if (c == 0x74 || c == 0x7C) h = (h ^ *(WORD*)(p + 14)) * 0x01000193u;
    return h;
}
static void R3FxCentre(const BYTE* p, const R3FxLay* l, float* cx, float* cy) {
    float x = 0, y = 0;
    for (int v = 0; v < l->nv; v++) { x += *(short*)(p + l->xy[v]); y += *(short*)(p + l->xy[v] + 2); }
    *cx = x / l->nv; *cy = y / l->nv;
}
static int g_r3FxRelaxed;
static DWORD R3FxSig(const BYTE* p, const R3FxLay* l) {
    DWORD h = p[7] * 0x01000193u;
    for (int i = 0; i < l->nsig; i++) h = (h ^ *(DWORD*)(p + l->sig[i])) * 0x01000193u;
    return h;
}
static int g_r3FloatDrawn, g_r3FloatCollected;
static void __cdecl R3FloatCount(BYTE* p) { if (g_r3FxRecord) g_r3FloatDrawn++; g_r3Handlers[p[7]](p); }
static void __cdecl R3FxHandler(BYTE* p) {
    BYTE code = p[7];
    const R3FxLay* l = R3Lay(code);
    if (!l) { g_r3Handlers[code](p); return; }
    if (g_r3FxRecord) {
        if (g_r3FxN < R3_FX_MAX) {
            R3Fx& f = g_r3Fx[g_r3FxN++]; f.code = code; f.nv = l->nv; f.sig = R3FxSig(p, l); f.rsig = R3FxRSig(p, l); R3FxCentre(p, l, &f.cx, &f.cy);
            for (int v = 0; v < l->nv; v++) { f.xy[v][0] = *(short*)(p + l->xy[v]); f.xy[v][1] = *(short*)(p + l->xy[v] + 2); }
        }
        g_r3Handlers[code](p); return;
    }
    if (!g_r3InExtra) { g_r3Handlers[code](p); return; }
    // in-between frame: find last tick's twin (same type/texture, nearest anchor)
    DWORD sig = R3FxSig(p, l);
    short x0 = *(short*)(p + l->xy[0]), y0 = *(short*)(p + l->xy[0] + 2);
    int best = -1, bd = R3_FX_RADIUS * R3_FX_RADIUS + 1;
    // draw order is stable from tick to tick: take the next look-alike after the previous match, so
    // identical pieces (e.g. the many triangles of a shadow) keep their own twins
    for (int i = g_r3FxCursor; i < g_r3FxPrevN && i < g_r3FxCursor + 48; i++) {
        R3Fx& f = g_r3FxPrev[i];
        if (f.sig != sig || f.nv != l->nv) continue;
        int dx = f.xy[0][0] - x0, dy = f.xy[0][1] - y0, d = dx * dx + dy * dy;
        if (d < bd) { bd = d; best = i; }
        break;
    }
    if (best >= 0) g_r3FxCursor = best + 1;
    else for (int i = 0; i < g_r3FxPrevN; i++) {
        R3Fx& f = g_r3FxPrev[i];
        if (f.sig != sig) continue;
        int dx = f.xy[0][0] - x0, dy = f.xy[0][1] - y0, d = dx * dx + dy * dy;
        if (d < bd) { bd = d; best = i; if (!d) break; }
    }
    if (best < 0) {
        // no exact twin: same sprite in another animation frame -> shift the current frame by its centre
        DWORD rsig = R3FxRSig(p, l); float cx, cy; R3FxCentre(p, l, &cx, &cy);
        int rb = -1; float rd = (float)(R3_FX_RADIUS * R3_FX_RADIUS) + 1;
        for (int i = 0; i < g_r3FxPrevN; i++) {
            R3Fx& f = g_r3FxPrev[i];
            if (f.rsig != rsig) continue;
            float dx = f.cx - cx, dy = f.cy - cy, d = dx * dx + dy * dy;
            if (d < rd) { rd = d; rb = i; }
        }
        if (g_debug >= 2) { g_r3FxCnt[code]++; if (rb < 0) g_r3FxMiss[code]++; else g_r3FxRelaxed++; }
        if (rb < 0 || rd == 0) { g_r3Handlers[code](p); return; }
        float ox = (g_r3FxPrev[rb].cx - cx) * (1.0f - g_r3FxT), oy = (g_r3FxPrev[rb].cy - cy) * (1.0f - g_r3FxT);
        short sx = (short)floorf(ox + 0.5f), sy = (short)floorf(oy + 0.5f);
        if (!sx && !sy) { g_r3Handlers[code](p); return; }
        for (int v = 0; v < l->nv; v++) { short* q = (short*)(p + l->xy[v]); q[0] += sx; q[1] += sy; }
        g_r3Handlers[code](p);
        for (int v = 0; v < l->nv; v++) { short* q = (short*)(p + l->xy[v]); q[0] -= sx; q[1] -= sy; }
        return;
    }
    R3Fx& f = g_r3FxPrev[best];
    int moved = 0;
    for (int v = 0; v < l->nv; v++) if (f.xy[v][0] != *(short*)(p + l->xy[v]) || f.xy[v][1] != *(short*)(p + l->xy[v] + 2)) moved = 1;
    if (g_debug >= 2) { g_r3FxCnt[code]++; if (moved) g_r3FxMoved[code]++; }
    if (!moved) { g_r3Handlers[code](p); return; }
    short saved[4][2];
    float t = g_r3FxT;
    for (int v = 0; v < l->nv; v++) {
        short* q = (short*)(p + l->xy[v]); saved[v][0] = q[0]; saved[v][1] = q[1];
        // a corner shared by several pieces of the same kind gets one position per frame (no seams)
        DWORD key = ((DWORD)(WORD)q[0] << 16 | (WORD)q[1]) ^ ((DWORD)code << 24);
        DWORD h = (key * 0x9E3779B1u) >> 21; int hit = -1;
        for (int n = 0; n < 2048; n++, h = (h + 1) & 2047) {
            if (g_r3SealStamp[h] != g_r3SealFrame) break;
            if (g_r3SealKey[h] == key) { hit = (int)h; break; }
        }
        if (hit >= 0) { q[0] = g_r3SealVal[hit][0]; q[1] = g_r3SealVal[hit][1]; continue; }
        float x = f.xy[v][0] + (q[0] - f.xy[v][0]) * t, y = f.xy[v][1] + (q[1] - f.xy[v][1]) * t;
        short nx = (short)floorf(x + 0.5f), ny = (short)floorf(y + 0.5f);
        if (g_r3SealStamp[h] != g_r3SealFrame) { g_r3SealStamp[h] = g_r3SealFrame; g_r3SealKey[h] = key; g_r3SealVal[h][0] = nx; g_r3SealVal[h][1] = ny; }
        q[0] = nx; q[1] = ny;
    }
    g_r3Handlers[code](p);
    for (int v = 0; v < l->nv; v++) { short* q = (short*)(p + l->xy[v]); q[0] = saved[v][0]; q[1] = saved[v][1]; }
}
static void R3FxEndTick(int valid) {
    R3Fx* sw = g_r3FxPrev; g_r3FxPrev = g_r3Fx; g_r3Fx = sw;
    g_r3FxPrevN = valid ? g_r3FxN : 0; g_r3FxN = 0;
}
static int R3InstallFx() {
    R3InitLayouts();
    void** tbl = (void**)R3Cr(R3_DISPATCH_OFF);
    int n = 0;
    if (g_debug >= 2) for (int code = 0xD4; code <= 0xDA; code += 2) {
        DWORD f = (DWORD)tbl[code];
        if (f < g_r3CrBase || f >= g_r3CrEnd) continue;
        g_r3Handlers[code] = (R3HandlerFn)f; PatchVtbl(tbl, code, (void*)R3FloatCount);
    }
    for (int code = 0x20; code < 0x80; code++) {
        if (!R3Lay((BYTE)code)) continue;
        DWORD f = (DWORD)tbl[code];
        if (f < g_r3CrBase || f >= g_r3CrEnd) continue;
        g_r3Handlers[code] = (R3HandlerFn)f;
        PatchVtbl(tbl, code, (void*)R3FxHandler); n++;
    }
    return n;
}

static void R3RenderAt(float t) {
    // 1. blended joints
    static R3Mtx saved[R3_MAX_JOINT]; static int savedIdx[R3_MAX_JOINT]; int ns = 0;
    __try {
        for (int i = 0; i < g_r3JcurN; i++) {
            R3Joint& c = g_r3Jcur[i];
            int pi = R3FindJoint(g_r3Jprev, g_r3JprevN, c.obj, c.j);
            if (pi < 0) continue;
            float a[16], b[16], o[16];
            R3MtxToF(g_r3Jprev[pi].m, a); R3MtxToF(c.m, b);
            if (!Blend(a, b, t, o)) continue;
            R3Mtx* slot = R3JointPtr(c.obj, c.j);
            saved[ns] = *slot; savedIdx[ns++] = i;
            R3Mtx m = c.m; R3FToMtx(o, m); *slot = m;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {}
    // 2. re-project every part into scratch arrays and patch its packets
    static float xy[256][2]; static WORD sz[256];
    static BYTE mscratch[0x40], vscratch[0x40];
    for (int i = 0; i < g_r3PartN; i++) {
        R3Part& p = g_r3Part[i];
        if (!p.vn || !g_r3JointPath) continue;
        int okp = 1;
        for (int ps = 0; ps < p.npass && okp; ps++) {
            DWORD ctx[5]; memcpy(ctx, p.ctx[ps], 20); ctx[3] = (DWORD)mscratch; ctx[4] = (DWORD)vscratch;
            __try { R3Project(p.fn[ps], xy, sz, ctx, (void*)p.stream[ps]); } __except (EXCEPTION_EXECUTE_HANDLER) { okp = 0; }
        }
        if (!okp) continue;
        for (int k = 0; k < p.vn; k++) {
            R3Vtx& e = g_r3Vtx[p.v0 + k];
            e.xy[0] = xy[e.idx][0]; e.xy[1] = xy[e.idx][1]; *e.z = sz[e.idx] / g_r3ZDiv;
        }
    }
    if (g_debug >= 2) {
        for (int i = 0; i < g_r3PkN; i++) {
            R3Pk& k = g_r3Pk[i];
            if (!k.joint || !k.lerp) continue;
            for (int c = 0; c < k.nc; c++) {
                float* xy = (float*)(k.p + kR3Xo[c]);
                float ex = k.prev[c][0] + (k.cur[c][0] - k.prev[c][0]) * t, ey = k.prev[c][1] + (k.cur[c][1] - k.prev[c][1]) * t;
                double d = sqrt((double)(xy[0] - ex) * (xy[0] - ex) + (double)(xy[1] - ey) * (xy[1] - ey));
                g_r3DevSum += d; g_r3DevN++; if (d > g_r3DevMax) g_r3DevMax = d; if (d > 2.0) g_r3DevBig++;
            }
        }
    }
    R3GenericApply(t);
    // 3. joints back
    for (int k = 0; k < ns; k++) { R3Joint& c = g_r3Jcur[savedIdx[k]]; *R3JointPtr(c.obj, c.j) = saved[k]; }
    // 4. render + present, then undo everything the render changed
    g_r3InExtra = 1; g_r3FxT = t; g_r3FxCursor = 0; g_r3SealFrame++;
    ((void(*)())R3Cr(R3_RENDER_FN))();
    g_r3InExtra = 0;
    memcpy((void*)R3_STATE_LO, g_r3StateSnap, R3_STATE_HI - R3_STATE_LO);
    *(BYTE*)R3Cr(0x401220) = g_r3CrFlagSnap;
    for (int k = 0; k < g_r3VtxN; k++) { R3Vtx& e = g_r3Vtx[k]; e.xy[0] = e.ox; e.xy[1] = e.oy; *e.z = e.oz; }
    R3GenericRestore();
    g_r3Extras++;
}

// debug census: which exe draw path each part takes (DebugLog >= 2)
static int g_r3CntA, g_r3CntB; static DWORD g_r3ObjA[64], g_r3ObjB[64]; static int g_r3ObjAN, g_r3ObjBN;
static void R3NoteObj(DWORD* list, int* n) { DWORD o = *(DWORD*)0xA61CC4; for (int i = 0; i < *n; i++) if (list[i] == o) return; if (*n < 64) list[(*n)++] = o; }
static int __cdecl R3DrawA(DWORD a, DWORD f, DWORD p) { g_r3CntA++; R3NoteObj(g_r3ObjA, &g_r3ObjAN); return ((int(__cdecl*)(DWORD, DWORD, DWORD))0x438500)(a, f, p); }
static int __cdecl R3DrawB(DWORD a, DWORD f, DWORD p) { g_r3CntB++; R3NoteObj(g_r3ObjB, &g_r3ObjBN); return ((int(__cdecl*)(DWORD, DWORD, DWORD))0x438690)(a, f, p); }

static void __cdecl R3TickPoll() {
    g_r3Poll = NowMs();
    ((void(*)())R3Cr(R3_TICK_FN))();
}

static void __cdecl R3Render() {
    static int keyWas;
    int key = (GetAsyncKeyState(g_hotkey) & 0x8000) != 0;
    if (key && !keyWas) { g_enabled = !g_enabled; Log("toggled: %s", g_enabled ? "ON" : "OFF"); }
    keyWas = key;

    double now = NowMs();
    if (g_tickStart > 0) {
        double d = g_r3Poll - g_tickStart;
        if (d > 25.0 && d < 45.0) g_tickPeriod = g_tickPeriod * 0.95 + d * 0.05;
        if (*(BYTE*)R3_VSYNC_CNT == 2 && d > g_tickPeriod * 1.5) {
            g_r3Lost++;
            if (g_r3PrevRealEnd > g_tickStart + g_tickPeriod) g_r3LostLate++;
            if (g_r3PrevCut) g_r3LostAfterCut++;
            if (g_debug >= 2) Log("    lost tick: gap %.1f ms, prev tick extras %d, prev real render ended at %.1f ms, prev cut %d",
                d, g_r3PrevExtras, g_r3PrevRealEnd - g_tickStart, g_r3PrevCut);
        }
    }
    int extrasBefore = g_r3Extras;
    g_tickStart = g_r3Poll; g_ticks++; g_r3TickN++;

    R3BuildVertexMap();
    R3GenericBuild();
    g_r3CamCur = *(R3Mtx*)R3_CAMERA;
    // A camera cut shows up first as a pending background change (CR's render compares the game's cut
    // number with the one last drawn, exactly like this) and one tick later as a new camera matrix.
    int bgCut = (!(*(BYTE*)R3_BG_FLAGS & 0x40) && *(WORD*)R3_BG_DRAWN != *(BYTE*)R3_BG_CUT) || *(BYTE*)R3_BG_RELOAD;
    int cut = bgCut || memcmp(&g_r3CamCur, &g_r3CamPrev, sizeof(R3Mtx)) != 0;
    int vs = *(BYTE*)R3_VSYNC_CNT;
    g_r3Active = g_enabled && vs == 2 && g_r3HavePrev && !cut && !g_r3Overflow && (g_r3PartN > 0 || g_r3PkN > 0);
    if (!g_enabled) g_r3Why[0]++; else if (vs != 2) g_r3Why[1]++; else if (g_r3PartN <= 0 && g_r3PkN <= 0) g_r3Why[2]++;
    else if (!g_r3HavePrev) g_r3Why[3]++; else if (cut) g_r3Why[4]++; else if (g_r3Overflow) g_r3Why[5]++;
    if (g_r3Active) {
        memcpy(g_r3StateSnap, (void*)R3_STATE_LO, R3_STATE_HI - R3_STATE_LO);
        g_r3CrFlagSnap = *(BYTE*)R3Cr(0x401220);
        DWORD gte = *(DWORD*)R3_GTE_PTR;
        __try { memcpy(g_r3GteSnap, (void*)gte, sizeof g_r3GteSnap); } __except (EXCEPTION_EXECUTE_HANDLER) { gte = 0; }
        // one slow extra frame (e.g. first frame after a room load) must not block extra frames for good:
        // the shared pacer only lowers its cost peak when it draws, so cap it and let it decay per tick here
        if (g_frameCostPeak > 8.0) g_frameCostPeak = 8.0;
        PaceExtraFrames(g_tickPeriod, R3RenderAt);
        g_frameCostPeak *= 0.97;
        if (gte) memcpy((void*)gte, g_r3GteSnap, sizeof g_r3GteSnap);
    }
    // the tick's real frame
    g_r3FxRecord = 1; g_r3FxN = 0; g_r3FloatCollected += g_r3PkN;
    double a = NowMs();
    if (g_debug >= 2 && bgCut) { g_r3InBg = 1; g_r3BgOpens = 0; g_r3BgOpenMs = g_r3BgReadMs = 0; g_r3BgBytes = 0; }
    ((void(*)())R3Cr(R3_RENDER_FN))();
    if (g_r3InBg) {
        g_r3InBg = 0;
        Log("    BG load render %.1f ms: %d file opens %.1f ms, read %lu KB in %.1f ms; %S %S", NowMs() - a, g_r3BgOpens, g_r3BgOpenMs, g_r3BgBytes / 1024, g_r3BgReadMs, g_r3BgName[0], g_r3BgName[1]);
    }
    double c = NowMs() - a;
    g_r3FxRecord = 0;
    R3FxEndTick(!cut);
    g_r3PrevRealEnd = a + c; g_r3PrevCut = cut; g_r3PrevExtras = g_r3Extras - extrasBefore;
    if (g_debug >= 2) {   // timeline around camera cuts: 4 ticks before .. 5 after
        static double lastStart; double dt = lastStart ? g_tickStart - lastStart : 0; lastStart = g_tickStart;
        R3CutTr e = { (float)dt, (float)c, g_r3PrevExtras, bgCut, memcmp(&g_r3CamCur, &g_r3CamPrev, sizeof(R3Mtx)) != 0, *(BYTE*)R3_BG_RELOAD };
        if (g_r3CtN < 16) g_r3Ct[g_r3CtN++] = e; else { memmove(g_r3Ct, g_r3Ct + 1, 15 * sizeof e); g_r3Ct[15] = e; }
        if (cut && g_r3CtAfter < 0) g_r3CtAfter = 5;
        if (g_r3CtAfter >= 0 && g_r3CtAfter-- == 0) {
            char line[1024]; int o = 0;
            for (int k = 6; k < g_r3CtN; k++) o += sprintf_s(line + o, sizeof line - o, " [dt %.1f r %.1f x%d%s%s]", g_r3Ct[k].dt, g_r3Ct[k].render, g_r3Ct[k].extras, g_r3Ct[k].bg ? " BG" : "", g_r3Ct[k].cam ? " CAM" : "");
            Log("    cut timeline:%s", line);
        }
    }
    if (g_r3Active) { if (c < 30.0) g_costPeak = c > g_costPeak ? c : g_costPeak * 0.9 + c * 0.1; NotePresent(a); }
    g_frames++;

    // this tick becomes "previous"
    R3Joint* sw = g_r3Jprev; g_r3Jprev = g_r3Jcur; g_r3Jcur = sw; g_r3JprevN = g_r3JcurN; g_r3JcurN = 0;
    g_r3HavePrev = !g_r3Overflow;
    g_r3CamPrev = g_r3CamCur;
    g_r3PartN = 0; g_r3Overflow = 0;
    R3GenericEndTick();

    if (g_debug && now - g_r3StatT >= 2000.0) {
        double secs = (now - g_r3StatT) / 1000.0;
        Log("RE3 fps %.1f  ticks/s %.2f (P %.2f ms)  cap %.0f  extras/tick %.2f  corners matched %d unmatched %d (per tick)  joints %d  guard %.2f",
            g_frames / secs, g_r3TickN / secs, g_tickPeriod, FrameIntervalMs() > 0 ? 1000.0 / FrameIntervalMs() : 0.0,
            (double)g_r3Extras / (g_r3TickN ? g_r3TickN : 1), g_r3Matched / (g_r3TickN ? g_r3TickN : 1), g_r3Unmatched / (g_r3TickN ? g_r3TickN : 1),
            g_r3JprevN, g_costPeak + 0.3);
        if (g_debug >= 2) Log("    joint path vs straight blend: corners %d, mean %.2f px, max %.2f px, >2px %d; partially mapped joint packets %d",
            g_r3DevN, g_r3DevN ? g_r3DevSum / g_r3DevN : 0.0, g_r3DevMax, g_r3DevBig, g_r3Partial);
        g_r3DevN = g_r3DevBig = g_r3Partial = 0; g_r3DevSum = g_r3DevMax = 0;
        if (g_debug >= 2) {
            char line[1024]; int o = 0;
            for (int k = 0; k < 256; k++) if (g_r3FxCnt[k]) o += sprintf_s(line + o, sizeof line - o, " %02X:%d/%d/%d", k, g_r3FxCnt[k], g_r3FxMoved[k], g_r3FxMiss[k]);
            Log("    float packets drawn %d, collected via AddPrim %d; generic rejected for moving too far %d", g_r3FloatDrawn, g_r3FloatCollected, g_r3GenFar);
            Log("    generic objects per tick (max) %d; old order-only matching would have paired different objects %d times", g_r3TagsMax, g_r3OldCross);
            g_r3FloatDrawn = g_r3FloatCollected = g_r3GenFar = 0; g_r3TagsMax = g_r3OldCross = 0;
            Log("    2D relaxed (animated) matches %d", g_r3FxRelaxed); g_r3FxRelaxed = 0;
            if (o) Log("    2D packets in extra frames (code:drawn/moved/unmatched):%s", line);
            memset(g_r3FxCnt, 0, sizeof g_r3FxCnt); memset(g_r3FxMoved, 0, sizeof g_r3FxMoved); memset(g_r3FxMiss, 0, sizeof g_r3FxMiss);
        }
        Log("    generic packets per tick: matched %d, via shared corners %d, static %d", g_r3GenMatched / (g_r3TickN ? g_r3TickN : 1), g_r3GenMapped / (g_r3TickN ? g_r3TickN : 1), g_r3GenStatic / (g_r3TickN ? g_r3TickN : 1));
        g_r3GenMatched = g_r3GenMapped = g_r3GenStatic = 0;
        Log("    extra frame cost peak %.2f ms; lost ticks %d (our render late %d, after a camera change %d)", g_frameCostPeak, g_r3Lost, g_r3LostLate, g_r3LostAfterCut);
        g_r3Lost = g_r3LostLate = g_r3LostAfterCut = 0;
        if (g_debug >= 2) {
            Log("    draw paths per tick: 438500 %.1f calls (%d objs), 438690 %.1f calls (%d objs); projections %.1f",
                (double)g_r3CntA / (g_r3TickN ? g_r3TickN : 1), g_r3ObjAN, (double)g_r3CntB / (g_r3TickN ? g_r3TickN : 1), g_r3ObjBN, (double)g_r3CntProj / (g_r3TickN ? g_r3TickN : 1));
            g_r3CntA = g_r3CntB = g_r3CntProj = 0; g_r3ObjAN = g_r3ObjBN = 0;
        }
        Log("    inactive ticks: off %d  vsync %d  noparts %d  noprev %d  camera %d  overflow %d", g_r3Why[0], g_r3Why[1], g_r3Why[2], g_r3Why[3], g_r3Why[4], g_r3Why[5]);
        memset(g_r3Why, 0, sizeof g_r3Why);
        g_frames = 0; g_r3TickN = 0; g_r3Extras = 0; g_r3Matched = g_r3Unmatched = 0; g_ticks = 0; g_r3StatT = now;
        UpdateRefresh();
    }
}

static int R3CheckCall(DWORD site, DWORD target) {
    BYTE* s = (BYTE*)R3Cr(site);
    return s[0] == 0xE8 && (DWORD)(s + 5 + *(int*)(s + 1)) == R3Cr(target);
}
static void Re3InitInterp() {
    // Classic REbirth 1.0.3 layout check: every hooked call site and the packet / z constants
    BYTE* zc = (BYTE*)R3Cr(R3_ZDIV_CODE);
    int ok = R3CheckCall(R3_TICK_CALL, R3_TICK_FN) && R3CheckCall(R3_RENDER_CALL_, R3_RENDER_FN) &&
             R3CheckCall(R3_PROJ_A_CALL, R3_PROJ_A_FN) && R3CheckCall(R3_PROJ_B_CALL, R3_PROJ_B_FN) &&
             R3CheckCall(kR3Pass2Call[0], kR3Pass2Fn[0]) && R3CheckCall(kR3Pass2Call[1], kR3Pass2Fn[1]) &&
             R3CheckCall(kR3Pass2Call[2], kR3Pass2Fn[2]) && R3CheckCall(kR3Pass2Call[3], kR3Pass2Fn[3]) &&
             *(DWORD*)R3Cr(R3_ADDPRIM_PTR) == 0x425580 &&
             zc[0] == 0xF3 && zc[1] == 0x0F && zc[2] == 0x10 && zc[3] == 0x0D && *(DWORD*)(zc + 4) == R3Cr(R3_ZDIV_OFF);
    if (!ok) { Log("RE3: Classic REbirth build not recognised (RE3 support needs Classic REbirth 1.0.3) - mod inactive"); return; }
    g_r3ZDiv = *(float*)R3Cr(R3_ZDIV_OFF);
    g_r3JointPath = GetPrivateProfileIntA("REuncap", "RE3JointPath", 1, g_iniPath);
    g_r3StateSnap = (BYTE*)malloc(R3_STATE_HI - R3_STATE_LO);
    PatchCall(R3Cr(R3_PROJ_A_CALL), (void*)R3ProjHookA);
    PatchCall(R3Cr(R3_PROJ_B_CALL), (void*)R3ProjHookB);
    for (int k = 0; k < 4; k++) PatchCall(R3Cr(kR3Pass2Call[k]), (void*)kR3Pass2Hook[k]);
    PatchCall(R3Cr(R3_RENDER_CALL_), (void*)R3Render);
    {
        DWORD* slot = (DWORD*)R3Cr(R3_ADDPRIM_PTR); DWORD old;
        g_r3AddPrim = (R3AddPrimFn)*slot;
        VirtualProtect(slot, 4, PAGE_EXECUTE_READWRITE, &old); *slot = (DWORD)R3AddPrimHook; VirtualProtect(slot, 4, old, &old);
    }
    PatchCall(R3Cr(R3_TICK_CALL), (void*)R3TickPoll);
    { int nfx = R3InstallFx(); Log("RE3 2D packet handlers hooked: %d", nfx); }
    g_r3StatT = NowMs();
    Log("RE3 interpolation active (CR 1.0.3, z divisor %.1f, FpsCap %d)", g_r3ZDiv, g_fpsCap);
}
