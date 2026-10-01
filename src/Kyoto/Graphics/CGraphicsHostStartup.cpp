/**
 * `CGraphics`' bring-up on the host: `CGraphicsSys`'s constructor and destructor and retail's
 * `CGraphics::Startup` -> `ConfigureVideo` -> `InitGraphicsVariables` -> `ConfigureFrameBuffer` ->
 * `InitGraphicsDefaults` -> `SetDefaultVtxAttrFmt` chain, copied from upstream's
 * `src/Kyoto/Graphics/DolphinCGraphics.cpp` (lines 284-483 and 1738-1790, which is the Echoes
 * body and is the source for this file).
 *
 * **Port-only**, like `CGraphicsHostGlobals.cpp` and `CGraphicsHostScene.cpp`: `configure.py`
 * does not declare it, so mwcceppc never sees it and the DOL objects are byte-identical with or
 * without it. It is listed in `files.cmake`, never in `configure.py`, and it has **no**
 * `TARGET_PC` guard - see `CGraphicsHostGlobals.cpp`'s header for the measurement that makes
 * that arrangement the only safe one.
 *
 * **Upstream's file is not listed and cannot be**, for the reasons `tools/check_files_cmake.py`
 * records against it (four compile errors that are not local to it, and it drags in
 * `CCubeModel`/`CCubeMaterial`/`CFrameDelayedKiller`). So the bodies are copied here rather than
 * shared, and every departure from the copy is a *port* difference with a reason, never a
 * different algorithm.
 *
 * ## The one port-only addition, and it is load-bearing
 *
 * `Startup` opens an Aurora frame before anything else. `GXInit` (`ConfigureVideo`) and
 * `GXSetVtxAttrFmt` / `CGX::ResetGXStates` submit FIFO register writes that Aurora's worker
 * thread processes, and that thread only runs inside an open frame; without it the very first
 * `GXInit` in the process either blocks or faults. The Metroid Prime port does exactly this -
 * its `CGraphics::Startup` begins with the same call - and `CGraphicsHostScene.cpp` owns the
 * flag so that `BeginScene`/`EndScene` bracket the rest of the frame. `Startup` running before
 * `InvokeCMain` is therefore the *first* frame opener, not a second one.
 *
 * ## Where the storage lives
 *
 * The C++ static data members are defined here with retail's initial values, **except** the ones
 * a carve or a sibling port file already owns, which are reached by the `extern "C"` name dtk
 * gives them. That is the same rule the rest of the directory follows and the reason is in
 * `CGraphicsHostGlobals.cpp`'s header: `CGraphics` has no `.cpp` in the matching build, so a
 * C++-named static has nothing in the DOL to bind to and every carved caller has to use dtk's
 * spelling. Defining `CGraphics::mpFrameBuf1` *and* `lbl_804199A8` would be two objects for one
 * concept, which is worse than a dangling declaration.
 *
 * | member | retail | owned by |
 * | --- | --- | --- |
 * | `mProj` | `lbl_80416F28`, `.bss:0x80416F28` | `CGraphicsHostGlobals.cpp` |
 * | `mModelMatrix` | `lbl_80416F74`, `.bss:0x80416F74` | `CGraphicsHostGlobals.cpp`, `Carve802C24AC.cpp`, `Carve802C2614.c` |
 * | `mIsGXModelMatrixIdentity` | `lbl_80418AFD`, `.sdata` | `CGraphicsHostGlobals.cpp`, `Carve802C24AC.cpp`, `Carve802C2614.c` |
 * | `mRenderModeObj` | `mRenderModeObj__9CGraphics`, `.bss:0x80417264` | `CGraphicsHostGlobals.cpp` |
 * | `mpFrameBuf1/2`, `mpCurrenFrameBuf` | `lbl_804199A8/AC/B0` | `CGraphicsHostScene.cpp` |
 * | `mFirstFrame` | `lbl_80418AFE`, `.sdata` = 1 | `CGraphicsHostScene.cpp` |
 * | `mUseVideoFilter` | `lbl_80418AFF`, `.sdata` = 1 | `CGraphicsHostGlobals.cpp` |
 * | `sIs50Hz` | `lbl_804199CC` | `mainHead.cpp` |
 * | `mScreenStretch/X/Y` | `lbl_804199E0/E4/E8` | `PortGlobals.cpp` |
 * | `mViewport` | `mViewport__9CGraphics`, `.data:0x803B9FE8` | `CGraphicsHostGlobals.cpp` |
 * | `mpSpareBuffer`, `mSpareBufferSize` | `.sbss:0x804199B8/B4` | `CGraphicsHostGlobals.cpp` |
 * | `mViewMatrix` | `mViewMatrix__9CGraphics`, `.bss:0x80416F44` | `PortGlobals.cpp` (aliased; see step 3 there) |
 *
 * ## One call that stays a stub
 *
 * `CTevCombiners::Init` (retail `fn_802BE51C`, 0x6C bytes) has **no decompiled body in this
 * tree** (`fn_8032F6EC`, the skinned-model workspace set-up `ConfigureVideo` hands its 0x40000
 * arena slice to, used to be the second; `CGraphicsHostWorkspace.cpp` now has its body).
 * Inventing offsets or bodies for it is exactly the failure this repository's docs keep warning
 * about, so it is declared `extern "C"` and called under retail's own name; the boot probe's reach
 * stub logs it, and it is an entry on
 * `docs/research/port_link_gap_list.md`. Everything else below is retail's code.
 */
#include "Kyoto/Graphics/CGraphics.hpp"

#include "Kyoto/Basics/COsContext.hpp"
#include "Kyoto/Basics/RAssertDolphin.hpp"
#include "Kyoto/CFrameDelayedKiller.hpp"
#include "Kyoto/Graphics/CGX.hpp"
#include "Kyoto/Graphics/CGraphicsSys.hpp"
#include "Kyoto/Math/CRelAngle.hpp"
#include "Kyoto/Math/CTransform4f.hpp"

#include "dolphin/gx.h"
#include "dolphin/mtx.h"
#include "dolphin/vi.h"

#include <math.h>
#include <string.h>

// `port::gfx::AuroraFrameBegin` is defined in `CGraphicsHostScene.cpp`, which owns the flag
// `BeginScene` and `EndScene` bracket the frame with. Declared here rather than in a header
// because it is one function with one caller outside that file, and a header for it would be a
// second place to keep the Aurora frame state in sync.
namespace port {
namespace gfx {
bool AuroraFrameBegin();
} // namespace gfx
} // namespace port

extern "C" {

/** Guest storage owned elsewhere; see the table in this file's header. `lbl_8041E508` is the
 * exception that is *not* owned elsewhere: it is the `.sdata2` float 0 that
 * `Carve802C2534.cpp` (retail `SetViewPointMatrix`, 0x802C2534) reads for the transposed `Mtx`'s
 * zero column. It cannot be *claimed* in `configure.py` - claiming 0x8041E508..0x8041E50C
 * removes the name from dtk's gap object and five other retail functions then fail to link - so
 * the host gives it storage here, with the value `tools/dol_read.py 0x8041E508` reads. */
float lbl_8041E508 = 0.f;

extern CGraphics::CProjectionState lbl_80416F28;   // mProj
extern CTransform4f lbl_80416F74;                  // mModelMatrix
extern uchar lbl_80418AFD;                         // mIsGXModelMatrixIdentity
extern GXRenderModeObj mRenderModeObj__9CGraphics; // mRenderModeObj
extern void* lbl_804199A8;                         // mpFrameBuf1
extern void* lbl_804199AC;                         // mpFrameBuf2
extern void* lbl_804199B0;                         // mpCurrenFrameBuf
extern bool lbl_804199CC;                          // sIs50Hz
extern int lbl_804199E0;                           // mScreenStretch
extern int lbl_804199E4;                           // mScreenPositionX
extern int lbl_804199E8;                           // mScreenPositionY

// No decompiled body in this tree; see this file's header.
extern void fn_802BE51C();
extern void fn_8032F6EC(void* buffer, uint size);

// `CGraphics::SetModelMatrix` (Carve802C24AC.cpp) and `CGraphics::SetViewPointMatrix`
// (Carve802C2534.cpp) both end in retail's shared `SetViewMatrix` tail, which is
// `fn_802C2614` and has its own unit because the other two relocate against it.
extern void fn_802C2614();

// `CGraphics::CRenderState::Flush`, retail 0x802BE7E4, 0x4 bytes: a bare `blr`. It is its own
// 4-byte unit (`Carve802BE7E4.c`) because a unit may not claim two discontiguous ranges, and
// because the neighbouring functions on either side are not trivial.
extern void fn_802BE7E4();

} // extern "C"

namespace {

// Retail's four file-scope statics for the graphics arena, spelled as upstream's
// `DolphinCGraphics.cpp` spells them (its "guessed names for the graphics arena helpers" note is
// the honest one - the functions are unnamed in the DOL and only their behaviour is known).
//
// **The arena is `COsContext`'s `mArenaBlock`, allocated by its constructor** (0x1FE000, retail's
// own size: 2 x 0x96000 framebuffers + 0x60000 FIFO + 0x46000 spare + 0x40000 skinning
// workspace). `InitializeGraphicsArena` is therefore called with a real pointer and a real size
// here, where on the console retail's `Startup` did the same against the block its own
// `COsContext` had already taken.
int sSpareAllocationSize = 0;
void* sSpareAllocation = nullptr;
uchar* sGraphicsArena = nullptr;
int sGraphicsArenaSize = 0;
int sGraphicsArenaOffset = 0;

} // namespace

// ---------------------------------------------------------------------------
// C++ static data members, with retail's initial values.
// ---------------------------------------------------------------------------

/**
 * `mLightTypes[8]` - retail `lbl_803BA000`, `.data:0x803BA000`, `size:0x20` = 8 x 4 bytes, and
 * **all eight words are 2** (`tools/dol_read.py 0x803BA000 0x20` reads
 * `00 00 00 02` eight times). `2` is `kLT_Directional`, so this is a fact read out of the DOL
 * rather than a plausible guess, and it matches `InitGraphicsVariables`'s first loop, which
 * writes the same value back.
 */
ELightType CGraphics::mLightTypes[8] = {
    kLT_Directional, kLT_Directional, kLT_Directional, kLT_Directional,
    kLT_Directional, kLT_Directional, kLT_Directional, kLT_Directional,
};

/** `mLightActive` - retail `mLightActive__9CGraphics`, `.sbss:0x804199A6`, 1 byte, so 0. */
uchar CGraphics::mLightActive = 0;

/** `mNumLightsActive` - `.sbss`, 0. `DisableAllLights` and `SetLightState` are its only writers. */
int CGraphics::mNumLightsActive = 0;

/**
 * `mDepthFunc` - `.sdata`, and **retail's word at 0x80418AE8 is 0x00000003**
 * (`tools/dol_read.py 0x80418AE0 0x30`). `3` is `GX_LEQUAL`, i.e. `kE_LEqual`.
 */
ERglEnum CGraphics::mDepthFunc = kE_LEqual;

/** `mDepthNear` - `.sbss` in retail (`lbl_...`, 0 before `SetDepthRange`), so 0. */
float CGraphics::mDepthNear = 0.f;

/**
 * `mDepthFar` - `.sdata:0x80418AF0`, and the word there is `3f 80 00 00` = 1.0f. It is the pair
 * `SetDepthRange(0.f, 1.f)` writes from `InitGraphicsDefaults`, so 1.0f is also what the boot
 * path puts back.
 */
float CGraphics::mDepthFar = 1.f;

/** `mClearDepthValue` - `.sdata:0x80418AF4`, `size:0x4`, and the word is `00 ff ff ff`. That is
 * `GX_MAX_Z24`, which is what `SetCopyClear(color, 1.f)` computes as `1.f * GX_MAX_Z24`. */
u32 CGraphics::mClearDepthValue = 0x00FFFFFF;

/**
 * `mClearColor` - upstream spells it `CColor::Black()`, i.e. `0x000000FF`. It is spelled here as
 * the constructor's own argument for one reason: `CColor::Black()` returns a reference to a
 * `const CColor` in `CColor.cpp`, which is a *dynamically* initialised object in another
 * translation unit, and static-initialisation order across TUs is unspecified. Reading it here
 * could therefore capture a not-yet-constructed `CColor`. The value is identical and there is no
 * ordering question.
 */
CColor CGraphics::mClearColor(0x000000FF);

/** `mCullMode` - `.sbss`, 0, which is `kCM_None` (`GX_CULL_NONE`). `InitGraphicsVariables`
 * writes `kCM_None` explicitly before anything reads it. */
ERglCullMode CGraphics::mCullMode = kCM_None;

/** `mRenderState` - `.bss`, two ints, both 0 (`CRenderState`'s constructor). */
CGraphics::CRenderState CGraphics::mRenderState;

/**
 * `mPixelAspectRatio` - retail `mPixelAspectRatio__9CGraphics`, `.sdata:0x80418AF8`, and the word
 * is `3f 80 00 00` = 1.0f. `ConfigureVideo` writes 1.f into it on every video reconfigure.
 */
float CGraphics::mPixelAspectRatio = 1.f;

/** `mTexRegions[GX_MAX_TEXMAP]` and `mTexRegionsCI[GX_MAX_TEXMAP / 2]` - 8 and 4 `GXTexRegion`s.
 * `.bss` in retail, so zeroed; `Startup` hands them to `GXInitTexCacheRegion`. */
GXTexRegion CGraphics::mTexRegions[GX_MAX_TEXMAP];
GXTexRegion CGraphics::mTexRegionsCI[GX_MAX_TEXMAP / 2];

/** `mGXDefaultTexRegionCallback` - `.bss`, and `GXSetTexRegionCallback` returns null on Aurora
 * (`platform/sdk_stubs.cpp`), so what `Shutdown` restores is what `Startup` was handed. */
GXTexRegionCallback CGraphics::mGXDefaultTexRegionCallback = nullptr;

/** `mpFifo`, `mFifoSize`, `mpFifoObj` - `.sbss`, so null / 0 / null before `ConfigureVideo`. */
void* CGraphics::mpFifo = nullptr;
uint CGraphics::mFifoSize = 0;
GXFifoObj* CGraphics::mpFifoObj = nullptr;

/** `mSpareBufferTexCacheSize` - `.sbss`; `Startup` sets it to 0x10000. */
int CGraphics::mSpareBufferTexCacheSize = 0;

/** `CGraphicsSys::mGraphicsInitialized` - retail `__ct__12CGraphicsSysFRC10COsContextRC10CMemorySysb`
 * tests it, so it is real state rather than a local. `.bss`, so false. */
bool CGraphicsSys::mGraphicsInitialized = false;

// ---------------------------------------------------------------------------
// The bring-up, in retail's order.
// ---------------------------------------------------------------------------

CGraphicsSys::CGraphicsSys(const COsContext& osContext, const CMemorySys& memorySys,
                           bool progressive) {
  if (!mGraphicsInitialized) {
    mGraphicsInitialized = CGraphics::Startup(osContext, progressive);
  }
}

CGraphicsSys::~CGraphicsSys() {
  if (mGraphicsInitialized == true) {
    CGraphics::Shutdown();
    mGraphicsInitialized = false;
  }
}

void CGraphics::InitGraphicsFifo(GXFifoObj* obj, void* fifo, uint fifoSize) {
  GXFifoObj fifoObj;
  GXInitFifoBase(&fifoObj, fifo, fifoSize);
  GXSetCPUFifo(&fifoObj);
  GXSetGPFifo(&fifoObj);
  GXInitFifoLimits(obj, fifoSize - 0x4000, fifoSize - 0x10000);
  GXSetCPUFifo(obj);
  GXSetGPFifo(obj);
}

// Guessed names for the graphics arena helpers (upstream's own note).
static void InitializeGraphicsArena(void* base, int size) {
  sGraphicsArena = static_cast< uchar* >(base);
  sGraphicsArenaSize = size;
  sGraphicsArenaOffset = 0;
}

static void* AllocateGraphicsArena(int size) {
  void* result = sGraphicsArena + sGraphicsArenaOffset;
  sGraphicsArenaOffset += size;
  return result;
}

static void ResetGraphicsArena() { sGraphicsArenaOffset = 0; }

bool CGraphics::Startup(const COsContext& osContext, bool progressive) {
  // Port: an Aurora frame must be open before GXInit submits FIFO commands - see this file's
  // header. The Metroid Prime port's `CGraphics::Startup` opens one at the same point.
  port::gfx::AuroraFrameBegin();
  InitializeGraphicsArena(osContext.GetArenaBlock(), osContext.GetArenaBlockSize());
  VIInit();
  VISetBlack(TRUE);
  ConfigureVideo(true, progressive);
  CGX::ResetGXStates();
  InitGraphicsVariables();
  ConfigureFrameBuffer();
  for (int i = 0; i < ARRAY_SIZE(mTexRegions); ++i) {
    GXInitTexCacheRegion(&mTexRegions[i], false, 0x8000 * i, GX_TEXCACHE_32K, 0x80000 + 0x8000 * i,
                         GX_TEXCACHE_32K);
  }
  for (int i = 0; i < ARRAY_SIZE(mTexRegionsCI); ++i) {
    GXInitTexCacheRegion(&mTexRegionsCI[i], false, (8 + 2 * i) << 15, GX_TEXCACHE_32K,
                         (9 + 2 * i) << 15, GX_TEXCACHE_32K);
  }
  mGXDefaultTexRegionCallback = GXSetTexRegionCallback(TexRegionCallback);
  mSpareBufferSize = sSpareAllocationSize;
  mpSpareBuffer = sSpareAllocation;
  mSpareBufferTexCacheSize = 0x10000;
  return true;
}

// Guessed name (upstream's own note).
static void SetProgressiveFilter(GXRenderModeObj& mode) {
  const u8 filter[7] = {4, 4, 16, 16, 16, 4, 4};
  memcpy(mode.vfilter, filter, sizeof(filter));
}

/**
 * `ConfigureVideo`. Retail's body, copied.
 *
 * **One host gap inside it, and it is a real missing symbol rather than a stubbed one.**
 * `GXNtsc480Prog` is declared in Aurora's `dolphin/gx/GXFrameBuffer.h` and **never defined** -
 * `extern/aurora/lib/dolphin/gx/GXFrameBuffer.cpp` defines `GXNtsc480IntDf`, `GXNtsc480Int`,
 * `GXPal528IntDf` and `GXMpal480IntDf`, and no progressive template. Aurora has no 480p-progressive
 * VI mode at all, so the `progressive` branch of the switch below cannot be given a target without
 * inventing a `GXRenderModeObj`, which would be fabricating console data rather than porting it.
 * `platform/main.cpp` passes `false` for the same reason retail's own PAL build does, so the
 * branch is dead on the host and the reference is a link-time cost, not a behaviour gap.
 * `tools/link_gap.py` files it under "aurora header only" rather than `MISSING`, which is exactly
 * the classification trap `docs/PROCESS_LESSONS.md` warns about: a declaration in a header is not
 * a definition. The boot probe stubs it (`PortReachStubs.cpp`'s `reachdata_471`).
 */
void CGraphics::ConfigureVideo(bool initial, bool progressive) {
  if (!initial) {
    CFrameDelayedKiller::StallAndFlushAllAllocations();
  }
  GXRenderModeObj* mode = nullptr;
  switch (VIGetTvFormat()) {
  case VI_NTSC:
    mode = progressive ? &GXNtsc480Prog : &GXNtsc480IntDf;
    break;
  case VI_MPAL:
    mode = progressive ? &GXNtsc480Prog : &GXMpal480IntDf;
    break;
  case VI_PAL:
  case VI_EURGB60:
    rs_debugger_printf("PAL TV Format not compatible with NTSC build");
    break;
  }
  GXAdjustForOverscan(mode, &mRenderModeObj__9CGraphics, 0, 16);
  mRenderModeObj__9CGraphics.viWidth += 20;
  mRenderModeObj__9CGraphics.viXOrigin -= 10;
  mPixelAspectRatio = 1.f;
  lbl_804199CC = false;
  if (progressive) {
    SetProgressiveFilter(mRenderModeObj__9CGraphics);
  }
  lbl_804199A8 = nullptr;
  lbl_804199AC = nullptr;
  mpFifo = nullptr;
  sSpareAllocation = nullptr;
  fn_8032F6EC(nullptr, 0);
  ResetGraphicsArena();
  const int frameBufferSize = ((mRenderModeObj__9CGraphics.fbWidth + 15) & ~15) *
                              mRenderModeObj__9CGraphics.xfbHeight * 2;
  lbl_804199A8 = AllocateGraphicsArena(frameBufferSize);
  lbl_804199AC = AllocateGraphicsArena(frameBufferSize);
  mFifoSize = 0x60000;
  sSpareAllocationSize = 0x46000;
  mpFifo = AllocateGraphicsArena(mFifoSize);
  sSpareAllocation = AllocateGraphicsArena(sSpareAllocationSize);
  fn_8032F6EC(AllocateGraphicsArena(0x40000), 0x40000);
  mSpareBufferSize = sSpareAllocationSize;
  mpSpareBuffer = sSpareAllocation;
  if (!initial) {
    mRenderModeObj__9CGraphics.viWidth += lbl_804199E0 * 2;
    mRenderModeObj__9CGraphics.viXOrigin += lbl_804199E4 - lbl_804199E0;
    mRenderModeObj__9CGraphics.viYOrigin += lbl_804199E8;
    VIWaitForRetrace();
    VIWaitForRetrace();
  }
  VIConfigure(&mRenderModeObj__9CGraphics);
  VIFlush();
  if (initial) {
    mpFifoObj = GXInit(mpFifo, mFifoSize);
  }
  InitGraphicsFifo(mpFifoObj, mpFifo, mFifoSize);
  GXSetCopyFilter(mRenderModeObj__9CGraphics.aa, mRenderModeObj__9CGraphics.sample_pattern, GX_TRUE,
                  mRenderModeObj__9CGraphics.vfilter);
}

GXTexRegion* CGraphics::TexRegionCallback(const GXTexObj* obj, GXTexMapID id) {
  static uchar nextTexRgn = 0;
  static uchar nextTexRgnCI = 0;
  // Port: Aurora's `GXGetTexObjFmt` takes a non-const `GXTexObj*`; the game's own
  // `include/dolphin/gx/GXGet.h` declares the const form. Retail's signature is the const one
  // (`GXTexRegionCallback` is `GXTexRegion* (*)(const GXTexObj*, GXTexMapID)`), so the
  // const_cast is a shim to Aurora's declaration and not a change of behaviour.
  const GXTexFmt fmt = GXGetTexObjFmt(const_cast< GXTexObj* >(obj));
  const bool indexed = fmt == GX_TF_C4 || fmt == GX_TF_C8 || fmt == GX_TF_C14X2;
  if (id == GX_TEXMAP7) {
    return indexed ? &mTexRegionsCI[0] : &mTexRegions[0];
  }
  if (indexed) {
    do {
      nextTexRgnCI = (nextTexRgnCI + 1) & 3;
    } while (nextTexRgnCI == 0);
    return &mTexRegionsCI[nextTexRgnCI];
  }
  do {
    nextTexRgn = (nextTexRgn + 1) & 7;
  } while (nextTexRgn == 0);
  return &mTexRegions[nextTexRgn];
}

void CGraphics::InitGraphicsVariables() {
  for (int i = 0; i < ARRAY_SIZE(mLightTypes); ++i) {
    mLightTypes[i] = kLT_Directional;
  }
  mLightActive = 0;
  SetDepthWriteMode(false, mDepthFunc, false);
  SetCullMode(kCM_None);
  SetAmbientColor(CColor(0.2f, 0.2f, 0.2f, 1.f));
  lbl_80418AFD = 0;
  SetIdentityViewPointMatrix();
  SetIdentityModelMatrix();
  SetViewport(0, 0, mViewport.mWidth, mViewport.mHeight);
  SetPerspective(60.f, static_cast< float >(mViewport.mWidth) / static_cast< float >(mViewport.mHeight),
                 lbl_80416F28.GetNear(), lbl_80416F28.GetFar());
  SetCopyClear(mClearColor, 1.f);
  const GXColor white = {0xFF, 0xFF, 0xFF, 0xFF};
  CGX::SetChanMatColor(CGX::Channel0, white);
  mRenderState.ResetFlushAll();
}

void CGraphics::Shutdown() {
  GXSetTexRegionCallback(mGXDefaultTexRegionCallback);
  CFrameDelayedKiller::StallAndFlushAllAllocations();
  lbl_804199A8 = nullptr;
  lbl_804199AC = nullptr;
  mpFifo = nullptr;
  sSpareAllocation = nullptr;
  fn_8032F6EC(nullptr, 0);
  ResetGraphicsArena();
}

void CGraphics::InitGraphicsDefaults() {
  SetDepthRange(0.f, 1.f);
  lbl_80418AFD = 0;
  SetModelMatrix(lbl_80416F74);
  SetViewPointMatrix(mViewMatrix);
  SetDepthWriteMode(false, mDepthFunc, false);
  SetCullMode(mCullMode);
  SetViewport(mViewport.mLeft, mViewport.mTop, mViewport.mWidth, mViewport.mHeight);
  FlushProjection();
  // `CTevCombiners::Init` is retail's `fn_802BE51C`, 0x6C bytes, with no decompiled body here.
  fn_802BE51C();
  DisableAllLights();
  SetDefaultVtxAttrFmt();
}

void CGraphics::ConfigureFrameBuffer() {
  VIConfigure(&mRenderModeObj__9CGraphics);
  VISetNextFrameBuffer(lbl_804199A8);
  lbl_804199B0 = lbl_804199AC;
  GXSetViewport(0.f, 0.f, static_cast< float >(mRenderModeObj__9CGraphics.fbWidth),
                static_cast< float >(mRenderModeObj__9CGraphics.efbHeight), 0.f, 1.f);
  GXSetScissor(0, 0, mRenderModeObj__9CGraphics.fbWidth, mRenderModeObj__9CGraphics.efbHeight);
  GXSetDispCopySrc(0, 0, mRenderModeObj__9CGraphics.fbWidth, mRenderModeObj__9CGraphics.efbHeight);
  GXSetDispCopyDst(mRenderModeObj__9CGraphics.fbWidth, mRenderModeObj__9CGraphics.efbHeight);
  GXSetDispCopyYScale(static_cast< float >(mRenderModeObj__9CGraphics.xfbHeight) /
                      static_cast< float >(mRenderModeObj__9CGraphics.efbHeight));
  GXSetCopyFilter(mRenderModeObj__9CGraphics.aa, mRenderModeObj__9CGraphics.sample_pattern, GX_ENABLE,
                  mRenderModeObj__9CGraphics.vfilter);
  GXSetPixelFmt(mRenderModeObj__9CGraphics.aa ? GX_PF_RGB565_Z16 : GX_PF_RGB8_Z24, GX_ZC_LINEAR);
  GXSetDispCopyGamma(GX_GM_1_0);
  GXCopyDisp(lbl_804199B0, true);
  VIFlush();
  VIWaitForRetrace();
  VIWaitForRetrace();
  mViewport.mWidth = mRenderModeObj__9CGraphics.fbWidth;
  mViewport.mHeight = mRenderModeObj__9CGraphics.efbHeight;
  InitGraphicsDefaults();
}

void CGraphics::SetDefaultVtxAttrFmt() {
  GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
  GXSetVtxAttrFmt(GX_VTXFMT1, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
  GXSetVtxAttrFmt(GX_VTXFMT2, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
  GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_NRM, GX_NRM_XYZ, GX_F32, 0);
  GXSetVtxAttrFmt(GX_VTXFMT1, GX_VA_NRM, GX_NRM_XYZ, GX_S16, 14);
  GXSetVtxAttrFmt(GX_VTXFMT2, GX_VA_NRM, GX_NRM_XYZ, GX_S16, 14);
  GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);
  GXSetVtxAttrFmt(GX_VTXFMT1, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);
  GXSetVtxAttrFmt(GX_VTXFMT2, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);
  GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);
  GXSetVtxAttrFmt(GX_VTXFMT1, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);
  GXSetVtxAttrFmt(GX_VTXFMT2, GX_VA_TEX0, GX_TEX_ST, GX_U16, 15);
  for (int i = 1; i <= 7; ++i) {
    GXAttr attr = static_cast< GXAttr >(GX_VA_TEX0 + i);
    GXSetVtxAttrFmt(GX_VTXFMT0, attr, GX_TEX_ST, GX_F32, 0);
    GXSetVtxAttrFmt(GX_VTXFMT1, attr, GX_TEX_ST, GX_F32, 0);
    GXSetVtxAttrFmt(GX_VTXFMT2, attr, GX_TEX_ST, GX_F32, 0);
  }
}

void CGraphics::DisableAllLights() {
  mNumLightsActive = 0;
  mLightActive = 0;
  CGX::SetChanCtrl(CGX::Channel0, false, GX_SRC_REG, GX_SRC_REG, GX_LIGHT_NULL, GX_DF_NONE,
                   GX_AF_NONE);
  CGX::SetChanCtrl(CGX::Channel1, false, GX_SRC_REG, GX_SRC_REG, GX_LIGHT_NULL, GX_DF_NONE,
                   GX_AF_NONE);
}

void CGraphics::SetDepthWriteMode(const bool test, ERglEnum comp, const bool write) {
  mDepthFunc = comp;
  CGX::SetZMode(test, static_cast< GXCompare >(comp), write);
}

void CGraphics::SetCullMode(ERglCullMode cullMode) {
  mCullMode = cullMode;
  GXSetCullMode(static_cast< GXCullMode >(cullMode));
}

void CGraphics::SetDepthRange(float near, float far) {
  mDepthNear = near;
  mDepthFar = far;
  GXSetViewport(static_cast< float >(mViewport.mLeft), static_cast< float >(mViewport.mTop),
                static_cast< float >(mViewport.mWidth), static_cast< float >(mViewport.mHeight),
                mDepthNear, mDepthFar);
}

void CGraphics::FlushProjection() {
  float right = lbl_80416F28.GetRight();
  float left = lbl_80416F28.GetLeft();
  float top = lbl_80416F28.GetTop();
  float bottom = lbl_80416F28.GetBottom();
  float near = lbl_80416F28.GetNear();
  float far = lbl_80416F28.GetFar();
  if (lbl_80416F28.IsPerspective()) {
    Mtx44 mtx;
    MTXFrustum(mtx, top, bottom, left, right, near, far);
    GXSetProjection(mtx, GX_PERSPECTIVE);
  } else {
    Mtx44 mtx;
    MTXOrtho(mtx, top, bottom, left, right, near, far);
    GXSetProjection(mtx, GX_ORTHOGRAPHIC);
  }
}

void CGraphics::SetAmbientColor(const CColor& color) {
  CGX::SetChanAmbColor(CGX::Channel0, color.GetGXColor());
  CGX::SetChanAmbColor(CGX::Channel1, color.GetGXColor());
}

void CGraphics::SetCopyClear(const CColor& color, float depth) {
  mClearColor = color;
  mClearDepthValue = static_cast< u32 >(depth * GX_MAX_Z24);
  GXSetCopyClear(color.GetGXColor(), mClearDepthValue);
}

void CGraphics::SetPerspective(float fovy, float aspect, float znear, float zfar) {
  float t = tan(CRelAngle::FromDegrees(fovy).AsRadians() / 2.f);
  lbl_80416F28 = CProjectionState(true,                                 // Is Projection
                                 -(aspect * 2.f * znear * t * 0.5f),   // Left
                                 (aspect * 2.f * znear * t * 0.5f),    // Right
                                 (znear * 2.f * t * 0.5f),             // Top
                                 -(znear * 2.f * t * 0.5f),            // Bottom
                                 znear, zfar);
  FlushProjection();
}

/**
 * `SetIdentityModelMatrix`, retail's shape. Upstream's body is
 * `mModelMatrix = CTransform4f::Identity(); mIsGXModelMatrixIdentity = true; SetViewMatrix();`
 * inside the `if`, where `SetViewMatrix` is the composed-matrix push. **On this tree that tail is
 * `fn_802C2614`**, the one both `SetModelMatrix` and `SetViewPointMatrix` call: it is the unit
 * `Carve802C2614.c` reconstructs from retail's bytes, it reads and writes the same four guest
 * `Mtx` objects, and it honours the same `lbl_80418AFD` identity latch - so calling it *is*
 * retail's `SetViewMatrix` here, and calling a `CGraphics::SetViewMatrix` that this port does not
 * define would be calling nothing.
 */
void CGraphics::SetIdentityModelMatrix() {
  if (!lbl_80418AFD) {
    lbl_80416F74 = CTransform4f::Identity();
    lbl_80418AFD = 1;
    fn_802C2614();
  }
}

/**
 * `SetIdentityViewPointMatrix`. Upstream's whole body is
 * `SetViewPointMatrix(CTransform4f::Identity())`, and retail's `SetViewPointMatrix` is
 * `Carve802C2534.cpp` (0x802C2534, `NonMatching` at 99.11% - see that file's header for the ten
 * float-register bytes), which is a unit `configure.py` claims and this port lists.
 */
void CGraphics::SetIdentityViewPointMatrix() { SetViewPointMatrix(CTransform4f::Identity()); }

// ---------------------------------------------------------------------------
// `CRenderState`, retail 0x802BE7E8 onwards. `ResetFlushAll`'s last statement is retail's
// `CRenderState::Flush`, which `Carve802BE7E4.c` holds as the 4-byte `blr` at 0x802BE7E4.
// ---------------------------------------------------------------------------

CGraphics::CRenderState::CRenderState() {
  x0_ = 0;
  x4_ = 0;
}

int CGraphics::CRenderState::SetVtxState(const float* pos, const float* nrm, const uint* clr) {
  CGX::SetArray(GX_VA_POS, pos, 12);
  CGX::SetArray(GX_VA_NRM, nrm, 12);
  CGX::SetArray(GX_VA_CLR0, clr, 4);
  int result = 1;
  if (nrm != nullptr) {
    result |= 2;
  }
  if (clr != nullptr) {
    result |= 16;
  }
  return result;
}

void CGraphics::CRenderState::ResetFlushAll() {
  x0_ = 0;
  SetVtxState(nullptr, nullptr, nullptr);
  for (int i = 0; i < 8; i++) {
    CGX::SetArray(static_cast< GXAttr >(GX_VA_TEX0 + i), nullptr, 8);
  }
  fn_802BE7E4();
}

// ---------------------------------------------------------------------------
// The per-frame state setters `CCubeRenderer::BeginScene`/`EndScene` call, and the address names
// the carves call them by.
//
// `SetClearColor`, `SetBlendMode` and `TickRenderTimings` are upstream's bodies
// (`DolphinCGraphics.cpp:748`, `:922`, `:1511`), like the rest of this file. `TickRenderTimings`
// writes the `lbl_` spellings of `mRenderTimings`/`mSecondsMod900` for the reason
// `PortGlobals.cpp` gives beside `lbl_804199D8`, which `GetSecondsMod900` reads. The modulus is
// 54000: retail's `lis r3,1 ; subi r3,r3,0x2d10` at 0x802BF64C/5C is 0x10000 - 0x2D10 = 0xD2F0.
//
// The `fn_` names are the same functions: `symbols.txt` gives 0x802C1F5C, 0x802C1608, 0x802C162C,
// 0x802C15E8, 0x802C235C and 0x802BF640 these six `CGraphics` names, but the `Carve8026*.cpp`
// units were written against the address names and are `Matching` that way. On the host the two
// spellings are different symbols, so until these forwarders existed every frame called six
// stubs that only printed their names while the bodies above went uncalled.
// `Carve8026E7F0.cpp` calls `fn_802C162C` with two arguments, as retail does there; its `write`
// is whatever the third argument register holds, on the host as on the GameCube.
// ---------------------------------------------------------------------------

extern "C" uint lbl_804199D4 = 0;
extern "C" float lbl_804199D8;

void CGraphics::SetClearColor(const CColor& color) {
  mClearColor = color;
  GXSetCopyClear(mClearColor.GetGXColor(), mClearDepthValue);
}

void CGraphics::SetBlendMode(ERglBlendMode mode, ERglBlendFactor src, ERglBlendFactor dst,
                             ERglLogicOp op) {
  CGX::SetBlendMode(static_cast< GXBlendMode >(mode), static_cast< GXBlendFactor >(src),
                    static_cast< GXBlendFactor >(dst), static_cast< GXLogicOp >(op));
}

void CGraphics::TickRenderTimings() {
  lbl_804199D4 = (lbl_804199D4 + 1) % (900 * 60);
  lbl_804199D8 = static_cast< float >(lbl_804199D4) / 60.f;
}

extern "C" {
void fn_802C1F5C(const CColor& color) { CGraphics::SetClearColor(color); }
void fn_802C1608(GXCullMode mode) { CGraphics::SetCullMode(static_cast< ERglCullMode >(mode)); }
void fn_802C162C(bool test, GXCompare comp, bool write) {
  CGraphics::SetDepthWriteMode(test, static_cast< ERglEnum >(comp), write);
}
void fn_802C15E8(GXBlendMode mode, GXBlendFactor src, GXBlendFactor dst, GXLogicOp op) {
  CGraphics::SetBlendMode(static_cast< ERglBlendMode >(mode), static_cast< ERglBlendFactor >(src),
                          static_cast< ERglBlendFactor >(dst), static_cast< ERglLogicOp >(op));
}
void fn_802C235C(float fovy, float aspect, float znear, float zfar) {
  CGraphics::SetPerspective(fovy, aspect, znear, zfar);
}
void fn_802BF640() { CGraphics::TickRenderTimings(); }
}
