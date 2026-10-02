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
 * `lbl_80418AE4` (below, retail value 1) was added later and is the exception, and so are
 * `lbl_80418AFF` and `lbl_80418AFD`, which were zero here until 2026-09-29 and are **1** in
 * retail: `tools/dol_read.py 0x80418AE0 0x30` reads `01 01 01 01` at 0x80418AFC..0x80418AFF, so
 * all four of that `.sdata` run's bytes are 1. `lbl_80418B08` (below) is the eleventh and the
 * first *pointer* rather than a value - the word at 0x80418B08 is a pool address the carve
 * dereferences - so it is a pointer plus the array it points at; its own note records what is and
 * is not measured about it. The rest are `.bss` or uninitialised `.sdata` in retail, so zero is
 * retail's own value and the bytes are not a claim about anything.
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

/**
 * `CGraphics::GetUseVideoFilter` / `SetUseVideoFilter`'s flag, and `EndScene`'s "use the render
 * mode's own vfilter" choice. `.sdata:0x80418AFF`, 1 byte, and **retail initialises it to 1** -
 * `tools/dol_read.py 0x80418AE0 0x30` reads `01 01 01 01` across 0x80418AFC..0x80418AFF. It was 0
 * here until 2026-09-29, so `EndScene` was taking the unfiltered copy-filter path against
 * `skUnfilteredCopy` instead of the video filter.
 */
uchar lbl_80418AFF = 1;

/**
 * `CGX::SetModelMatrix`'s "already set the identity once" flag, i.e. `CGraphics::mIsGXModelMatrixIdentity`.
 * `.sdata:0x80418AFD`, 1 byte, and **retail initialises it to 1** (same `.sdata` run as above).
 * It was 0 here until 2026-09-29, so `fn_802C2614` took its `PSMTXConcat` branch on the first
 * frame instead of the `PSMTXCopy` one that retail's initial state selects.
 */
uchar lbl_80418AFD = 1;

/**
 * `CGraphics::mIsBeginSceneClearFb`: `SetIsBeginSceneClearFb` writes it, `CCubeRenderer` reads it.
 * `.sdata`, 1 byte, and **retail initialises it to 1** (`auto_09_80418AD4_sdata.s`), the one flag
 * here that is not zero. Until 2026-09-29 it was a zero-filled reach-data stub, so the renderer
 * read the opposite of retail.
 */
uchar lbl_80418AE4 = 1;

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

/**
 * `CTexture::sLoadedTextures`, the guest global `Carve802C4248.cpp` reaches as `lbl_80418B08`. It
 * is a **pointer variable, not an array**, and getting that wrong is a null dereference rather
 * than a wrong answer: the carve declares `extern "C" uint*` and writes `lbl_80418B08[id] = 0`, so
 * the word *at* 0x80418B08 is read as an address. Retail's is a pool pointer -
 * `tools/dol_read.py 0x80418AE0 0x30` reads `803d fbb8` at 0x80418B08 and `0000 0000` at
 * 0x80418B0C, which is the array's first two words, not its extent.
 *
 * What is measured: `Carve802C4248.cpp`'s header has retail's four instructions -
 * `lwz r4,-29304(r13)` / `slwi r0,r3,2` / `li r3,0` / `stwx r3,r4,r0` - so the base is loaded
 * *indirectly* and the object is a word array indexed by `GXTexMapID` whose only writer stores
 * zero. `CTexture::InvalidateTexmaps` (`DolphinCTexture.cpp:555`) indexes it the same way and
 * compares each word against `reinterpret_cast<uint>(this) + mClampMode`. Both loops are bounded
 * by `GX_MAX_TEXMAP` (8), so eight words is what the *code* asks for.
 *
 * What is not measured, recorded rather than resolved by assertion: the array's **address** is
 * retail's, 0x803DFBB8, a console pool address that means nothing on the host, and no body in this
 * tree establishes its extent beyond those two loops. The host therefore gives it eight zeroed
 * words - which is also the only value that cannot be a live `CTexture* + mClampMode`, so
 * `InvalidateTexmaps`'s comparison is meaningful - and does not reproduce 0x803DFBB8. A pool
 * sentinel on a host where nothing allocates at pool addresses would only ever produce false
 * negatives.
 *
 * The array is a file static rather than an exported object because retail's is at a pool address
 * no DOL symbol names, so there is no second name for it to be reached by; only the pointer needs
 * one. `lbl_80418B08` is a dynamic initialiser, but the initialiser is a link-time address
 * constant, so it is resolved before any other static initialiser runs and there is no
 * initialisation-order question.
 */
static uint sLoadedTextures[GX_MAX_TEXMAP];
uint* lbl_80418B08 = sLoadedTextures;

/**
 * `fn_802C2614`'s "the normal matrix is worth uploading" latch. `.sdata:0x80418AFC`, 1 byte, and
 * **retail initialises it to 1** - `tools/dol_read.py 0x80418AE0 0x30` reads `01 01 01 01` across
 * 0x80418AFC..0x80418AFF, so `PSMTXInvXpose` + `GXLoadNrmMtxImm` run from the first frame, as in
 * retail. It was 0 here until 2026-09-29 and the normal matrix was never uploaded at all.
 */
u8 lbl_80418AFC = 1;

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
 *
 * `lbl_80417330`, the fourth of the four, is **not** defined here: it is `CGraphics::mCameraMtx`
 * and is declared by alias below, next to that member, so the two names are one object.
 */
Mtx lbl_804172A0 = { { 0.f } };
Mtx lbl_804172D0 = { { 0.f } };
Mtx lbl_80417300 = { { 0.f } };

} // extern "C"

// ---------------------------------------------------------------------------
// `CGraphics::mCameraMtx` - the view-projection matrix `PSMTXConcat`'s first argument in
// `CModelData::SetupWorldSpacePortalPlane` (retail 0x800E4AA0 passes `0x80417330`, which
// `config/G2ME01/symbols.txt` names `mCameraMtx__9CGraphics` at `.bss:0x80417330`).
//
// Retail's own value before `CGraphics::SetViewPointMatrix` has run is zero, all `.bss`, so the
// zero initialiser below is retail's value and not a stand-in.
//
// `lbl_80417330` is the same object under the name retail's `SetViewPointMatrix`
// (`src/Kyoto/Graphics/Carve802C2534.cpp`:119) and `fn_802C2614`
// (`src/Kyoto/Graphics/Carve802C2614.c`:106,109) reach it by. Without this alias the port would
// have *two* camera matrices: the C++ member every caller of the inline `GetCameraMtx()` reads -
// `CModelData::SetupWorldSpacePortalPlane` (`src/MetroidPrime/CModelData.cpp`:716) among them -
// and the one the carves write, so that reader would be handed a permanently-zero matrix nothing
// ever writes. An alias, not a second definition, is what makes them one, and it is the same fix
// `src/MetroidPrime/PortGlobals.cpp` applies to `mViewMatrix__9CGraphics` at 0x80416F44.
//
// A GCC alias attribute is the mechanism because both spellings have to name one object and a
// second `extern "C"` definition would be a duplicate; `alias` on a C++ object with C linkage
// gives `lbl_80417330` the same address as `_ZN9CGraphics10mCameraMtxE`, which is what
// `config/G2ME01/symbols.txt` records for retail (`.bss:0x80417330`, `size:0x30`). Verified with
// `nm` on the port object.
// ---------------------------------------------------------------------------
Mtx CGraphics::mCameraMtx = { { 0.f } };
extern "C" Mtx lbl_80417330 __attribute__((alias("_ZN9CGraphics10mCameraMtxE")));

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
// **The only writer is now in the port build.** `src/Kyoto/Graphics/DolphinCGraphics.cpp`
// lines 326-327 and 377-378 assign both from `sSpareAllocation`; that file is
// `configure.py` `NonMatching` and `files.cmake` still does not list it (it pulls in
// `CCubeModel`, `CCubeMaterial` and `CFrameDelayedKiller`, and has four compile errors that
// are not local to it). **Its copy of `Startup`/`ConfigureVideo` is port-only, in
// `src/Kyoto/Graphics/CGraphicsHostStartup.cpp`** (2026-09-29, listed), and it writes both of
// these on the host: `ConfigureVideo` takes a 0x46000 slice out of the graphics arena into
// `sSpareAllocation` and `Startup` copies it into `mSpareBufferSize` / `mpSpareBuffer`. So the
// "never allocated" note above is superseded for the spare buffer: it is now a real pointer
// into `COsContext`'s arena block, and `CPlayerGun.cpp:1150`'s `GXCopyTex` (a no-op on the
// host) reads a real address rather than null.
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
 * **The two float stores ARE the half-extents. The analysis that said otherwise is superseded,
 * and it is wrong for two reasons that are both checkable.**
 *
 * What it got right: `mViewport` is at 0x803B9FE8, the field order is `left` / computed / `width`
 * / `height`, and the body ends in `GXSetViewport`.
 *
 * What it got wrong, and it is the whole reason the body was not written:
 *
 *   * **`lhz 6(r9)` is `mRenderModeObj.efbHeight`, not an anonymous `.bss` word.** `r9` is
 *     `mRenderModeObj__9CGraphics` - 0x80417264, `size:0x3C`, settled by the `lis r9,-32703` /
 *     `addi r9,r9,29284` pair at 0x802C2088/0x802C208C and by `Carve802BEC24.cpp`'s relocations
 *     against the same name - and +6 is `efbHeight` in `GXRenderModeObj`. It is a `.bss` *field* of
 *     a named object, which is exactly why reading it as an unrelated global produced nonsense.
 *   * **`lbl_8041E4F0` is the int-to-float magic double, not a viewport constant.** Its eight retail
 *     bytes are `43 30 00 00 80 00 00 00` = `0x4330000080000000`, and the bias an int is XORed with
 *     before it can be reinterpreted as a `float` is `2^55 + 2^23` = `0x4330000080000000`. So the
 *     `fsub` against it is an ordinary `static_cast<float>(int)` whose result MWCC kept in an `f`
 *     register, and the "six `fsubs` ... the other four are dead" reading was arithmetic on a
 *     register that had already been overwritten.
 *   * Consequently the two stored floats are `(float)(width / 2)` and `(float)(height / 2)` -
 *     `CViewport::mHalfWidth` and `mHalfHeight` under the names the header gives them, and the
 *     values retail's own `.data` initialiser holds for 640x480 (320.0f, 240.0f).
 *
 * The body below is therefore upstream's, `src/Kyoto/Graphics/DolphinCGraphics.cpp:717`, with
 * `mRenderModeObj` spelled `mRenderModeObj__9CGraphics` (the object this port owns) and
 * `mDepthNear` / `mDepthFar` the two `CGraphics` statics that
 * `src/Kyoto/Graphics/CGraphicsHostStartup.cpp` now defines.
 */
void CGraphics::SetViewport(int left, int bottom, int width, int height) {
  mViewport.mLeft = left;
  mViewport.mTop = mRenderModeObj__9CGraphics.efbHeight - (bottom + height);
  mViewport.mWidth = width;
  mViewport.mHeight = height;
  mViewport.mHalfWidth = static_cast< float >(width / 2);
  mViewport.mHalfHeight = static_cast< float >(height / 2);
  GXSetViewport(static_cast< float >(mViewport.mLeft), static_cast< float >(mViewport.mTop),
                static_cast< float >(mViewport.mWidth), static_cast< float >(mViewport.mHeight),
                mDepthNear, mDepthFar);
}

/** Retail 0x802BE8E8, 8 bytes: `stb r3, lbl_80418AE4@sda21(r0); blr`. */
void CGraphics::SetIsBeginSceneClearFb(bool clear) { lbl_80418AE4 = clear; }

