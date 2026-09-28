/**
 * Host-side storage for the nine guest globals that `src/Kyoto/Graphics/Carve*` reach.
 *
 * **This file is port-only**: `configure.py` does not declare it, so mwcceppc never sees it and
 * the DOL objects are byte-identical with or without it. That is the arrangement
 * `src/Kyoto/CSimplePoolPort.cpp` and `src/MetroidPrime/PortGlobals.cpp` already use, and it is
 * here because the obvious alternative is wrong.
 *
 * **Why not `#ifdef TARGET_PC` in the carve files**, which is how they arrived? Because
 * `TARGET_PC` reaches `mp_game` *only* inside the `if(MP_SDK_HEADERS_ONLY)` branch of
 * `CMakeLists.txt` (line 162), and deliberately so: the `#ifndef TARGET_PC` regions in
 * `src/MetroidPrime/main.cpp` hold bodies the port still needs, so giving `mp_game` `TARGET_PC`
 * unconditionally would delete `CMain::RsMain` from the port build. The two guards cannot both be
 * satisfied by one macro.
 *
 * The cost of getting that wrong was concrete and silent in the sense that matters: the ordinary
 * port link passed, because the headers-only configuration *does* define `TARGET_PC`, so the
 * definitions were compiled in. Only `tools/boot_probe.sh` failed, with
 *
 *     Carve802BEC1C.cpp:27: undefined reference to `lbl_80418AFF'
 *
 * and `build/gate.sh` could not see it, because the gate does not run the probe. So the file
 * looked correct to every instrument the project owns and was wrong in one configuration.
 *
 * **The lesson is the shape of the trap, not the missing symbol:** a host-only definition guarded
 * by a macro that only *some* host configurations define is correct in every configuration that
 * runs the gate and missing in the one that runs the port. A port-only file has no
 * configuration to get wrong.
 *
 * All nine are `.bss` or uninitialised `.sdata` in retail, so zero is retail's own value and the
 * bytes are not a claim about anything. If one of them turns out to be non-zero in retail, that is
 * a real finding and belongs in the carve's own header, not here.
 *
 * **The four `Mtx` objects at the bottom are the reason this file's scope grew.** `fn_802C2614`
 * is the function both `CGraphics::SetModelMatrix` and `CGraphics::SetViewPointMatrix` call to
 * push the composed matrices into the GX pipe; it reads and writes four guest matrices, and
 * with `Carve802C24AC.cpp` unlisted (it relocates against `fn_802C2614`) nothing in the port
 * needed them. Writing `fn_802C2614` is what makes those four names reachable from the port.
 */
#include "Kyoto/Graphics/CGraphics.hpp"

#include <stdio.h>

extern "C" {

/**
 * **These have initialisers on purpose.** Inside `extern "C" { }` a bare `extern uchar lbl;` is a
 * *declaration* in C++, not a tentative definition - C gives a tentative definition a common symbol,
 * C++ does not, and the reference stays undefined at link time. That is not hypothetical: it is
 * the same bug that had `extern FScriptLoader REL_loader_CannonBall;` bound as a `FUNC` in
 * `.text` earlier in this project, and the general check is `readelf -sW` - a data symbol showing
 * as `FUNC` in `.text` is it.
 */

/** `CGraphics::GetUseVideoFilter` / `SetUseVideoFilter`'s flag. `.sdata`, 1 byte. */
uchar lbl_80418AFF = 0;

/** `CGX::SetModelMatrix`'s "already set the identity once" flag. `.sdata`, 1 byte. */
uchar lbl_80418AFD = 0;

/** `CGraphics::GetProjectionState`'s return object. `.bss`. */
CGraphics::CProjectionState lbl_80416F28 = CGraphics::CProjectionState(false, 0.f, 0.f, 0.f, 0.f,
                                                                    0.f, 0.f);

/**
 * `CGX::SetModelMatrix`'s destination. `.bss`.
 *
 * **The 12-float constructor, not the old 4-`CVector3f` one.** Upstream's `CTransform4f` has no
 * four-`CVector3f` constructor; the pre-merge header's took the three basis vectors and the
 * translation and interleaved them itself (`m0(m0), posX(pos.GetX()), m1(m1), ...`). That
 * interleaving is exactly the 12-float constructor upstream kept, whose parameters are
 * `m00..m23` in declaration order - so `m0.x, m0.y, m0.z, pos.x, m1.x, ...` maps one for one onto
 * `_m00.._m23` and the member offsets are unchanged (`CTransform4f` is 12 floats, 0x30, either
 * way). Every argument here is 0, so the all-zero call is the same object both spellings build;
 * the twelve-float form is used because it is the one that exists.
 */
CTransform4f lbl_80416F74 = CTransform4f(0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f);

/** `CGraphics::SetScreenPosition`'s `GXRenderModeObj` source. `.bss`. */
GXRenderModeObj mRenderModeObj__9CGraphics = { (VITVMode)0 };

/**
 * `CGraphics::SetModelMatrix` compares against this, so it needs an address, which is why it is
 * here rather than a temporary. Retail has the object in `.bss`; see the header note above.
 */
/** Same construction as `lbl_80416F74` above: the 12-float constructor upstream has. */
CTransform4f sIdentity__12CTransform4f =
    CTransform4f(0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f);

/** `fn_802C2614`'s "the normal matrix is worth uploading" latch. `.sdata`, 1 byte. */
u8 lbl_80418AFC = 0;

/**
 * The four `Mtx` objects `fn_802C2614` and `CGraphics::SetViewPointMatrix` compose, in
 * `.bss` at retail and named by dtk's labels. **`Mtx` is `f32[3][4]` = 0x30 bytes**
 * (`extern/aurora/include/dolphin/mtx/GeoTypes.h`), which is exactly the `size:0x30` dtk gives
 * each of these four in `config/G2ME01/symbols.txt` - that agreement is the evidence they are
 * Mtx and not CTransform4f, and it is why 0x804172A0..0x804172D0 and 0x804172D0..0x80417300
 * do not overlap.
 *
 * The initialisers are explicit because a C++ `extern "C"` block has no tentative definitions
 * (see the note above this block); retail's own value here is zero, all `.bss`.
 */
Mtx lbl_804172A0 = { { 0.f } };
Mtx lbl_804172D0 = { { 0.f } };
Mtx lbl_80417300 = { { 0.f } };
Mtx lbl_80417330 = { { 0.f } };

} // extern "C"

// ---------------------------------------------------------------------------
// `CGraphics::mpSpareBuffer` and `CGraphics::mSpareBufferSize` - retail's spare texture
// scratch pair, and the two `CGraphics` static data members the 2026-09-28 merge left the
// port asking for.
//
// Retail: `mpSpareBuffer__9CGraphics = .sbss:0x804199B8, size 0x4` and
// `mSpareBufferSize__9CGraphics = .sbss:0x804199B4, size 0x4` (config/G2ME01/symbols.txt
// 21025-21026). Both are `.sbss`, so retail's own value before `CGraphics::Initialize` runs
// is **zero**, and that is what is written here - zero is not a stand-in value for these
// two, it is the value the retail binary holds.
//
// **Who asks for it.** `CPlayerGun.cpp:1150`, inside the gun's dolphin-only copy-back path:
//
//     GXCopyTex(CGraphics::GetDolphinSpareBuffer(), GX_FALSE);
//
// `GetDolphinSpareBuffer()` is inline in `include/Kyoto/Graphics/CGraphics.hpp:375` and
// returns the member, so a *read* of a static data member is what raises the reference -
// the same shape as `mViewport` below, and the same fix. On the host `GXCopyTex` is a no-op
// (src/Kyoto/Graphics/CGX.cpp) and the spare buffer is never allocated, so the read yields
// null and the copy does nothing, which is the honest host behaviour rather than a
// fabricated allocation.
//
// **The only writer is not in the port build.** `src/Kyoto/Graphics/DolphinCGraphics.cpp`
// lines 326-327 and 377-378 assign both from `sSpareAllocation`; that file is
// `configure.py` `NonMatching` and `files.cmake` does not list it (it pulls in
// `CCubeModel`, `CCubeMaterial` and `CFrameDelayedKiller`), so on the host these two stay
// at retail's `.sbss` value for the life of the process. **Delete these two definitions
// when `DolphinCGraphics.cpp` is listed**, or the link sees two definitions - the same
// arrangement and the same warning as `CModelPortStub.cpp`.
// ---------------------------------------------------------------------------
void* CGraphics::mpSpareBuffer = nullptr;
int CGraphics::mSpareBufferSize = 0;

// ---------------------------------------------------------------------------
// `CGraphics::mViewport` and `CGraphics::SetViewport` - the two `CGraphics` names the
// 82-slot `CCubeRenderer` vtable wave asked for.
//
// `src/MetaRender/Carve8026FBFC.cpp` is `CCubeRenderer::BeginScene`, vtable slot 35 and **the
// first thing the frame loop calls** (`lwz r12,148(r12)` on `gpRender`, once per frame). Its
// second and third statements are
//
//   Carve8026FBFC.cpp:91   int width  = CGraphics::GetViewport().mWidth;
//   Carve8026FBFC.cpp:92   int height = CGraphics::GetViewport().mHeight;
//   Carve8026FBFC.cpp:94   CGraphics::SetViewport(0, 0, width, height);
//
// so the two references this file closes are raised from the one vtable slot that has a real
// body. `GetViewport()` is inline in the header and returns `mViewport` by reference, which is
// where the data symbol comes from - **it is emitted by a read, and a static data member is a
// different kind of thing from the four function stubs in the rest of the vtable wave.** `nm` on
// the host object shows it as `B`/`D`, not `T`; it is the fourth time in this project that a
// *data* symbol has been reached for as though it were a function, and the fix is the same shape
// each time: find out what it is before writing anything for it.
// ---------------------------------------------------------------------------

/**
 * `CGraphics::mViewport` - retail `.data:0x803B9FE8`, `size:0x18`, and **the initial value is
 * recoverable rather than guessed**, which is the whole reason this is not written down as "some
 * plausible viewport".
 *
 * `0x18` = 24 bytes is exactly `sizeof(CViewport)` as the header declares it
 * (`int mLeft; int mTop; int mWidth; int mHeight; float mHalfWidth; float mHalfHeight;`).
 *
 * **The 24 bytes, from the retail DOL:**
 *
 * ```
 * $ python3 tools/dol_read.py 0x803B9FE8 0x18 orig/G2ME01/sys/main.dol
 * .data @ 0x803b9fe8  (file 0x3b6fe8)
 * hex : 00 00 00 00 00 00 00 00 00 00 02 80 00 00 01 e0 43 a0 00 00 43 70 00 00
 * ```
 *
 * **The `LE : [...]` line quoted here before 2026-09-27 no longer exists, because the tool was
 * wrong and has been fixed** - see `tools/dol_read.py`, which now uses one `BYTE_ORDER = ">"`
 * for the whole image instead of a per-section table. The bytes above have not changed; what
 * changed is that the tool stopped offering the other reading.
 *
 * **Read as little-endian this is nonsense** - `0x80020000` as a signed int is **-2147352576**,
 * not the -2147418112 an earlier version of this comment claimed, and neither is a width. Read
 * **big-endian**, which is what a PowerPC image's `.data` actually is, and it is
 * `{0, 0, 640, 480, 320.0f, 240.0f}`: PAL, with the two half-extents exactly half the two
 * dimensions. Two independent cross-checks say that is the right reading rather than a coincidence:
 *
 *   * **`.data`, not `.bss`.** `symbols.txt` says `.data:0x803B9FE8` and not `.bss`, so retail
 *     *has* an initialiser here and it is a fact to be read rather than a default to be assumed.
 *   * **The float bytes are retail's own `.sdata2` constants, byte for byte.** `lbl_8041DFBC` is
 *     `42 96 00 00` (75.0f) and `lbl_8041DEE4` is `3f 80 00 00` (1.0f), as
 *     `Carve8026FBFC.cpp`'s header records - the same file's constants, same big-endian order, same
 *     tool. A little-endian read of `lbl_8041DEE4` would be 0x0000803F, and nothing in this project
 *     is 4.9e-42.
 *
 * **`tools/dol_read.py` decodes `.data`/`.sdata`/`.sdata2` as little-endian and prints `BE` only for
 * `.text`.** That is a bug in the tool, not in the data - a PowerPC ELF is big-endian throughout -
 * and it is the reason this finding needed a second source. **I did not edit the tool** (out of
 * lane); it is reported for the orchestrator. Reading `.data` with it and believing the `LE` line is
 * how a viewport gets invented.
 *
 * `CGraphics::SetViewport` below is what overwrites these six words, and it is the *only* thing
 * that does: nothing in `CGraphics` writes `mViewport` in this tree, so until a lane writes
 * retail's 0x802C207C, this value is what every read of it returns, on every frame.
 */
CViewport CGraphics::mViewport = { 0, 0, 640, 480, 320.f, 240.f };

/**
 * `CGraphics::SetViewport(int left, int bottom, int width, int height)` - retail
 * `SetViewport__9CGraphicsFiiii`, `.text:0x802C207C`, `size:0xF8`.
 *
 * **A no-op here is the worst of the six, and this is why.** The other five are destructors that
 * nothing reaches during boot. This one is on the per-frame path, and it is a **mutator of
 * renderer state**: retail's body writes all six words of `mViewport` and then calls
 * `GXSetViewport`. A no-op therefore does not fail, does not fault, and does not look wrong - it
 * leaves `mViewport` reading exactly as it did, which on the first frame is *correct*, and leaves
 * the GX viewport register holding whatever Aurora's `GXInit` put there, which is not. **A caller
 * that trusted it would get a wrong projection and no indication why.**
 *
 * Retail's body, for a lane that wants to write it:
 *
 * ```
 * 802c2084:  lis  r10,17200                 <- 0x43300000, the float 1.0E9
 * 802c2088:  lis  r9,-32703  ; addi r9,r9,29284    <- r9 = 0x80417264
 * 802c20b8:  lhz  r12,6(r9)                 <- a halfword at 0x8041726A, which is in .bss
 * 802c20cc:  subf r12,r31,r12
 * 802c20d0:  lfd  f4,-16080(r2)             <- R_PPC_EMB_SDA21 lbl_8041E4F0, a .sdata2 *double*
 * 802c20f0:  lis  r11,-32708                 <- R_PPC_ADDR16_HA mViewport__9CGraphics
 * 802c2108:  stwu r3,-24600(r11)            <- so r11 = 0x80440000 - 0x6018 = 0x803B9FE8
 * 802c2144:  stw  r12,4(r11)                <- +0x04 <- r12
 * 802c214c:  stw  r5,8(r11)                 <- +0x08 <- r5   (the `width` argument)
 * 802c2150:  stw  r6,12(r11)                <- +0x0C <- r6   (the `height` argument)
 * 802c2154:  stfs f8,16(r11)                <- +0x10
 * 802c2158:  stfs f7,20(r11)                <- +0x14
 * 802c215c:  bl   8036c99c <GXSetViewport>
 * 802c2170:  blr
 * ```
 *
 * **The relocations, not the mnemonics, are what settle two things here.** `mViewport` is
 * confirmed twice over - the `lis`/`stwu` pair on `r11` carries `R_PPC_ADDR16_HA` /
 * `R_PPC_ADDR16_LO` against `mViewport__9CGraphics`, so the `-24600(r11)` is not a guess about a
 * base, it is the linker saying so. And the stores fix the field order: `r3` (the `left`
 * argument) at +0, one *computed* word at +4, then `width` and `height` at +8 and +0xC. **So the
 * header's `mTop` slot is fed a value derived from the `bottom` argument rather than the `bottom`
 * argument itself** - either the field is misnamed in this tree or retail applies an offset to it.
 * That is a claim about the header, not a correction to it, and it is left here rather than acted
 * on.
 *
 * **The two float stores are not half-extents, and that is why this is not reproduced.** The
 * operands, resolved:
 *
 *   * `f4` = `lbl_8041E4F0`, a `.sdata2` **double** (`size:0x8`) whose eight retail bytes are
 *     `43 30 00 00 80 00 00 00` = 0x4330000080000000. Not 0.5, and not a viewport quantity.
 *   * `f0` is a double assembled from two stack words: the high half is the 1.0E9f `0x43300000`
 *     and the low half is `lhz r12,6(r9) ^ 0x8000`, read from **0x8041726A, which is in `.bss`**.
 *   * The body evaluates **six** `fsubs` of that double against `f4` and `stfs`es two of the
 *     results, so the stored values are about 4.288e18 truncated to `float`. The other four
 *     `fsubs` (into `f1`..`f4`) are dead in this range, and the two `.sdata` floats loaded into
 *     `f5` and `f6` at 0x802C20F4/0x802C2104 are never used at all.
 *
 * **So `width * 0.5` would be a fabrication, and so would copying the arithmetic.** Either way the
 * two float fields of `CViewport` are not doing what their names say, which is a finding for
 * whoever writes this function and not something to paper over with a plausible value. It also
 * would not help: `SetViewport(0, 0, mWidth, mHeight)` from `BeginScene` is **idempotent against
 * the initialiser above** - 640x480 in, 640x480 out - so on the port's own path the six stores are
 * unobservable and the only real loss is the `GXSetViewport` call.
 *
 * `mpUnwritten` below logs, which is the only honest thing available: a caller that reaches this
 * every frame is told, once per frame, that the projection it is about to use is not retail's.
 */
static void mpUnwrittenSetViewport(int left, int bottom, int width, int height) {
  printf("[CGraphics] SetViewport(%d, %d, %d, %d) has no decompiled body - stand-in, retail "
         "behaviour NOT reproduced; mViewport left at retail's .data initial value and the GX "
         "viewport register NOT set, so the projection below is wrong\n",
         left, bottom, width, height);
  fflush(nullptr);
}

void CGraphics::SetViewport(int left, int bottom, int width, int height) {
  mpUnwrittenSetViewport(left, bottom, width, height);
}

