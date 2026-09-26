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
 * constructor, so the object is constructed.
 *
 * ## `sizeof(CCubeRenderer)` is 1376 and the header says 860 - this is the headline
 *
 * Retail's own allocator is handed `li r3,1376` (`fn_8026EF54`, 0x8026EF70): that is the block
 * size, so **1376 = 0x560 is the object's size.** The highest thing this constructor writes is
 * `stw r6,1372(r30)` at 0x8027175C, i.e. 0x558 + 4 = 0x55C, and 0x560 is the next 16-byte
 * boundary above it. `include/MetaRender/CCubeRenderer.hpp` ends its member list at
 * `CVector3f x350_normal`, which puts `sizeof` at **0x35C = 860 - 516 bytes short.**
 *
 * The class is therefore read here through a **local duplicate shape** - the pattern
 * `CModelDataModelSlots` already uses in this tree - rather than through the header. That is
 * deliberate: `include/MetaRender/CCubeRenderer.hpp` is shared with other lanes and that fix
 * belongs with its owner. `CHECK_SIZEOF(CCubeRendererCtor, 0x560)` at the bottom of this file
 * is the measurement - **mwcceppc agrees, so this shape is 0x560 and the header is not.** A
 * host `sizeof` is not evidence about anything here; the host is 64-bit and MWCC is 32-bit.
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
 * The unit is `NonMatching`, and none of the three reasons is a missing spelling:
 *
 *  1. **The layout is 516 bytes short in the header this tree would have to use.** The body
 *     below is written against the measured 0x560 shape, so *this* file is right and
 *     `include/MetaRender/CCubeRenderer.hpp` is wrong. Nothing here can be `Matching` until
 *     that header is corrected by its owner; until then the offsets in the header and the
 *     offsets retail writes are different objects, and every store in this unit would be at the
 *     wrong displacement.
 *  2. **Three `.data` vtable addresses are unowned** - 0x803B0C1C, 0x803B8B70, and
 *     `__vt__13CCubeRenderer` at 0x803B8C10 (0x140 and 0x150 bytes of vtable). Claiming them
 *     means claiming 0x140 + 0x150 bytes of `.data`, which is a different unit's problem, and
 *     `dtk dol split` refuses a claim that ends inside a symbol.
 *  3. **The stores cannot be emitted in retail's order.** The member constructions have to be a
 *     **mem-initialiser list** and the scalar stores in the **body**, because MWCC 2.7 accepts
 *     `p->Ctor(args)` (measured) and clang rejects it (`invalid use of 'CToken::CToken'`), and a
 *     file the host also compiles cannot use the MWCC-only form. Retail's order is
 *     base vptrs, derived vptrs, then the body; here it is the mem-inits, then the vptrs, which
 *     already puts the two `stw ... 0(r30)` pairs 200-odd bytes apart from retail. A second
 *     function also falls out of it: mwcceppc emits the mem-initialiser list as its own
 *     `__ct__17CCubeRendererCtorFR12IObjectStoreR8IFactory`, so the object's `.text` is 0x758
 *     bytes for two functions where retail has 0x59C for one.
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
 * three vtable addresses are unowned too, so `gpRender->` dispatches through a zero table - which
 * is the same "jump to address 0" the boot probe is built to turn into a named symptom, but it
 * does mean the next thing to write is a `CCubeRenderer` vtable, not another method. And the
 * seven `TLockedToken<>` loads are `GetObj(name)` then `CToken::GetObj()` then a load of the
 * result's `+4`; `CToken::GetObj()` dereferences `x0_objRef` without a null test, so in a port
 * with no paks loaded that is a null dereference, and it is retail's behaviour rather than a
 * bug here. All three are named blockers, not surprises.
 */

#include "types.h"

#include <string.h>

// MWCC 2.7 has the placement form of `operator new` built in and has no `<new>` to
// include (its own C++ headers are not on this project's path: `-nosyspath` plus
// `-i libc`), so the include is for the host build only. Measured: including it
// unconditionally fails with "the file 'new' cannot be opened".
#ifndef __MWERKS__
#include <new>
#endif

#include "Kyoto/Alloc/CMemorySys.hpp"
#include "Kyoto/Basics/COsContext.hpp"
#include "Kyoto/CResFactory.hpp"
#include "Kyoto/CRandom16.hpp"
#include "Kyoto/CToken.hpp"
#include "Kyoto/Graphics/CColor.hpp"
#include "Kyoto/Graphics/CTexture.hpp"
#include "Kyoto/IObjectStore.hpp"
#include "Kyoto/Math/CFrustumPlanes.hpp"
#include "Kyoto/Math/CTransform4f.hpp"
#include "Kyoto/Math/CVector3f.hpp"
#include "Kyoto/TToken.hpp"

class CModel;

/** `CFont`'s two fields: `int mFontSize` at +0, `float mScale` at +4. */
struct SFont {
  int mFontSize;
  float mScale;
};

extern "C" {
/** `CFont::CFont(float)`: `x00 = (int)(16.f * scale) ; x04 = scale`. Unnamed in symbols.txt. */
void fn_802BAD6C(SFont* self, float scale);
/** Unnamed in `symbols.txt`; its `r4` is a `CToken` and its result is the one word at +0x550. */
int fn_802711A4(void* self, CToken& tok);
void fn_802C46E0(CTexture* tex, int, int, int);
void fn_802C4A5C(CTexture* tex, int, int);
/** The five `CCubeRenderer` methods the tail of this constructor calls, all unclaimed. */
void fn_80270EC8(void* self);
void fn_80270D44(void* self);
void fn_80270BB4(void* self);
void fn_80270A64(void* self);
void fn_80271104(void* self);
void fn_80272624();

/**
 * The three `.data` addresses retail stores into the two vptr slots. **Declared, never defined
 * in the matching build** - see (2) in the header. Under `TARGET_PC` they are defined as zeros
 * below, and that is deliberate rather than a shortcut: a zero vptr makes `gpRender->` dispatch
 * a jump to address 0, which is the exact symptom `tools/boot_probe.sh` is built to turn into a
 * named missing symbol. Leaving them undefined would instead let the probe's self-heal append
 * a *function* stub for a symbol that is a data table, and the object would then hold a code
 * address. A wrong-looking fault is worth more than a plausible-looking one.
 */
extern void* lbl_803B0C1C[];
extern void* lbl_803B8B70[];
extern void* __vt__13CCubeRenderer[];

/** `.sbss:0x80419758`, 4 bytes. The last thing this constructor writes. */
extern void* lbl_80419758;
} // extern "C"

#ifdef TARGET_PC
void* lbl_803B0C1C[0x50] = {0};
void* lbl_803B8B70[4] = {0};
void* __vt__13CCubeRenderer[0x54] = {0};
void* lbl_80419758 = 0;
#endif


/**
 * **`sizeof` is 0x560 and the class in `include/MetaRender/CCubeRenderer.hpp` is 0x35C.** A
 * local duplicate shape on purpose: the header is shared, its owner has to make that change,
 * and reading the object through a shape whose size is checked here is what makes the
 * discrepancy visible instead of silently wrong. The members are declared in the order retail
 * constructs them, because mwcceppc emits a mem-initialiser list in declaration order.
 */
class CCubeRendererCtor {
public:
  // 0x000. Retail writes the base pair first - the inlined `IRenderer` ctor - and then its own,
  // 0x140 bytes into the derived vtable, which is the secondary vptr of the virtual base. The
  // two words after them are the only two constructor arguments retail reads: `r7` (the fourth)
  // and `r4` (the first).
  void* x000_vtable0;
  void* x004_vtable1;
  IFactory* x008_factory;
  IObjectStore* x00c_store;
  SFont x010_font;      // 0x010, 8 B
  int x018_zero;        // 0x018
  uchar x01c_pad[4];     // 0x01C - never written by retail's constructor
  void* x020_p0;        // 0x020, four self-pointers, all holding &x028_p2
  void* x024_p1;
  void* x028_p2;
  void* x02c_p3;
  int x030_zero;
  CFrustumPlanes x034_frustum;  // 0x034, 0x64
  void* x098_callback;          // 0x098
  int x09c_unk;                 // 0x09C - never written by retail's constructor
  CVector3f x0a0_normal;        // 0x0A0
  float x0ac_w;                 // 0xAC
  bool x0b0_flag;               // 0xB0, one byte: retail stores it with `stb`
  uchar x0b1_pad[7];            // 0x0B1 - never written by retail's constructor
  CTexture x0b8_tex;            // 0x0B8, 0x68
  int x120_zero;                // 0x120
  CTexture x124_tex;            // 0x124, 0x68
  CTexture x18c_tex;            // 0x18C, 0x68
  CTexture x1f4_tex;            // 0x1F4, 0x68
  CTexture x25c_tex;            // 0x25C, 0x68
  CTexture x2c4_tex;            // 0x2C4, 0x68
  CRandom16 x32c_random;        // 0x32C
  uchar x330_pad[4];             // 0x330 - never written by retail's constructor
  void* x334_p0;                // 0x334, four self-pointers, all holding &x33c_p2
  void* x338_p1;
  void* x33c_p2;
  void* x340_p3;
  int x344_zero;
  int x348_two;
  CColor x34c_white;  // 0x34C
  uchar x350_pad[0x1ac];  // 0x350 .. 0x4FC
  TLockedToken< CTexture > x4fc_ring;   // 0x4FC
  TLockedToken< CTexture > x508_cloud;  // 0x508
  TLockedToken< CTexture > x514_sweep;  // 0x514
  TLockedToken< CModel > x520_flat;     // 0x520
  TLockedToken< CModel > x52c_flatlow;  // 0x52C
  TLockedToken< CModel > x538_cyl;      // 0x538
  TLockedToken< CModel > x544_cyllow;   // 0x544
  int x550_unk;                         // 0x550
  bool x554_b0 : 1;  // 0x554
  bool x554_b1 : 1;
  bool x554_b2 : 1;
  bool x554_b3 : 1;
  bool x554_b4 : 1;
  bool x554_b5 : 1;
  bool x554_b6 : 1;
  bool x554_b7 : 1;
  uchar x555_pad[3];  // 0x555 - never written by retail's constructor
  int x558_zero;  // 0x558
  int x55c_zero;  // 0x55C
  // 0x560

  CCubeRendererCtor(IObjectStore& store, IFactory& resFactory);
};

CCubeRendererCtor::CCubeRendererCtor(IObjectStore& store, IFactory& resFactory)
: x000_vtable0(lbl_803B0C1C),
  x004_vtable1(lbl_803B8B70),
  x008_factory(&resFactory),
  x00c_store(&store),
  x018_zero(0),
  x020_p0(&x028_p2),
  x024_p1(&x028_p2),
  x028_p2(&x028_p2),
  x02c_p3(&x028_p2),
  x030_zero(0),
  // 0x802712D8. 900.0f / -23.0f / -23.0f / 5.0f - see (3) in the header for the prototype.
  x034_frustum(CTransform4f::Identity(), 900.0f, -23.0f, -23.0f, false, 5.0f),
  x098_callback(0),
  // 0x802712E0-0x80271338. The normalised vector and the two words after it are in the body,
  // because `Normalize()` is not const and cannot be a mem-initialiser here.
  x0b8_tex(kTF_RGB565, 4, 4, 1),
  x120_zero(0),
  x124_tex(kTF_IA8, 32, 32, 1),
  x18c_tex(kTF_I8, 256, 256, 1),
  x1f4_tex(kTF_I8, 32, 32, 1),
  x25c_tex(kTF_I4, 16, 16, 1),
  x2c4_tex(kTF_I4, 8, 8, 1),
  x32c_random(20),
  x334_p0(&x33c_p2),
  x338_p1(&x33c_p2),
  x33c_p2(&x33c_p2),
  x340_p3(&x33c_p2),
  x344_zero(0),
  x348_two(2),
  // 0x802713F0. `White()` returns a reference, which is why the next instruction is an `lwz`.
  x34c_white(CColor::White()),
  // 0x80271418-0x802716CC. Seven `TLockedToken<>` and one bare `CToken`. Each is
  // `GetObj(name)` on the store, a `CToken` copy, `CToken::GetObj()`, a load of its +4 and a
  // destruction of the temporary. `lwz r12,16(r12)` is `IObjectStore` vtable slot 2, which is
  // `GetObj(const char*)` returning through the hidden `r3`.
  x4fc_ring(store.GetObj("TXTR_BigRing")),
  x508_cloud(store.GetObj("TXTR_DarkWorldCloud")),
  x514_sweep(store.GetObj("TXTR_ScanSweepBar")),
  x520_flat(store.GetObj("CMDL_FlatSphere")),
  x52c_flatlow(store.GetObj("CMDL_FlatSphereLow")),
  x538_cyl(store.GetObj("CMDL_FlatCylinder")),
  x544_cyllow(store.GetObj("CMDL_FlatCylinderLow")),
  // 0x802711A4's prototype takes its `CToken` by reference and MWCC 2.7 will not bind a
  // prvalue to it, so the eighth `GetObj` needs a named temporary and moves to the body.
  x550_unk(0),
  // 0x802716E8-0x8027175C. Eight bitfields, six of them written twice: only bit 6 is set.
  x554_b0(false),
  x554_b1(false),
  x554_b2(false),
  x554_b3(false),
  x554_b4(false),
  x554_b5(false),
  x554_b6(true),
  x554_b7(false),
  x558_zero(0),
  x55c_zero(0) {
  // 0x80271280-0x80271290. Retail's own two vptr stores, which C++ cannot put before the
  // member constructions; see the fourth reason in the header.
  x000_vtable0 = __vt__13CCubeRenderer;
  x004_vtable1 = &__vt__13CCubeRenderer[0x50];

  // 0x80271294. f1 is loaded at 0x80271244 and not touched until here: -23.0f.
  fn_802BAD6C(&x010_font, -23.0f);

  // 0x802712E0-0x80271338. (-0.0f, -23.0f, -0.0f) normalised in retail through a stack
  // temporary; the three components and a fourth float land at 0xAC, and a byte at 0xB0.
  x0a0_normal.SetX(-0.0f);
  x0a0_normal.SetY(-23.0f);
  x0a0_normal.SetZ(-0.0f);
  x0a0_normal.Normalize();
  x0ac_w = -0.0f;
  x0b0_flag = false;

  // 0x80271760. `rlwimi r0,r5,7,24,24` is the first bit of `CTexture`'s bitfield group at +0xA,
  // and `SetFlag1` is `mLocked = b` inline - so it is this, and not `Lock()`.
  x0b8_tex.SetFlag1(true);

  // 0x802716A0-0x802716CC. The eighth `GetObj` is not a member: it is a stack temporary whose
  // address is `r4` to fn_802711A4, and only that function's return value is stored.
  CToken palette = store.GetObj("TXTR_DarkLightworldPalette");
  x550_unk = fn_802711A4(this, palette);

  fn_802C46E0(&x0b8_tex, 0, 1, 0);
  memset(&x0b8_tex, 0, 32);
  fn_802C4A5C(&x0b8_tex, 0, 32);

  // 0x80271788-0x802717A8.
  fn_80270EC8(this);
  fn_80270D44(this);
  fn_80270BB4(this);
  fn_80270A64(this);
  fn_80271104(this);

  // 0x802717AC. r13 is `_SDA_BASE_` = 0x8041FD80, so -26168 is 0x80419758.
  lbl_80419758 = this;
  fn_80272624();
}

extern "C" {
/**
 * `CCubeRenderer`'s constructor, retail 0x80271238. `self` is the block `fn_80272958`
 * returned; the return value is `self` (`mr r3,r30` at 0x802717B8), which is what
 * `AllocateRenderer` stores into `gpRender`. Only `r4` and `r7` are read - the three arguments
 * between them are dead in retail too - and this is the signature `Carve8026EF54.cpp` declares.
 *
 * **`extern "C"` is load-bearing.** Retail names this function nothing
 * (`config/G2ME01/symbols.txt` line 10867 is the `fn_80271238` placeholder), so objdiff pairs
 * it by that literal name. A C++ definition would mangle to `_Z13fn_80271238v`, objdiff would
 * pair nothing, and the unit would silently score 0/0 - which is why the anonymous carves in
 * this tree are `.c` files. `extern "C"` gets the same unmangled symbol out of a `.cpp`, which
 * is what lets the body be C++ at all.
 */
void* fn_80271238(void* self, IObjectStore& store, COsContext& osContext, CMemorySys& memorySys,
                  IFactory& resFactory);
} // extern "C"

void* fn_80271238(void* self, IObjectStore& store, COsContext& osContext, CMemorySys& memorySys,
                  IFactory& resFactory) {
  new (self) CCubeRendererCtor(store, resFactory);
  return self;
}

// The measurement this file exists to make: three `.data` words, readable with
// `objdump -s` on the object. mwcceppc says 0x560, which is what retail's own `li r3,1376`
// says, and the class in `include/MetaRender/CCubeRenderer.hpp` is 0x35C.
// `CHECK_SIZEOF(CCubeRendererCtor, 0x560)` is NOT used: mwcceppc 2.7 rejects
// `check_sizeof<cls,n>::value` as an array bound for a class with a mem-initialiser list
// ("illegal constant expression", measured), which is why this is a `.data` word instead.
extern "C" const int lbl_sizeof_CCubeRendererCtor = sizeof(CCubeRendererCtor);
extern "C" const int lbl_offsetof_CCubeRendererCtor = offsetof(CCubeRendererCtor, x4fc_ring);
extern "C" const int lbl_offsetof2_CCubeRendererCtor = offsetof(CCubeRendererCtor, x550_unk);
