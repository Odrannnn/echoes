/**
 * `fn_80271238` - `CCubeRenderer`'s constructor. Retail 0x80271238, 0x59C = 1436 bytes, one
 * function, unclaimed until this unit.
 *
 * ## Why this is the function the port is waiting on
 *
 * `src/MetaRender/Carve8026EF54.cpp` is `AllocateRenderer`: it takes 1376 bytes from
 * `fn_80272958` and hands them to *this* function, then stores the result into `gpRender`
 * (`.sbss:0x804192F8`, written by `CGameGlobalObjects::PostInitialize`,
 * `src/MetroidPrime/main.cpp:239`). Until now `fn_80271238` had no body anywhere, so
 * **`gpRender` pointed at 1376 bytes that no constructor ever ran** - the first
 * `lwz r12,0(r3)` in the frame loop read whatever the allocator left there. This unit is the
 * constructor, so the object is constructed. **It is written and measured; what is still missing
 * is the `files.cmake` entry** - see `src/MetaRender/PortCCubeRenderer.cpp` and the end of this
 * header.
 *
 * ## `sizeof(CCubeRenderer)` is 1376, and the header now says so
 *
 * Retail's own allocator is handed `li r3,1376` (`fn_8026EF54`, 0x8026EF70): that is the block
 * size, so **1376 = 0x560 is the object's size.** The highest thing this constructor writes is
 * `stw r6,1372(r30)` at 0x8027175C, i.e. 0x558 + 4 = 0x55C, and 0x560 is the next 16-byte
 * boundary above it.
 *
 * **CORRECTED 2026-09-27 (lane `render2`).** This file used to read the class through a **local
 * duplicate shape** and its header said `include/MetaRender/CCubeRenderer.hpp` "is 516 bytes short
 * (`sizeof` 0x35C against the 0x560 retail's own `li r3,1376` implies)", and that "nothing here
 * can be `Matching` until that header is corrected by its owner". Both are now false: commit
 * `ce1236d` fixed the header, this file includes it, and the body is written against the real
 * class. The measured consequence is large and is the reason the correction was worth making:
 *
 * | | before the header fix | after |
 * |---|---|---|
 * | objdiff fuzzy | 3.56% | **98.92%** |
 * | functions paired | 0 | **1 / 1** |
 * | `sizeof` (mwcceppc, `.sdata2` word 0) | 0x35C | **0x560** |
 * | `offsetof(x4fc_bigRing)` | - | **0x4FC** |
 * | `offsetof(x550_darkLightworldPalette)` | - | **0x550** |
 *
 * The three measurement words are `lbl_sizeof_CCubeRenderer`, `lbl_offsetof_CCubeRenderer_x4fc` and
 * `lbl_offsetof_CCubeRenderer_x550` at the bottom of this file; **mwcceppc puts them in `.sdata2`,
 * not `.data`**, because they are `const` - `nm` reports the `.data` symbols at their
 * `.data`-relative offsets with the values zero, so reading `.data` measures nothing.
 * `CHECK_SIZEOF(CCubeRenderer, 0x560)` is NOT usable: mwcceppc 2.7 rejects
 * `check_sizeof<cls,n>::value` as an array bound for a class with a mem-initialiser list
 * ("illegal constant expression", measured).
 *
 * Every offset in the shape is read out of retail's own stores, not assumed:
 *
 * ```
 * 0x000 stw r7 / stw r31                 two vptrs, then IFactory* and IObjectStore*
 * 0x010 bl fn_802BAD6C                   CFont::CFont(float), f1 = -23.0f
 * 0x018 stw r6                           0
 * 0x020 stw r0 x4 (r0 = this+0x28), stw r6 four self-pointers, then a 0
 * 0x034 bl __ct__14CFrustumPlanes...     900.f, -23.f, -23.f, 5.f, and a matrix
 * 0x098 stw r0                           0
 * 0x0A0 stfs f1/f1/f1/f0, stb r0         CVector3f::Normalize() of (-0.f,-23.f,-0.f)
 * 0x0B8 bl __ct__8CTextureF12... x6      4x4, 32x32, 256x256, 32x32, 16x16, 8x8
 * 0x120 stw r0                           0
 * 0x124 .. 0x32C                         the five remaining CTextures
 * 0x32C bl __ct__9CRandom16FUi           seed 20
 * 0x334 stw r4 x4, stw r3, stw r0        four self-pointers, 0, 2
 * 0x34C bl White__6CColorFv              then lwz r0,0(r3)
 * 0x4FC .. 0x544                         seven TLockedToken<>, each from store.GetObj()
 * 0x550 stw r3                           fn_802711A4(this, &token)
 * 0x554 rlwimi x8, stw r6 x2             eight bitfields (one true) and two zeros
 * 0x0C2 rlwimi                           CTexture::SetFlag1(true) on the first texture
 * 0x0B8 bl fn_802C46E0 / memset / fn_802C4A5C
 * ...      bl fn_80270EC8, 80270D44, 80270BB4, 80270A64, 80271104
 * 0x7AC stw r30,-26168(r13)              lbl_80419758 = this
 * 0x7B0 bl fn_80272624 ; mr r3,r30 ; blr
 * ```
 *
 * The two runs of four self-pointers (0x020 and 0x334) have the same shape - four words all
 * holding `this+8`, then a zero - and the 0x334 one has a second word, `2`. That is written
 * here as five and six plain fields rather than as a type, because **no name for either is
 * claimed**: the .rodata neighbourhood gives no evidence and inventing one would be a claim
 * about a class this file has not measured.
 *
 * ## The eight pool names, and a stale fact in this tree's own tooling
 *
 * The eight `IObjectStore::GetObj(const char*)` calls pass
 * `0x803AE3BC + {93, 106, 126, 144, 160, 179, 197, 218}`, which read as **`TXTR_BigRing`,
 * `TXTR_DarkWorldCloud`, `TXTR_ScanSweepBar`, `CMDL_FlatSphere`, `CMDL_FlatSphereLow`,
 * `CMDL_FlatCylinder`, `CMDL_FlatCylinderLow`, `TXTR_DarkLightworldPalette`.**
 *
 * **Getting those names out needed a fix, and the fix is in `tools/dol_read.py`, not here.**
 * Its hardcoded `SECTIONS` table puts `.rodata` at file offset **0x3A27A0**; the DOL's own
 * header puts it at **0x3A26C0** (`offsets[4]`, with `.ctors` at 0x3A24A0 and `.dtors` at
 * 0x3A26A0). Every `.rodata` virtual address therefore resolves 0xE0 = 224 bytes late, and
 * `0x803AE419` came back as the tail of an unrelated string - which is what made the first
 * pass at this function look like it was passing eight garbage names. At the correct offset the
 * same eight addresses give the eight names above, and `lbl_803AE3BC` really is the
 * `"DrawGeometryScanTranslast"` the `Carve8026EF54` header already reconstructed. `.data` and
 * `.sdata2` in that table are right, which is why the vtable at 0x803B8C10 read correctly. The
 * tool's own docstring already warns that a question about `.data` cannot be answered from the
 * built ELF; the same is true of `.rodata`, and this is the measurement.
 *
 * ## The float constants
 *
 * `r2` is `_SDA_BASE_` = 0x8041FD80, so the `lfs` displacements read against `.sdata2` and are
 * exact: `-17628` = **-23.0f** (used twice), `-17344` = **900.0f**, `-17340` = **5.0f**,
 * `-17596` = **-0.0f**. `-17624` = 471.0f is the near neighbour and is not used here.
 *
 * ## The matrix at 0x804173D4 is `CTransform4f::sIdentity`
 *
 * `CFrustumPlanes`'s `const CTransform4f&` argument is 0x804173D4, which is in the DOL's
 * **unbacked gap** between `.data` (ends 0x803C5A10) and `.sdata` (0x80417D80) - so it is a
 * runtime-initialised `.bss` object and there are no bytes at that address in the file to read.
 * **`src/MetroidPrime/main.cpp:463` already names it**: "sIdentity__12CTransform4f is .bss
 * 0x804173D4", from `CGameGlobalObjects::AddPaksAndFactories`'s two
 * `CGraphics::Set*(CTransform4f::Identity())` calls. The first pass of this file passed a
 * zeroed local array there, on the reasoning that an unbacked address held zeros; that was
 * wrong - it is the identity matrix, filled in by a static initialiser - and `Identity()` is
 * what the call uses now. Reading the address out of the neighbouring unit is cheaper than
 * inferring it.
 *
 * ## `NonMatching`, and the measured reason
 *
 * The unit is `NonMatching`, and the reason is a single proven structural wall. `unit_fit` on
 * 2026-09-27: **`.text` claimed 1436, ours 2652, over by 1216**, and all 1216 bytes are 12 COMDAT
 * weak destructors pulled in by the mem-initialiser list (`__dt__14CFrustumPlanesFv` 144,
 * two `rstl::list` destructors 140 each, `rstl::vector` 132, four `TLockedToken`/`single_ptr` 88
 * each, two `TToken` 84 each). The retail linker discards them and so does mwldeppc's
 * `-strip_partial`, which is the `CAi` case `unit_fit`'s own note describes.
 *
 * The *function* is 1432 bytes against retail's 1436 - one instruction short - and the whole
 * difference is the string-literal base. Retail emits
 *
 * ```
 * lis  r3,0        ; R_PPC_ADDR16_HA lbl_803AE3BC
 * addi r5,r3,0     ; R_PPC_ADDR16_LO lbl_803AE3BC
 * addi r5,r5,93    ; "TXTR_BigRing" is lbl_803AE3BC + 93
 * ```
 *
 * and this file emits the same three instructions against `@stringBase0` with residuals
 * 13, 33, 51, 67, 86, 104, 125, ... instead of 93, 106, 126, 144, 160, 179, 197, 218. That is
 * **eight `addi` immediates and nothing else** - 17 differing lines in `tools/lanediff.sh`, all of
 * them one of those pairs or the two-relocation interleaving around it.
 *
 * It cannot be fixed, and this is not a missing spelling: owning `lbl_803AE3BC` means owning
 * 0x803AE3BC, which is **4 (mod 8)** while every MWCC data input section is 8-aligned, so
 * mwldeppc places the object at 0x803AE3C0 and leaves four zero bytes behind. Measured: 856
 * `.text` bytes, 6,651 `.rodata` bytes, 10 `.data` and 3 `.sdata` of `main.dol` stop matching
 * retail. `Carve8026EF54.cpp`'s header has the full three-link chain, and
 * `config/G2ME01/splits.txt` refuses the claim twice over besides ("ends within symbol").
 * **`Matching` here would mean shipping a broken DOL**, so the unit is `NonMatching` on purpose.
 *
 * The two reasons this header used to give for the same verdict - "the layout is 516 bytes short
 * in the header" and "the three `.data` vtable addresses are unowned" - are handled elsewhere and
 * neither is a reason any more:
 *
 *  1. **The layout is right.** `sizeof(CCubeRenderer)` is 0x560, measured above.
 *  2. **The three `.data` vtables (0x803B0C1C, 0x803B8B70, `__vt__13CCubeRenderer` at 0x803B8C10)
 *     are still unclaimed `.data`, so in the matching build the `stw r0,0(r30)` /
 *     `stw r0,4(r30)` at 0x8027124C/0x8027126C point at retail's own addresses and this object
 *     is linked from dtk's retail object anyway.** On the host they are no longer zeros:
 *     `src/MetaRender/Carve80270848.cpp` is the class's key function and therefore emits
 *     `vtable for CCubeRenderer`, so the vptr stores resolve to a real table.
 *  3. **The mem-init/body order is a MWCC/host difference, not a fault.** Retail's order is base
 *     vptrs, derived vptrs, then the body; a mem-initialiser list runs first, which is why the
 *     two `stw ... 0(r30)` pairs land 200-odd bytes from retail's. It costs bytes, not
 *     correctness, and it is why the extra COMDAT destructors exist at all.
 *
 * **`CFrustumPlanes` is not a wall, and that was worth checking.** Retail's mangled name is
 * `__ct__14CFrustumPlanesFRC12CTransform4ffffbf`, and the call in this file emits *exactly* that
 * symbol from the tree's own `(const CTransform4f&, float, float, float, bool, float)` prototype
 * - `ffffbf` is one `f` from the class name `CTransform4f` plus `f,f,f,b,f`. Retail's `li r6,0`
 * at 0x80271298 is a long-lived zero temporary that is also stored at 0x18 and 0x30, not a
 * sixth argument. So the argument registers agree: `fov` 900.0, `aspect` -23.0, `nearZ` -23.0,
 * `farZ` 5.0, `useFarPlane` false.
 *
 * For the port this costs nothing: a `NonMatching` unit is linked into `main.dol` from **dtk's
 * retail object** (`build/G2ME01/obj/...`), so the DOL is unchanged and its sha1 holds, while
 * `files.cmake` compiles this body for the host and the object the port dereferences is real.
 *
 * ## What this does not buy
 *
 * It does not render a frame, and it is not close. The six tail callees (`fn_80270EC8`,
 * `fn_80270D44`, `fn_80270BB4`, `fn_80270A64`, `fn_80271104`, `fn_80272624`), plus
 * `fn_802C46E0`, `fn_802C4A5C`, `fn_802BAD6C` and `CTexture`'s constructor, are all unclaimed, so
 * this constructor *calls* them: the boot probe stubs them, the real port link does not. The
 * seven `TLockedToken<>` loads are `GetObj(name)` then `CToken::GetObj()` then a load of the
 * result's `+4`; `CToken::GetObj()` dereferences `x0_objRef` without a null test, so in a port
 * with no paks loaded that is a null dereference, and it is retail's behaviour rather than a
 * bug here. **That null dereference is the next wall on this path, and it is a data wall, not a
 * decompilation one** - see `docs/HANDOFF.md`'s "the game's assets are not on this machine".
 *
 * **And the host reaches this constructor through `mp_CCubeRenderer_ctor`, not through
 * `fn_80271238`.** The name `fn_80271238` is what dtk gives the *claim*; the definition in this
 * file is the C++ member, which objdiff pairs as
 * `__ct__13CCubeRendererFR12IObjectStoreR10COsContextR10CMemorySysR8IFactory`. Those are two
 * different symbols, so `Carve8026EF54.cpp` calling `extern "C" fn_80271238` resolved to the
 * reach stub even with this file compiled. The bridge is
 * `src/MetaRender/PortCCubeRenderer.cpp`.
 */

#include "types.h"

#include <string.h>

#include "MetaRender/CCubeRenderer.hpp"

#include "Kyoto/Alloc/CMemorySys.hpp"
#include "Kyoto/Basics/COsContext.hpp"
#include "Kyoto/CResFactory.hpp"
#include "Kyoto/IObjectStore.hpp"

extern "C" {
/**
 * Unnamed in `symbols.txt`. `r3` is `this` and unused; `r4` is the palette texture's token, and
 * the result - a `new CGraphicsPalette` built from that texture's data - lands in +0x550.
 */
CGraphicsPalette* fn_802711A4(CCubeRenderer* self, const TLockedToken< CTexture >& tok);
/**
 * `CTexture::GetBitMapData(int)` - a one-call forwarder to `GetConstBitMapData__8CTextureCFi`
 * that reads `r3` and `r4` only. Retail's `r5 = 1` and `r6 = 0` at the call are left over from
 * the flag and bitfield stores, not arguments; the first reading passed them as two more.
 */
void* fn_802C46E0(CTexture* tex, int mip);
/** `CTexture`'s unlock. */
void fn_802C4A5C(CTexture* tex);
/** The five `CCubeRenderer` methods the tail of this constructor calls, all unclaimed. */
void fn_80270EC8(CCubeRenderer* self);
void fn_80270D44(CCubeRenderer* self);
void fn_80270BB4(CCubeRenderer* self);
void fn_80270A64(CCubeRenderer* self);
void fn_80271104(CCubeRenderer* self);
/** Clears four `.sbss` words (0x80419754, 58, 64, 68). The destructor calls it too. */
void fn_80272624();

/**
 * `.sbss:0x80419748`, 4 bytes: the live renderer. **Not 0x80419758** - the first pass read
 * `stw r30,-26168(r13)` against the wrong base; dtk's own disassembly names the target
 * `lbl_80419748@sda21`, and the destructor clears the same word (0x80270880).
 */
extern CCubeRenderer* lbl_80419748;
} // extern "C"

#ifdef TARGET_PC
CCubeRenderer* lbl_80419748 = 0;
#endif

CCubeRenderer::CCubeRenderer(IObjectStore& store, COsContext& osContext, CMemorySys& memorySys,
                             IFactory& resFactory)
: x8_factory(resFactory)
, xc_store(store)
// 0x80271294. `lbl_8041DEE4` is 1.0f. The first pass read it as -23.0f by resolving the
// `@sda21` against `_SDA_BASE_` (r13); `.sdata2` is addressed from r2, and dtk's label is
// the address. The same mistake made the frustum's constants 900/-23/-23/5.
, x10_font(1.f)
, x18_(0)
// 0x802712D8. `lbl_8041E000` = 1.5707964f (pi/2), `lbl_8041DEE4` = 1.0f twice (`fmr f3,f2`),
// `r5` = 0, `lbl_8041E004` = 100.0f. Retail's mangled callee is
// `__ct__14CFrustumPlanesFRC12CTransform4ffffbf`, which this prototype emits exactly.
, x34_frustumPlanes(CTransform4f::Identity(), 1.5707964f, 1.f, 1.f, false, 100.f)
, x98_drawableCallback(nullptr)
// 0x802712E0-0x80271334: (0, 1, 0) built on the stack, `Normalize()`d in place, copied in,
// and a 0.0f constant at +0xAC.
, xa0_viewPlane(0.f, CUnitVector3f(0.f, 1.f, 0.f, CUnitVector3f::kN_Yes))
, xb0_(false)
, xb8_blackTex(kTF_RGB565, 4, 4, 1)
, x124_tex(kTF_IA8, 32, 32, 1)
, x18c_tex(kTF_I8, 256, 256, 1)
, x1f4_tex(kTF_I8, 32, 32, 1)
, x25c_tex(kTF_I4, 16, 16, 1)
, x2c4_tex(kTF_I4, 8, 8, 1)
, x32c_random(20)
, x348_(2)
// 0x802713F0. `White()` returns a reference, which is why the next instruction is an `lwz`.
, x34c_color(CColor::White())
, x350_normal(CVector3f::Forward())
, x370_count(0)
, x4f4_phazonSuitMaskCountdown(0)
// 0x80271460-0x802716CC. Each is `GetObj(name)` on the store (`lwz r12,16(r12)`, slot 2),
// `CToken`'s copy constructor into the member, `CToken::GetObj()`, a load of its +4 into the
// member's +8, and the temporary's destructor.
, x4fc_bigRing(store.GetObj("TXTR_BigRing"))
, x508_darkWorldCloud(store.GetObj("TXTR_DarkWorldCloud"))
, x514_scanSweepBar(store.GetObj("TXTR_ScanSweepBar"))
, x520_flatSphere(store.GetObj("CMDL_FlatSphere"))
, x52c_flatSphereLow(store.GetObj("CMDL_FlatSphereLow"))
, x538_flatCylinder(store.GetObj("CMDL_FlatCylinder"))
, x544_flatCylinderLow(store.GetObj("CMDL_FlatCylinderLow"))
// 0x8027167C-0x802716E4. The eighth token is a stack `TLockedToken<CTexture>` (its +8 is
// stored at 0x50(r1), 0x48 + 8), passed by address, and destroyed right after.
, x550_darkLightworldPalette(
      fn_802711A4(this, TLockedToken< CTexture >(store.GetObj("TXTR_DarkLightworldPalette"))))
// 0x802716E8-0x80271754: every one of the eight inserts r6, which is 0.
, x554_24_(false)
, x554_25_(false)
, x554_26_(false)
, x554_27_(false)
, x554_28_(false)
, x554_29_(false)
, x554_30_(false)
, x554_31_(false)
, x558_(0)
, x55c_(0) {
  // 0x80271760-0x80271780. `rlwimi r0,r5,7,24,24` on +0xC2 is `CTexture`'s `mLocked`: this is
  // the inline `CTexture::Lock()`, spelled out because its callee is unnamed in `symbols.txt`.
  xb8_blackTex.SetFlag1(true);
  memset(fn_802C46E0(&xb8_blackTex, 0), 0, 32);
  fn_802C4A5C(&xb8_blackTex);

  // 0x80271788-0x802717A8.
  fn_80270EC8(this);
  fn_80270D44(this);
  fn_80270BB4(this);
  fn_80270A64(this);
  fn_80271104(this);

  // 0x802717AC.
  lbl_80419748 = this;
  fn_80272624();
}

// The measurement: three `const int` words, readable with `objdump -s` on the object.
// mwcceppc says `sizeof(CCubeRenderer)` is 0x560, which is what retail's own `li r3,1376` says.
//
// **They land in `.sdata2`, not `.data`** - they are `const`, and mwldeppc's small-data rule keys
// off the declared `const`. Measured 2026-09-27: `objdump -s -j .data` on this object shows all
// zeroes and `nm` reports the three symbols in `.data` at offsets 0/4/8, which measures nothing;
// the values are in `.sdata2`:
//
//     0000 00000560 000004fc 00000550 3f800000   `.........P?`
//          ^0x560    ^0x4FC    ^0x550
//
// `CHECK_SIZEOF(CCubeRenderer, 0x560)` is NOT used: mwcceppc 2.7 rejects
// `check_sizeof<cls,n>::value` as an array bound for a class with a mem-initialiser list
// ("illegal constant expression", measured), which is why this is a word instead.
extern "C" const int lbl_sizeof_CCubeRenderer = sizeof(CCubeRenderer);
extern "C" const int lbl_offsetof_CCubeRenderer_x4fc = offsetof(CCubeRenderer, x4fc_bigRing);
extern "C" const int lbl_offsetof_CCubeRenderer_x550 =
    offsetof(CCubeRenderer, x550_darkLightworldPalette);
