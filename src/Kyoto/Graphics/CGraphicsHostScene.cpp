/**
 * `CGraphics`' frame bracket on the host: `BeginScene`, `ClearBackAndDepthBuffers`, `EndScene`,
 * `SwapBuffers` and the two VI retrace callbacks, written from retail's bodies in
 * `build/G2ME01/asm/Kyoto/Graphics/DolphinCGraphics.s` (upstream's `DolphinCGraphics.cpp` is the
 * same code and is excluded from the port; see `CGraphicsHostGlobals.cpp` for why a port-only file).
 *
 * **Port-only**, like `CGraphicsHostGlobals.cpp`: `configure.py` does not declare it, so the DOL
 * objects are byte-identical with or without it.
 *
 * **The frame is owned here, as in the Metroid Prime port** (`MetroidPrimePort`'s
 * `DolphinCGraphics.cpp`): `BeginScene` opens an Aurora frame and `EndScene` presents it between
 * `GXFlush` and `GXEnableBreakPt`. Until 2026-09-29 nothing in this port called
 * `aurora_begin_frame`/`aurora_end_frame`, so no frame was ever presented. The bracket itself is
 * now `port::gfx::AuroraFrameBegin` / `AuroraFrameEnd` rather than file-local statics, because
 * `CGraphics::Startup` (src/Kyoto/Graphics/CGraphicsHostStartup.cpp) opens the *first* frame from
 * `CGraphicsSys`'s constructor - `GXInit` submits FIFO writes that Aurora's worker only processes
 * inside a frame - and both must agree on whether one is open.
 *
 * **The breakpoint handshake runs synchronously.** Aurora has no GX breakpoints; the platform's
 * `GXEnableBreakPt` (`platform/shims.cpp`) calls the registered `SwapBuffers` and then pulses the
 * pre/post retrace callbacks (`platform/sdk_stubs.cpp`), which is what retires
 * `mNumBreakpointsWaiting`. That is why the callbacks live here: without them the next
 * `EndScene` waits 250 ms and takes the GX-abort path every frame.
 *
 * Retail's callers reach two of these by dtk's labels (`fn_802C1E60`, `fn_802C1658`; see
 * `src/MetaRender/Carve8026FB80.cpp`), so those two are also defined under those names below.
 */
#include "Kyoto/Graphics/CGraphics.hpp"

#include "Kyoto/Basics/CStopwatch.hpp"
#include "Kyoto/CFrameDelayedKiller.hpp"
#include "Kyoto/Graphics/CGX.hpp"
#include "Kyoto/Math/CMath.hpp"

#include <aurora/aurora.h>
#include <dolphin/gx/GXShims.h>
#include <dolphin/mtx.h>
#include <dolphin/os.h>
#include <dolphin/vi.h>

#include <stdio.h>

extern "C" {

/**
 * Retail storage, named by dtk's labels because that is how carved callers will reach them.
 * All `.sbss` (zero) except `lbl_80418AFE`, which is `.sdata` and 1 in retail.
 */
uchar lbl_80419928 = 0;     // sGXAborted
int lbl_80419988 = 0;       // CGraphics::mFrameCounter
float lbl_8041998C = 0.f;   // CGraphics::mFramesPerSecond
float lbl_80419990 = 0.f;   // CGraphics::mLastFramesPerSecond
float lbl_80419994 = 0.f;   // sFrameWaitFraction: the share of the frame period left after the wait
float lbl_80419998 = 0.f;   // sPreviousFrameWaitFraction
int lbl_8041999C = 0;       // CGraphics::mNumBreakpointsWaiting
int lbl_804199A0 = 0;       // CGraphics::mFlippingState
uchar lbl_804199A5 = 0;     // CGraphics::mInterruptLastFrameUsedAbove
void* lbl_804199A8 = nullptr; // CGraphics::mpFrameBuf1
void* lbl_804199AC = nullptr; // CGraphics::mpFrameBuf2
void* lbl_804199B0 = nullptr; // CGraphics::mpCurrenFrameBuf
uchar lbl_80418AFE = 1;     // CGraphics::mFirstFrame

/** Defined elsewhere: sIs50Hz (`mainHead.cpp`), mUseVideoFilter and mIsBeginSceneClearFb and
 * the render mode (`CGraphicsHostGlobals.cpp`). */
extern bool lbl_804199CC;
extern uchar lbl_80418AFF;
extern uchar lbl_80418AE4;
extern GXRenderModeObj mRenderModeObj__9CGraphics;

} // extern "C"

namespace {

/**
 * `sFPSTimer`, retail `.sbss:0x80419930`, 8 bytes. **Host difference:** `CStopwatch`'s only
 * constructor stamps `OSGetTime()` at static init where retail's `.sbss` holds 0, so the first
 * FPS sample differs; every later one is retail's.
 */
CStopwatch sFPSTimer;

/** Retail `lbl_803AF3B0`, `.rodata`, 0x20 bytes: POS, CLR0, TEX0 direct, then the terminator. */
const GXVtxDescList skPosColorTexDirect[] = {
    {GX_VA_POS, GX_DIRECT},
    {GX_VA_CLR0, GX_DIRECT},
    {GX_VA_TEX0, GX_DIRECT},
    {GX_VA_NULL, GX_NONE},
};

/** Retail `lbl_8041E4C0`, `.sdata2`: the copy filter used when the video filter is off. */
const u8 skUnfilteredCopy[7] = {0, 0, 21, 22, 21, 0, 0};

void NotReproducedOnce(bool& said, const char* what) {
  if (!said) {
    said = true;
    printf("[CGraphics] %s - retail behaviour NOT reproduced\n", what);
    fflush(nullptr);
  }
}

} // namespace

namespace port {
namespace gfx {

/**
 * The Aurora frame bracket, in `port::gfx` rather than in an anonymous namespace so that
 * `CGraphics::Startup` (src/Kyoto/Graphics/CGraphicsHostStartup.cpp) can open the *first* frame.
 * It has to: `GXInit` inside `ConfigureVideo` submits FIFO register writes that Aurora's worker
 * thread only processes inside an open frame, and the Metroid Prime port opens one at exactly
 * this point. Before 2026-09-29 the flag was file-local and `Startup` did not exist, so
 * `BeginScene` was the first and only opener.
 */
bool sAuroraFrameOpen = false;

/** Opens an Aurora frame; only records it as open if Aurora actually began one, or a later
 * `aurora_end_frame` would desynchronise its frame-slot accounting. */
bool AuroraFrameBegin() {
  if (!sAuroraFrameOpen && aurora_begin_frame()) {
    sAuroraFrameOpen = true;
  }
  return sAuroraFrameOpen;
}

void AuroraFrameEnd() {
  if (sAuroraFrameOpen) {
    aurora_end_frame();
    sAuroraFrameOpen = false;
  }
}

} // namespace gfx
} // namespace port

using port::gfx::AuroraFrameBegin;
using port::gfx::AuroraFrameEnd;

/** Retail 0x802C1E80, 0xD0. */
void CGraphics::ClearBackAndDepthBuffers() {
  const GXRenderModeObj& rm = mRenderModeObj__9CGraphics;
  GXInvalidateTexAll();
  if (rm.field_rendering) {
    GXSetViewportJitter(0.f, 0.f, static_cast< float >(rm.fbWidth),
                        static_cast< float >(rm.xfbHeight), 0.f, 1.f, VIGetNextField());
  } else {
    GXSetViewport(0.f, 0.f, static_cast< float >(rm.fbWidth), static_cast< float >(rm.xfbHeight),
                  0.f, 1.f);
  }
  GXInvalidateVtxCache();
}

/** Retail 0x802C1E60, 0x20: a forwarder. Host: opens the Aurora frame first. */
void CGraphics::BeginScene() {
  AuroraFrameBegin();
  ClearBackAndDepthBuffers();
}

/** Retail 0x802C1E38, 0x28. */
void CGraphics::SwapBuffers() {
  GXDisableBreakPt();
  lbl_804199A0 = 1;
}

/** Retail 0x802C1CE0, 0x158. */
void CGraphics::VideoPreCallback(u32) {
  if (lbl_8041999C != 0 && lbl_804199A0 == 1) {
    if (lbl_80418AFE) {
      VISetBlack(GX_FALSE);
      lbl_80418AFE = 0;
    }
    VISetNextFrameBuffer(lbl_804199B0);
    VIFlush();
    lbl_804199B0 = lbl_804199B0 == lbl_804199A8 ? lbl_804199AC : lbl_804199A8;
    lbl_804199A0 = 2;
  }

  // Retail follows with a GP-stall watchdog: when GXGetGPStatus reports the FIFO over its
  // high-water mark and GXGetOverflowCount has not moved for 10 retraces, it aborts the frame,
  // resets both FIFOs and sets sGXAborted. Aurora has no FIFO - its GXGetGPStatus never reports
  // the high-water mark and GXGetOverflowCount/GXGetCurrentGXThread do not exist - so the watchdog
  // cannot fire here and is not written.
  static bool said = false;
  NotReproducedOnce(said, "VideoPreCallback's GP-stall watchdog (no GX FIFO on the host)");
}

/** Retail 0x802C1C14, 0xCC. */
void CGraphics::VideoPostCallback(u32) {
  if (lbl_8041999C != 0 && lbl_804199A0 == 2) {
    --lbl_8041999C;
    lbl_804199A0 = 0;
    const float elapsed = sFPSTimer.GetElapsedTime();
    lbl_80419990 = lbl_8041998C;
    lbl_8041998C = 1.f / elapsed;
    sFPSTimer.Reset();
    lbl_804199A5 = VIGetNextField() == 1;
  }
}

/** Retail 0x802C1658, 0x5BC. */
void CGraphics::EndScene() {
  const OSTime start = OSGetTime();
  if (!lbl_80419928) {
    CStopwatch waitTimer;
    while (lbl_8041999C > 0 && !lbl_80419928) {
      OSYieldThread();
      if (OSTicksToMilliseconds(OSGetTime() - start) > 250) {
        const BOOL interrupts = OSDisableInterrupts();
        GXAbortFrame();
        lbl_80419928 = 1;
        OSRestoreInterrupts(interrupts != FALSE);
      }
    }
    const float elapsedMs = static_cast< float >(waitTimer.GetElapsedMicros() / 1000);
    const float framePeriod = lbl_804199CC ? 20.f : 16.666666f;
    lbl_80419998 = lbl_80419994;
    lbl_80419994 = (framePeriod - elapsedMs) / framePeriod;
  } else {
    GXAbortFrame();
  }
  if (lbl_80419928) {
    lbl_8041999C = 0;
    lbl_80419928 = 0;
    // Retail rebuilds GX here: GXInit on the FIFO, InitGraphicsFifo, SetDefaultVtxAttrFmt,
    // CGX::ResetGXStatesFull and InitGraphicsVariables. The host has no GX FIFO to rebuild and
    // none of those four is written for it.
    static bool said = false;
    NotReproducedOnce(said, "EndScene's GX-abort reinitialisation (no GX FIFO on the host)");
  }
  ++lbl_8041999C;

  // Only GX work needs an open Aurora frame; the handshake and the per-frame bookkeeping below run
  // regardless, so a frame Aurora declined still retires its breakpoint and frees its allocations.
  //
  // The GX work needs retail's GX setup, which **is** ported as of 2026-09-29:
  // `CGraphics::Startup` (0x802C329C) -> `ConfigureFrameBuffer` -> `InitGraphicsDefaults` ->
  // `SetDefaultVtxAttrFmt` runs in `CGraphicsSys`'s constructor, which platform/main.cpp builds
  // before `InvokeCMain`, and it is `ConfigureVideo` that fills `mRenderModeObj`. So `fbWidth` is
  // non-zero from the first frame and the gate below no longer fires.
  //
  // It stays as a gate rather than being deleted, because it is the only thing that keeps a
  // *misconfigured* GX from reaching Aurora: before `Startup` existed, an unset GX_VTXFMT0 made
  // Aurora parse these 288 vertices into a desynchronised command stream and abort ("indexed XF
  // load from unmapped array 24"), and a zero `fbWidth` is the observable symptom of exactly that.
  // One `!= 0` is cheaper than finding out again.
  const bool gxConfigured = mRenderModeObj__9CGraphics.fbWidth != 0;
  if (port::gfx::sAuroraFrameOpen && !gxConfigured) {
    static bool said = false;
    NotReproducedOnce(said, "EndScene's fade quad and GXCopyDisp (mRenderModeObj.fbWidth is 0: "
                            "CGraphicsSys was not constructed, so ConfigureVideo never ran)");
  }
  if (port::gfx::sAuroraFrameOpen && gxConfigured) {
    Mtx44 projection;
    C_MTXOrtho(projection, mViewport.mHeight / 2, -mViewport.mHeight / 2, -mViewport.mWidth / 2,
               mViewport.mWidth / 2, -1.f, -10.f);
    GXSetProjection(projection, GX_ORTHOGRAPHIC);
    Mtx model;
    PSMTXIdentity(model);
    GXLoadPosMtxImm(model, GX_PNMTX0);
    CGX::SetZMode(GX_TRUE, GX_LEQUAL, GX_FALSE);
    CGX::SetVtxDescv(skPosColorTexDirect);
    CGX::Begin(GX_TRIANGLES, GX_VTXFMT0, 288);
    for (int i = 0; i < 96; ++i) {
      GXPosition3f32(0.f, 1.f, 1.f);
      GXColor1u32(0);
      GXTexCoord2f32(0.f, 0.f);
      GXPosition3f32(0.f, 0.f, 1.f);
      GXColor1u32(0);
      GXTexCoord2f32(0.f, 0.f);
      GXPosition3f32(1.f, 0.f, 1.f);
      GXColor1u32(0);
      GXTexCoord2f32(0.f, 0.f);
    }
    CGX::End();
    CGX::SetZMode(GX_TRUE, GX_LEQUAL, GX_TRUE);

    GXRenderModeObj& rm = mRenderModeObj__9CGraphics;
    const float brightness = CMath::Clamp(0.f, mBrightness, 2.f);
    const u8* filter = lbl_80418AFF ? rm.vfilter : skUnfilteredCopy;
    u8 adjustedFilter[7];
    for (int i = 0; i < 7; ++i) {
      adjustedFilter[i] = static_cast< u8 >(brightness * filter[i]);
    }
    GXSetCopyFilter(rm.aa, rm.sample_pattern, GX_TRUE, adjustedFilter);
    GXCopyDisp(lbl_804199B0, lbl_80418AE4 != 0);
    GXSetCopyFilter(rm.aa, rm.sample_pattern, lbl_80418AFF != 0, rm.vfilter);
  }
  GXSetBreakPtCallback(SwapBuffers);
  VISetPreRetraceCallback(VideoPreCallback);
  VISetPostRetraceCallback(VideoPostCallback);
  if (port::gfx::sAuroraFrameOpen) {
    GXFlush();
  }
  // Host: present before the breakpoint, whose synchronous pulse is the end of the frame.
  AuroraFrameEnd();
  void* readPtr;
  void* writePtr;
  GXGetFifoPtrs(GXGetGPFifo(), &readPtr, &writePtr);
  GXEnableBreakPt(writePtr);
  mLastFrameUsedAbove = lbl_804199A5;
  ++lbl_80419988;
  CFrameDelayedKiller::FlushAllocationsForFrame();
}

extern "C" void fn_802C1E60() { CGraphics::BeginScene(); }
extern "C" void fn_802C1658() { CGraphics::EndScene(); }
