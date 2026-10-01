#include "MetroidPrime/Player/CMorphBall.hpp"

#include "Kyoto/Alloc/CMemory.hpp"
#include "Kyoto/Audio/CSfxManager.hpp"
#include "Kyoto/CResFactory.hpp"
#include "Kyoto/CSimplePool.hpp"
#include "Kyoto/Math/CMath.hpp"
#include "Kyoto/Math/CPlane.hpp"
#include "Kyoto/Math/CRelAngle.hpp"
#include "Kyoto/Particles/CDeferredParticleEffect.hpp"
#include "Kyoto/Particles/CElementGen.hpp"
#include "Kyoto/Particles/CParticleSwoosh.hpp"
#include "MetroidPrime/CActorLights.hpp"
#include "MetroidPrime/CAnimRes.hpp"
#include "MetroidPrime/CCameraManager.hpp"
#include "MetroidPrime/CModelData.hpp"
#include "MetroidPrime/CRainSplashGenerator.hpp"
#include "MetroidPrime/CWorldShadow.hpp"
#include "MetroidPrime/Cameras/CGameCamera.hpp"
#include "MetroidPrime/Player/CMorphBallShadow.hpp"
#include "MetroidPrime/Player/CPlayer.hpp"
#include "MetroidPrime/Tweaks/CTweakBall.hpp"
#include "MetroidPrime/Tweaks/CTweakPlayer.hpp"

#include "rstl/math.hpp"

// Structure-first reconstruction. TODO bodies below are scaffolds, not equivalent implementations.

// Retail's unclaimed `.rodata` for this unit starts at 0x803A84B8. Its first 0xE4 bytes are the
// eleven tables `LoadMorphBallModel` indexes by suit, in the order below, then 0x124 bytes of
// further const data, then `InitializeWakeEffects`' `effects`/`groups` arrays at +0x208, then the
// string pool at +0x238. Declaration order is what puts a table at a fixed offset, so this block
// is load-bearing: `kUnidentifiedConst` exists only to keep the two arrays after it at +0x208.
struct SMorphBallModelRes {
  const char* name;
  uint shader;
};
static const SMorphBallModelRes kPlainBallModels[3] = {
  {"SamusBallCMDL", 0}, {"SamusBallDarkCMDL", 0}, {"SamusBallLightCMDL", 0}};
static const SMorphBallModelRes kPlainBallLowPolyModels[3] = {
  {"SamusBallLowPolyCMDL", 0}, {"SamusBallLowPolyCMDL", 0}, {"SamusBallLowPolyCMDL", 0}};
static const SMorphBallModelRes kSpiderBallModels[3] = {
  {"SamusBallCMDL", 0}, {"SamusSpiderBallDarkCMDL", 0}, {"SamusBallLightCMDL", 0}};
static const SMorphBallModelRes kSpiderBallLowPolyModels[3] = {
  {"SamusSpiderBallLowPolyCMDL", 0}, {"SamusSpiderBallLowPolyCMDL", 0},
  {"SamusSpiderBallLowPolyCMDL", 0}};
static const SMorphBallModelRes kBoostBallModels[3] = {
  {"SamusBallCMDL", 0}, {"SamusBoostBallDarkCMDL", 0}, {"SamusBallLightCMDL", 0}};
static const SMorphBallModelRes kBoostBallLowPolyModels[3] = {
  {"SamusSpiderBallLowPolyCMDL", 0}, {"SamusSpiderBallLowPolyCMDL", 0},
  {"SamusSpiderBallLowPolyCMDL", 0}};
static const SMorphBallModelRes kSpiderBallCapsModels[3] = {
  {nullptr, 0}, {"SamusSpiderBallDarkCapsCMDL", 0}, {nullptr, 0}};
static const SMorphBallModelRes kFrozenBallModels[3] = {
  {"SamusBallFrozenCMDL", 0}, {"SamusBallFrozenCMDL", 0}, {"SamusBallFrozenCMDL", 0}};
static const int kPlainBallGlowColor[3] = {0, 1, 2};
static const int kSpiderBallGlowColor[3] = {0, 1, 2};
static const int kBoostBallGlowColor[3] = {0, 1, 2};
// 0x124 bytes of const colour tables at 0x803A85A0 that no function in this TU references yet.
// Reserved, not fabricated data: dropping them would move `effects`/`groups` off +0x208.
static const int kUnidentifiedConst[73] = {0};

// Retail's `@stringBase0` puts these two at pool offsets 194 and 195 - immediately after the nine
// model tables and **before** `InitializeWakeEffects`' twelve wake names (214..) - and they are the
// last two literals any function in this TU depends on for its `lis`/`addi` offsets. Declared here
// rather than inline in the constructor because the pool is ordered by first use in declaration
// order: inlined they intern *after* the wake names, which shifts `"??"(??)"` from 378 to 358 and
// costs `CreateBallShadow`, both `Update*Effect` and `InitializeWakeEffects` their last instruction.
// `const char* const` (not `char[]`) is what keeps them pool literals rather than `.data` objects.
static const char* const kNoModelName = "";
static const char* const kMultiplayerBallModelName = "SamusMultiBallANCS";

// Guessed names for TU-local state.
static float sBallCloseToCollisionDistance;

// `sWakeEffectForMaterial` maps a material to the index of the wake effect `mWakeEffects`
// plays for it, with -1 for "none" - what retail's `InitializeWakeEffects` writes
// (`stw` at +0x20 / +0x24 / +0x60 / +0x48 of the object, i.e. `data()[7] / [8] / [23] / [17]`
// = kMT_Phazon / kMT_Dirt / kMT_Organic / kMT_Sand) and what `CMorphBall::CollidedWith`
// reads back (`addi r21,r3,4` for `data()`, then `slwi r0,r0,2` / `lwzx r6,r21,r0` /
// `cmpwi r6,0` - a 4-byte element tested against zero, at 0x800C28BC). The element is an
// **enum, not `int`**, and that is a measurement rather than a guess: retail's own
// out-of-line `resize` for this table (`fn_800C084C`, written out below) fills through
// `rstl::construct`, keeping its placement-new null test (`cmplwi r6,0` / `beq` at
// 0x800C08A4) and its one-store-per-trip loop. `int` is one of the
// `RSTL_DECLARE_TRIVIALLY_CONSTRUCTIBLE` types in `include/rstl/construct.hpp`, so a
// `reserved_vector<int, 64>` fills through the assignment specialisation instead, which
// mwcceppc unrolls eight wide - that is retail's `fn_800D0170` (`reserved_vector<float,15>`,
// 0x800D0170), and it is not what 0x800C084C is. An enum is not on that list, so it takes
// the `new (dest) T(src)` path and the loop keeps its shape. Measured: `int` scores 0% on
// the 29 instructions (the loop is unrolled and the null test is gone), the enum 100%.
enum EWakeEffectIndex {
  kWEI_None = -1,
  kWEI_Phazon = 0,
  kWEI_Dirt = 2,
  kWEI_Organic = 3,
  kWEI_Sand = 4,
};
typedef rstl::reserved_vector< EWakeEffectIndex, 64 > SWakeEffectIndices;

static SWakeEffectIndices sWakeEffectForMaterial;
// Retail passes the fill value by address: `addi r5,r13,-31780` is
// `lbl_8041815C` in `.sdata`, whose four bytes are 0xFFFFFFFF (`tools/dol_read.py
// 0x8041815C 4`), i.e. the -1 of `resize(64, -1)`.
static const EWakeEffectIndex kNoWakeEffect(kWEI_None);

// Retail 0x800CA558, 0x8 = 2 insns: `lwz r3,104(r3)` / `blr`, i.e. `return mCurFrame` with
// `mCurFrame` at +0x68 (`include/Kyoto/Particles/CElementGen.hpp:198`).
//
// **This is the definition of `CElementGen::GetEmitterTime`, and retail's split puts it in *this*
// object**: `nm build/G2ME01/obj/MetroidPrime/Player/CMorphBall.o` has it as a strong `T` and
// `nm build/G2ME01/obj/Kyoto/Particles/CElementGen.o` has it as `U`, and
// `config/G2ME01/splits.txt` puts 0x800CA558 inside `MetroidPrime/Player/CMorphBall.cpp`'s
// `.text` range (0x800C02A4..0x800D06CC). Written inline in the class body it was emitted
// **weak** into `CElementGen.o` and nowhere else, so a flipped link failed with
// `undefined: 'CElementGen::GetEmitterTime() const'` - one of the two symbols
// `tools/flip_test.sh` reported for this unit. It is therefore declared in the header and
// defined here, so the strong definition lands in the object that claims the address.
int CElementGen::GetEmitterTime() const { return mCurFrame; }

// Retail 0x8008808C, 0x2C = 11 insns: the free `rstl::operator==(const basic_string&, const
// char*)`, `__eq__4rstlFRCQ24rstl66basic_string<c,Q24rstl14char_traits<c>,Q24rstl17rmemory_allocator>PCc`
// in `config/G2ME01/symbols.txt:2447`. `CMorphBall::GetMorphBallModel` is its one caller here
// (`bl 8008808c` at 0x800C12D8), and retail emits it **out of line**; the tree's
// `include/rstl/string.hpp:411` declares the same function `inline`, so reaching it through C++
// inlines the `compare` call instead and the caller gains a `li r5,-1` and a `cmpwi r3,0`.
// Written out under an `extern "C"` name here so the call site is a real `bl`, exactly as
// `fn_800C084C` and the other `fn_` bodies in this file are written. **This is what took
// `GetMorphBallModel` from 84.14% to 99.9375%.**
//
// The 0.0625% that was left was **not** the call site's relocation, as this comment used to say:
// objdiff normalises a `R_PPC_REL24` target, so the renamed symbol costs nothing. It was the
// `beq`/`bne` of the empty-name test, and flipping the condition's polarity to match retail
// (see `GetMorphBallModel` below) makes the function **100.00%** with the reloc still named
// `rstl_string_eq_c`. For the record, renaming the symbol anyway is not reachable from C++: MWCC
// accepts neither an `asm("...")` label on a function (`type cannot be made into a global register
// variable; only scalers, doubles, floats and vectors are supported`) nor namespace-scope
// `__asm__` (`')' expected`) - both measured, both rejected by the compiler - and nothing needs it.
extern "C" bool rstl_string_eq_c(const rstl::string& lhs, const char* rhs) {
  return lhs.compare(rhs) == 0;
}

// The pair at retail 0x800D0490..0x800D0584 is `rstl::vector< TUniqueId, float >::reserve(int)`
// and the `rstl::uninitialized_copy` helper it calls, written out under `extern "C"` names for the
// same reason `rstl::reserved_vector`'s `operator=` is (see `include/rstl/reserved_vector.hpp`:
// retail's object emits a template instantiation under an unmangled name, so objdiff never pairs
// it with the mangled symbol the compiler writes for us). Retail's pair is byte-identical to the
// weak `reserve__Q24rstl62vector<Q24rstl18pair<9TUniqueId,f>,...>Fi` and the local
// `uninitialized_copy<...pointer_iterator<pair<TUniqueId, float>>...>` this file already emits -
// verified instruction by instruction, 46/46 with only branch-target addresses differing, and
// 15/15 for the helper. The helper's `lhz`/`sth` at +0 and `lfs`/`stfs` at +4 are `TUniqueId`'s
// 2-byte value and the float.
//
// **Both bodies are written out rather than delegated to the template, and the helper's loop is
// written in `pointer_iterator` terms.** `rstl::vector::reserve` and
// `rstl::uninitialized_copy` are both out-of-line under this unit's rule flags
// (`-inline deferred,noauto`), so calling either emits a forwarder - measured 17.09% for the
// wrapper and a 12-instruction stub for the helper. And `fn_800D0548`'s loop is
// `uninitialized_copy`'s own `It`/`It` form, not its `S*`/`S*` overload: retail opens
// `lwz r6,0(r3)` / `lwz r0,0(r4)`, i.e. it reads each argument's `current` field, and the same
// loop written over raw pointers parks the bound in `r3` instead of `r0` (98.67%, that one
// instruction).
typedef rstl::vector< rstl::pair< TUniqueId, float > > SUniqueIdFloats;

extern "C" SUniqueIdFloats::value_type* fn_800D0548(SUniqueIdFloats::iterator begin,
                                                    SUniqueIdFloats::iterator end,
                                                    SUniqueIdFloats::value_type* out) {
  SUniqueIdFloats::value_type* tmp = out;
  SUniqueIdFloats::iterator cur = begin;
  for (; cur != end; ++cur, ++tmp) {
    rstl::construct(tmp, *cur);
  }
  return tmp;
}

// Retail 0x800D0490, 0xB8 = 46 insns: `reserve` growing to `slwi r3,size,3` bytes, copying
// `[begin(), end())` through `fn_800D0548` into it with the two iterators built on the stack
// (`stw r6,12(r1)` .. `stw r0,20(r1)`), freeing the old buffer, then storing the new pointer at
// +12 and the new capacity at +8. The pointer walk between the copy and the `Free` is
// `destroy(mItems, mItems + mCount)` over a `pair<TUniqueId, float>` that is not registered as
// trivially destructible, which is what keeps that loop in retail's object.
extern "C" void fn_800D0490(SUniqueIdFloats* self, int size) {
  if (size <= self->mCapacity) {
    return;
  }
  SUniqueIdFloats::value_type* newData;
  self->mAllocator.allocate(newData, size);
  fn_800D0548(self->begin(), self->end(), newData);
  for (SUniqueIdFloats::iterator it = self->begin(); it != self->end(); ++it) {
    rstl::destroy(&*it);
  }
  self->mAllocator.deallocate(self->mItems);
  self->mItems = newData;
  self->mCapacity = size;
}

// `fn_800D042C` (0x800D042C, 0x64 = 25 insns) is retail's out-of-line copy of one
// `CCollisionInfo`, and this unit's split is what claims 0x800D042C for it - so it is written
// out here, not in the `Collision/` unit that first needed it. See
// `include/Collision/CCollisionInfo.hpp` for why it lives outside `src/Collision/CCollisionInfo.cpp`.
//
// **Retail's copy is a flat 96-byte block move, and the memberwise spelling does not produce
// it.** Measured, not assumed: retail is 12 `lfd`/`stfd` pairs over `0x60` bytes with no
// register reuse across pairs, while `*self = other` on this layout lowers to 24 `lwz`/`stw`
// moves plus `lhz`/`lbz` for the `TUniqueId` and the two bit-fields (0xB0 bytes, and
// `CCollisionInfo`'s own copy constructor is the same shape with `lfs`/`stfs` for the six
// `CVector3f`s). The 8-byte granularity is the whole difference, and it is what MWCC emits when
// the copy is written through a **`double`-typed** 8-byte view: a `double[12]` of this size
// copies as exactly these 12 instructions, and so do a `double[2]` x 6 and 12 `double` members
// (measured on scratch files compiled with this unit's own rule flags from `build.ninja`).
// The element type has to be the 8-byte *floating* one - a `u64[12]` of the same size is also
// 0x64 bytes but comes out as `lwz` pairs - and `lfd`/`stfd` are the only load/store in retail's
// 0x64 bytes, with no `lfdu`/`stfdu`, so retail's source loaded through a `double` lvalue too.
//
// So the copy is written as the block move it is, through a 12-`double` view of the object
// rather than through the members. This is retail's own lowering, not a shortcut around the
// work: it copies all 96 bytes, `CHECK_SIZEOF(CCollisionInfo, 0x60)` in the header holds the
// view's size to the object's, and the call sites are `bl`s to an undefined symbol in retail
// too (`Collision/CCollidableSphere.o` lists `U fn_800D042C` in the retail object dump), so
// nothing but this function's own bytes depends on the spelling.
union SCCollisionInfoBlock {
  CCollisionInfo mInfo;
  double mQuads[sizeof(CCollisionInfo) / sizeof(double)];
};

// Retail 0x800D042C, 0x64 = 25 insns, 100.00% - byte-identical to retail's 0x800D042C..0x800D0490.
extern "C" void fn_800D042C(CCollisionInfo* self, const CCollisionInfo& other) {
  SCCollisionInfoBlock* dst = reinterpret_cast< SCCollisionInfoBlock* >(self);
  const SCCollisionInfoBlock* src = reinterpret_cast< const SCCollisionInfoBlock* >(&other);
  for (int i = 0; i < sizeof(CCollisionInfo) / sizeof(double); ++i) {
    dst->mQuads[i] = src->mQuads[i];
  }
}

// The four pairs at retail 0x800D0024..0x800D0438 are `rstl::reserved_vector<T, N>`'s
// `resize(count, value)` and a "fill all N from empty" wrapper around it, one pair per
// instantiation, emitted out of line into this TU. The element stride is the measurement that
// separates the four: `slwi ...,2` for `fn_800D0170` (4-byte `float`), `mulli ...,12` for
// `fn_800D0064` and `fn_800D022C` (12-byte `CVector3f`), `slwi ...,4` for `fn_800D0338`
// (16-byte), and the wrapper's literal is the instantiation's `N` (`li r4,15` at 0x800D0024 and
// 0x800D0130, `li r4,5` at 0x800D01EC and 0x800D02F8). Retail's object does not resolve these
// names, so the `extern "C"` names below are the placeholders.
//
// **`resize` is spelled out here rather than called, and the fill is
// `rstl::uninitialized_fill_n` rather than a memberwise loop** - both measured on
// `fn_800D0170`. Calling `self->resize(n, *value)` emits a weak outlined
// `resize__Q24rstl21reserved_vector<f,15>FiRCf` and leaves the `extern "C"` function as a
// 0x20-byte forwarder, which objdiff cannot pair with retail at all (`---`). And a hand-written
// `self->mBuffer[i] = *value` over a `struct { int mCount; float mBuffer[15]; }` scored 81.77%:
// it folds the `+4` of `&mBuffer[i]` into the store offset and emits `stfsu`, where retail's
// fill base is `slwi r0,count,2` / `add r5,r3,r0` / `addi r5,r5,4` - `(self + count*stride) + 4`,
// which is what `data() + mCount` on a `uchar mData[]` starting at +4 lowers to - and its body
// is `stfs`+`addi` on a walked pointer, which is `uninitialized_fill_n`'s `++cur`. With both,
// `fn_800D0170` and all three sibling fills are byte-identical to retail.
typedef rstl::reserved_vector< float, 15 > SFillFloats15;
typedef rstl::reserved_vector< CVector3f, 15 > SFillVec3s15;
typedef rstl::reserved_vector< CVector3f, 5 > SFillVec3s5;

// Retail 0x800D0338, 0xF4 = 61 insns: the same fill with a 16-byte element, one `lfs` and three
// `lwz` per element (`lfs f0,0(r5)` / `lwz r7,4(r5)` / `lwz r6,8(r5)` / `lwz r5,12(r5)`). The
// element type is this file's choice, not a measurement - the retail object does not resolve the
// symbol. `CQuaternion` (a `float` then three more words) is what fits; a placeholder struct of
// one `float` and three `uint` scored identically.
typedef rstl::reserved_vector< CQuaternion, 5 > SFillQuats5;

// Retail 0x800D0338, 0xF4 = 61 insns: `reserved_vector< CQuaternion, 5 >::resize`.
extern "C" void fn_800D0338(SFillQuats5* self, int n, const CQuaternion* value) {
  const int count = self->mCount;
  if (count == n) {
    return;
  }
  if (count <= n) {
    rstl::uninitialized_fill_n(self->data() + count, n - count, *value);
  }
  self->mCount = n;
}

// Retail 0x800D02F8, 0x40 = 16 insns: the `N = 5` wrapper over `fn_800D0338` -
// `mr r5,r4` / `li r4,5` pass the literal count and the caller's pointer straight through,
// `li r0,0` / `stw r0,0(r3)` empties the vector first, and the epilogue's `mr r3,r31` returns
// the object, so the return type is a pointer, not `void`.
extern "C" SFillQuats5* fn_800D02F8(SFillQuats5* self, const CQuaternion* value) {
  self->mCount = 0;
  fn_800D0338(self, 5, value);
  return self;
}

// Retail 0x800D022C, 0xCC = 51 insns: `reserved_vector< CVector3f, 5 >::resize`.
extern "C" void fn_800D022C(SFillVec3s5* self, int n, const CVector3f* value) {
  const int count = self->mCount;
  if (count == n) {
    return;
  }
  if (count <= n) {
    rstl::uninitialized_fill_n(self->data() + count, n - count, *value);
  }
  self->mCount = n;
}

// Retail 0x800D01EC, 0x40 = 16 insns: the `N = 5` wrapper over `fn_800D022C`.
extern "C" SFillVec3s5* fn_800D01EC(SFillVec3s5* self, const CVector3f* value) {
  self->mCount = 0;
  fn_800D022C(self, 5, value);
  return self;
}

// Retail 0x800D0170, 0x7C = 31 insns: `reserved_vector< float, 15 >::resize`. `lwz r0,0(r3)` /
// `cmpw` / `beqlr` is `mCount == n` returning immediately (the count is written at the very end,
// `stw r4,0(r3)`, so the early exit must skip it), `bgt` skips the fill when the vector is
// shrinking, and `subf.` + `ble` is `resize`'s `mCount <= count` test. MWCC unrolls
// `uninitialized_fill_n` eight wide (`srwi. r0,r7,3` / `bdnz`, then the `andi. r6,r6,7` remainder).
//
// **The value arrives by pointer, not in a register** - measured, not assumed: retail's
// `lfs f0,0(r5)` dereferences `r5`, and `uninitialized_fill_n`'s `const S& value` keeps `r5` as
// that address through the loop. A by-value parameter instead leaves the value already live in
// `f1` and shifts every subsequent allocation.
extern "C" void fn_800D0170(SFillFloats15* self, int n, const float* value) {
  const int count = self->mCount;
  if (count == n) {
    return;
  }
  if (count <= n) {
    rstl::uninitialized_fill_n(self->data() + count, n - count, *value);
  }
  self->mCount = n;
}

// Retail 0x800D0130, 0x40 = 16 insns: the `N = 15` wrapper over `fn_800D0170`.
extern "C" SFillFloats15* fn_800D0130(SFillFloats15* self, const float* value) {
  self->mCount = 0;
  fn_800D0170(self, 15, value);
  return self;
}

// Retail 0x800D0064, 0xCC = 51 insns: `reserved_vector< CVector3f, 15 >::resize` -
// `mulli r0,r6,12` is the 12-byte stride, and the three `lfs f2,f1,f0` of the value are hoisted
// above the trip-count guard.
extern "C" void fn_800D0064(SFillVec3s15* self, int n, const CVector3f* value) {
  const int count = self->mCount;
  if (count == n) {
    return;
  }
  if (count <= n) {
    rstl::uninitialized_fill_n(self->data() + count, n - count, *value);
  }
  self->mCount = n;
}

// Retail 0x800D0024, 0x40 = 16 insns: the `N = 15` wrapper over `fn_800D0064`.
extern "C" SFillVec3s15* fn_800D0024(SFillVec3s15* self, const CVector3f* value) {
  self->mCount = 0;
  fn_800D0064(self, 15, value);
  return self;
}

// Retail 0x800CF02C, 0x84 = 33 insns: the fourth link of the teardown chain, and the only one
// that takes a **`rstl::vector`** rather than a raw object. `lwz r0,4(r30)` / `lwz r3,12(r30)` /
// `slwi r0,r0,3` / `add r0,r3,r0` is the empty element walk `rstl::destroy(begin(), end())` over
// 8-byte elements, and `rstl::destroy`'s two `pointer_iterator` arguments are what the four
// `stw r3,20(r1)`..`stw r3,8(r1)` stores are - the same stack-built iterator pair
// `fn_800D0490` passes to `fn_800D0548`. **Calling `rstl::destroy(begin(), end())` is what reaches
// it**: an explicit `for (it = begin(); it != end(); ++it) rstl::destroy(&*it);` loop emits the
// same walk but no stack temporaries, a 16-byte frame instead of 32, and scores 81.58%. Adding
// `mItems = nullptr` / `mCount = 0` / `mCapacity = 0` after the free - retail has no such stores -
// costs 87.88%.
extern "C" void* fn_800CF02C(void* self, short deleting) {
  if (self != nullptr) {
    SUniqueIdFloats* v = static_cast< SUniqueIdFloats* >(self);
    rstl::destroy(v->begin(), v->end());
    CMemory::Free(v->mItems);
    if (deleting > 0) {
      CMemory::Free(self);
    }
  }
  return self;
}

// Retail 0x800CEF2C..0x800CF02C, a four-link out-of-line teardown chain declared descending
// by retail offset (`fn_800CF02C`, `fn_800CEFD8`, `fn_800CEF84`, `fn_800CEF2C`). They are global
// symbols (`config/G2ME01/symbols.txt:3681-3684`, no `scope:local`) and `fn_800CEF2C`/
// `fn_800CEF84` are also called from `CGrappleArm` and `CPlayerGunBase`, so they are member
// teardown helpers promoted to extern linkage, not statics.
//
// All four share one shape, measured instruction by instruction from the disassembly:
// `stwu/mflr/stw/stw r31/stw r30`, `mr r31,r4` (the flag), `mr. r30,r3` (the object, which sets
// CR0 so the opening `beq` is the null test), the body, then `extsh. r0,r31` + `ble` and a single
// exit. Three consequences, each measured on the identical chain in `CPhysicsActor.cpp`
// (`fn_800EB944` there, 25/25 bytes):
//   - the flag is a **sign-extended halfword** branched on **sign**, so the parameter is `short`
//     and the test is `deleting > 0`; a `bool` emits `clrlwi.`/`beq` instead of the `extsh.`;
//   - the return is `void*` and there is **one exit** returning the object (retail's epilogue is
//     a single `mr r3,r30` before the reloads), so `void` drops that instruction and an early
//     `return self` adds one back;
//   - each link passes a **literal** -1 or 1 - for the next link's flag, not a stored value.
//
// `fn_800CEFD8` (0x800CEFD8, 0x54 = 21 insns) frees the pointer at +12 unconditionally and the
// object only when `deleting > 0`.
extern "C" void* fn_800CEFD8(void* self, short deleting) {
  if (self != nullptr) {
    CMemory::Free(*reinterpret_cast< void** >(reinterpret_cast< char* >(self) + 12));
    if (deleting > 0) {
      CMemory::Free(self);
    }
  }
  return self;
}

// `fn_800CEF84` (0x800CEF84, 0x54 = 21 insns): `lwz`-free, so the first `Free` is the whole call -
// the chain link is `fn_800CEFD8(self, -1)`.
extern "C" void* fn_800CEF84(void* self, short deleting) {
  if (self != nullptr) {
    fn_800CEFD8(self, -1);
    if (deleting > 0) {
      CMemory::Free(self);
    }
  }
  return self;
}

// `fn_800CEF2C` (0x800CEF2C, 0x58 = 22 insns): dereferences +0 before delegating, so it is the
// outermost link and takes the pointer *stored in* its object.
extern "C" void* fn_800CEF2C(void* self, short deleting) {
  if (self != nullptr) {
    fn_800CEF84(*reinterpret_cast< void** >(self), 1);
    if (deleting > 0) {
      CMemory::Free(self);
    }
  }
  return self;
}

// `fn_800CD4B8` (0x800CD4B8, 0x98 = 38 insns) and `fn_800CD460` (0x800CD460, 0x58 = 22 insns) are the
// last two links of the teardown chain that starts at `fn_800CEF2C`, and the only two in it that
// take **a raw `void*` receiver with a flag byte at +0 and a block pointer at +12** rather than one
// of the `rstl` shapes the four above carry.
//
// `fn_800CD4B8` is the block teardown: it null-tests the receiver (`mr. r30,r3` / `beq`, so the
// argument is tested with `mr.` and not a separate `cmplwi`), and when the pointer at +12 is
// non-null it takes one of two release paths. Which one is **bit 4 of the flag byte** -
// `rlwinm. r0,r0,27,31,31` is retail's one-instruction `flags & 0x10` - and the two paths are
// `CMemory::Free(p)` and MWCC's small-block allocator free `fn_8033D2F4(p)`. Both are called with
// **the pointer at +12**, not with `self`: `lwz r3,12(r30)` happens before the test and neither
// call reloads `r30` into `r3`.
//
// The two byte stores after that are the allocator's reference count on the flag byte, and they
// are why this body is the one in the unit whose shape took the most spelling: the field is read
// out with `rlwinm r3,r4,30,30,31`, decremented with `addi r0,r3,-1`, and put back with an
// `rlwimi`, then the same field is re-read with `rlwinm.` (which is what sets CR0 for the `beq`)
// and a second `rlwimi` raises a further bit when the count is non-zero.
//
// **This is a C++ bit-field, and the mask/shift spelling that used to be here cannot reach it.**
// Writing the same three operations as `uchar` arithmetic is measurably different code: it emits
// `rlwinm. r0,r0,0,27,27` (a masked `andi.`, SH=0) where retail has `rlwinm. r0,r0,27,31,31`
// (SH=27, a rotate into the sign bit), `rlwimi r4,r0,4,26,27` where retail has `,2,28,29`, and a
// plain `ori r0,64` + `stb r0` where retail has `rlwimi r3,r3,1,26,26` + `stb r3`. Three
// instructions, 96.32%.
//
// The byte layout is not guessed - each field's position is what reproduces retail's SH/MB/ME
// triple, measured by compiling candidates with this unit's own mwcceppc 2.7 and comparing the
// emitted bytes against retail (`GC/2.7`, the flags read out of `build.ninja`, so the comparison
// is against the same compiler the build uses):
//
//   - **bit 2**, 1-bit: the `if` test. A 1-bit field at byte bit 2 emits `5400dfff`, retail's
//     exact word. The mask `(x & 0x40)` emits rotate 26 and `(x & 0x10)` emits rotate 28, one bit
//     off in both directions - see the same measurement on `SMorphBallPathFlags` below.
//   - **bits 4..5**, 2-bit: the count. Sweeping the field's bit offset, a 2-bit field emits
//     `rlwinm SH=26+off MB=30 ME=31` to read it and `rlwimi SH=(2+off<0?…) MB=24+off ME=25+off`
//     to write it back; **off=4** is the only offset that produces retail's `SH=30,MB=30,ME=31`
//     and `SH=2,MB=28,ME=29`. `f->mTwo = (f->mTwo - 1) & 3;` is the whole expression - the `& 3`
//     is redundant for the compiler and makes no difference to the emitted bytes.
//   - **bit 3**, 1-bit: the *source* of the final `rlwimi`. Retail's is `rlwimi r3,r3,1,26,26`
//     with **src == dest**, which no literal store produces (`f->mB2 = 1` emits SH=5 with a
//     separate `li r0,1`), and no store from the byte's own bit produces either. Assigning from
//     the neighbouring field does: `f->mB2 = f->mB3;` emits SH=1, MB=26, ME=26 with rA == rS.
//
// Bits 0, 1, 6 and 7 are declared as unnamed padding so the three real fields land where retail
// puts them; they are not read or written here, and the file's other accessors are the only other
// things that touch this byte.
//
// `fn_800CD460` is the same chain link as `fn_800CEF84` above, with `addi r3,r30,24` in place of
// `fn_800CEFD8(self,-1)`: it hands the **receiver's word at +0x18** to `fn_800CD4B8` with the
// literal -1 and then frees itself on the flag, which is why it is written out here under the
// retail name rather than shared with `fn_800CEF84`.
extern "C" void fn_8033D2F4(void* p);

// The allocator's flag byte at +0. Only the three fields retail touches are declared; the rest is
// padding so those three land on the bits the measurements above pin them to. The names say what
// the code *does*, not what retail called it: bit 2 selects the free path, bit 3 is the value bit 2
// is refreshed from, and bits 4..5 are the count. Nothing here establishes what the byte means
// outside this function.
struct SMorphBallAllocFlags {
  unsigned mPad0 : 1;    // +0 bit 0
  unsigned mPad1 : 1;    // +0 bit 1
  bool mFreePath : 1;    // +0 bit 2 - tested; selects CMemory::Free over fn_8033D2F4
  bool mValueSrc : 1;   // +0 bit 3 - the source of the final rlwimi
  unsigned mCount : 2;   // +0 bits 4..5 - decremented, then tested
  unsigned mPad6 : 1;    // +0 bit 6
  unsigned mPad7 : 1;    // +0 bit 7
};

extern "C" void* fn_800CD4B8(void* self, short deleting) {
  if (self != nullptr) {
    void* p = *reinterpret_cast< void** >(reinterpret_cast< char* >(self) + 12);
    if (p != nullptr) {
      SMorphBallAllocFlags* flags = static_cast< SMorphBallAllocFlags* >(self);
      if (flags->mFreePath) {
        CMemory::Free(p);
      } else {
        fn_8033D2F4(p);
      }
      flags->mCount = (flags->mCount - 1) & 3;
      if (flags->mCount != 0) {
        flags->mFreePath = flags->mValueSrc;
      }
    }
    if (deleting > 0) {
      CMemory::Free(self);
    }
  }
  return self;
}

extern "C" void* fn_800CD460(void* self, short deleting) {
  if (self != nullptr) {
    fn_800CD4B8(reinterpret_cast< char* >(self) + 24, -1);
    if (deleting > 0) {
      CMemory::Free(self);
    }
  }
  return self;
}

// `fn_800CD244` (0x800CD244, 0x118 = 70 insns) and `fn_800CD35C` (0x800CD35C, 0x104 = 65 insns) are
// the **two drivers of the spider-ball path parser**, and two of the five names `tools/flip_test.sh`
// reported `undefined:` for this unit before they were written (`fn_800CD244`, `fn_800CD35C`,
// `fn_800C33DC`, `fn_800C88C0` and `CAnimRes::kDefaultCharIdx`).
//
// Neither is a `CMorphBall` member: both take a *path* in `r3` and a *cursor* in `r4`, and every
// call site agrees. `CMorphBall::FindClosestSpiderBallWaypoint` (0x800CCFFC/0x800CCFF0) passes its
// two stack locals at `r1+848` and `r1+784` to each of them and nothing else, and the second one is
// guarded by bit 6 of a local flag byte (`rlwinm. r0,r0,25,31,31` at 0x800CCFA8).
//
// Both are the same loop over the same two workers, and the workers are the pair in the
// **unclaimed `.text` gap** `auto_03_80257AF8_text.o` covers (`nm` over every object in
// `build/G2ME01/obj` finds both of them only there, and `build.ninja` links that object into
// `main.dol`, so the DOL needs nothing from us - the host link does, and the definitions are at
// `src/MetroidPrime/PortGlobals.cpp` beside `fn_8033D2F4` for exactly the reason
// `fn_80045E18` / `__as__6CLightFRC6CLight` are):
//
//   fn_80258790  0x80258790, 0x14C - decode up to four opcodes out of that array and dispatch each
//                through a 16-entry jump table; returns a packed 32-bit value.
//   fn_802588DC  0x802588DC, 0x94  - advance the cursor past one waypoint: bump `mIndex`, set
//                `mNext`, fold the waypoint's flag into the cursor's flag byte.
//
// The cursor's word at +0x20 is an **index into a halfword array**, and that is measured rather
// than assumed: every load of it in both workers is `slwi rX,rX,1` followed by `lhz`/`lhzx` on
// `path->mWaypoints` (`fn_80258790` at 0x802587A0/0x802587AC, `fn_802588DC` at
// 0x802588E8/0x802588EC), and one waypoint record spans **36 halfwords**: `fn_800CD244` advances the
// index by 36 (`addi r0,r3,36`) and then builds a `CPlane` out of the three `CVector3f` at the
// record's +0, +12 and +24 and stores its four floats at +56. The record is therefore 0x48 bytes,
// which is what `SMorphBallWaypoint` below declares - and its +0x28/+0x2C pair is retail's
// `CMaterialFilter`, which `FindClosestSpiderBallWaypoint` hands straight to
// `CMaterialFilter::Passes` (0x800CD018) without a copy.
//
// `fn_800CD35C` is the same two drivers with the record-consuming tail of `fn_800CD244` dropped: it
// runs the workers until `mRemaining` is non-zero and returns `mNext`, and its source here is
// written as the tail call retail's last instruction is. It is **not** byte-identical to retail -
// see the note on the loop shape below the definition.
//
// The flag byte is written twice in `fn_800CD244` and that is the whole of its flag handling:
// `cntlzw` / `rlwimi r0,r3,2,24,24` / `stb` folds "the last waypoint is consumed" into it, and
// `rlwinm. r0,r0,26,31,31` / `beq` tests bit 5 to decide whether the plane is built at all. The
// `uchar = bool` store is MWCC's own: `CVisorFlare::UpdateFrustum` (100.00% in this build) shows
// the identical four instructions for `mOutsideFrustum = !PointInFrustumPlanes(pos)`.
struct SMorphBallWaypoint {
  CVector3f mV0;                   // +0x00
  CVector3f mV1;                   // +0x0C
  CVector3f mV2;                   // +0x18
  unsigned int mPad24;             // +0x24
  unsigned int mMaterialFilter[2]; // +0x28, retail's CMaterialFilter
  unsigned int mPad30[2];          // +0x30
  CPlane mPlane;                   // +0x38
};

struct SMorphBallPath {
  unsigned char mPad00[0x24];
  unsigned short* mWaypoints; // +0x24
};

// **The flag byte at +0x18 has exactly two declared one-bit fields, and that is load-bearing: do
// not "fill in" the bits retail's other two writers touch.** Four measurements, all in this build:
//
//   - the store: mwcceppc writes a bool into a one-bit field with `lbz` / `cntlzw` /
//     `rlwimi r0,rX,<SH>,24+bit,24+bit` / `stb`, and for the value retail has in the register
//     (`cntlzw`, 0..32) the shift field that comes out is `<SH>` = 2 for bit 0. Retail's is
//     `rlwimi r0,r3,2,24,24`, so the written field is bit 0. A store of a literal `1` into the same
//     field emits `rlwimi r0,r4,7,24,24` instead - the shift follows the value, which is why the
//     field cannot be identified from the shift alone.
//   - the test: mwcceppc tests a one-bit field with `rlwinm. r0,r0,31-bit,31,31`, so retail's
//     `rlwinm. r0,r0,26,31,31` is a read of **bit 5**.
//   - the allocation: it is **not** declaration order over the fields a function uses. Writing all
//     six fields of a six-field group allocates bits 0..5 in declaration order; reading only the
//     last of them allocates bit 1, and `(void)`-ing the four in between changes nothing (all three
//     measured, throwaway probes, deleted). Reading the last field of a **two**-field group
//     allocates bit 5 - `31 - 5 = 26`, which is retail's rotate.
//   - the alternative, a mask on the byte, cannot work: `(x & 0x10)`, `(x & 0x20)` and `(x & 0x40)`
//     emit rotates 28, 27 and 26 - one bit lower than the mask in every case, on a byte member, a
//     32-bit member and a `unsigned char*` dereference alike. So the mask that would produce
//     retail's rotate 26 is `0x40`, which is not the bit retail tests, and the field is the
//     honest spelling that produces retail's rotate.
//
// The bits retail's other two writers touch - bit 1 (`fn_802588DC`'s `rlwimi r0,r6,1,25,25`) and
// bit 6 (`FindClosestSpiderBallWaypoint`'s `rlwimi r0,r3,7,24,24`) - are therefore **not declared
// here**. They are left undeclared rather than guessed at, because declaring them is exactly the
// change that moves `mBuildPlane` off bit 5 and costs this function its match.
struct SMorphBallPathFlags {
  bool mLastWaypoint : 1; // +0x18 bit 0, written by fn_800CD244
  bool mBuildPlane : 1;   // +0x18 bit 5, read by fn_800CD244
};

struct SMorphBallPathCursor {
  unsigned int mPad00;        // +0x00
  unsigned int mCommand;      // +0x04, written by fn_80258790
  unsigned int mCommandArg;   // +0x08, written by fn_80258790
  unsigned int mPad0C;
  unsigned int mField10;      // +0x10, written by fn_80258790
  unsigned int mField14;      // +0x14, written by fn_80258790
  SMorphBallPathFlags mFlags; // +0x18
  unsigned char mPad19[3];
  SMorphBallWaypoint* mNext;  // +0x1C
  unsigned int mIndex;        // +0x20, a halfword index into path->mWaypoints
  unsigned int mRemaining;    // +0x24, waypoints still to be produced
  unsigned int mCommands;    // +0x28, commands still to be decoded
  unsigned int mCounter;      // +0x2C
  unsigned short* mOut;       // +0x30
};

extern "C" unsigned int fn_80258790(void* path, void* cursor);
extern "C" void fn_802588DC(void* path, void* cursor);

// **The 34 instructions retail has and this does not are two more copies of the worker block and
// their two guards, then the tail call** - retail runs the body three times before it tail-calls
// itself (0x800CD38C/0x800CD3C8/0x800CD40C, then `bl fn_800CD35C` at 0x800CD444), and MWCC here
// emits one copy and the call. It is not a register-allocation difference and it is not a loop
// unroll of anything this source says: mwcceppc unrolls a `while` whose trip count it knows (a
// `for` with a constant bound of 4 comes out as four copies, measured) and rotates one it does not
// (a backward branch, measured), and it does not inline recursion at all - `static` or
// `extern "C"`, tail-position or not (measured, throwaway probes, deleted). Five spellings of this
// loop were measured and are listed in `docs/goal-notes/cmorphball-unclaimed-80258790-802588dc.md`
// with their scores; the best is the tail call below at 47.69%.
extern "C" SMorphBallWaypoint* fn_800CD35C(SMorphBallPath* path, SMorphBallPathCursor* cursor) {
  if (cursor->mRemaining != 0) {
    return cursor->mNext;
  }
  if (cursor->mCommands == 0) {
    fn_80258790(path, cursor);
  }
  cursor->mCommands = cursor->mCommands - 1;
  fn_802588DC(path, cursor);
  return fn_800CD35C(path, cursor);
}

extern "C" SMorphBallWaypoint* fn_800CD244(SMorphBallPath* path, SMorphBallPathCursor* cursor) {
  if (cursor->mRemaining != 0) {
    SMorphBallWaypoint* waypoint =
        reinterpret_cast< SMorphBallWaypoint* >(path->mWaypoints + cursor->mIndex);
    cursor->mRemaining = cursor->mRemaining - 1;
    cursor->mFlags.mLastWaypoint = cursor->mRemaining == 0;
    cursor->mIndex = cursor->mIndex + 36;
    if (cursor->mFlags.mBuildPlane) {
      waypoint->mPlane = CPlane(waypoint->mV0, waypoint->mV1, waypoint->mV2);
      cursor->mCounter = cursor->mCounter - 1;
      if (cursor->mCounter == 0) {
        *cursor->mOut = 1;
      }
    }
    return waypoint;
  }
  if (cursor->mCommands == 0) {
    fn_80258790(path, cursor);
  }
  cursor->mCommands = cursor->mCommands - 1;
  fn_802588DC(path, cursor);
  return fn_800CD244(path, cursor);
}

// `fn_800C93B0` (0x800C93B0, 0x48 = 18 insns) and `fn_800C9380` (0x800C9380, 0x30 = 12 insns) are the
// pair retail emits for **copying a light into a caller-supplied 0x50-byte object and registering it
// with the scene**, and neither is a `CMorphBall` member: both take the destination in `r3` and the
// source in `r4`, which is what every call site does - `CMorphBall::UpdateBallLight` passes
// `r3 = r1+484` (a stack local it has just built) and `r4` = the `CActorLights` light it read out of
// the state manager (two `bl fn_800C9380` at 0x800C4CEC and 0x800C4D38), and the four sites in
// `auto_03_8021FABC_text.o` (`fn_8021FBCC`, unclaimed range) all pass a stack local as `r3`.
//
// `fn_800C9380` is the wrapper: it copies `r3` into `r31`, calls `fn_800C93B0` **without touching
// `r4`** - the source stays in the register the caller put it in - and returns `r3 = r31`, the
// destination, from one exit. That last `mr r3,r31` is why the return type is `void*`: written
// `void` the store back drops out of the epilogue.
//
// `fn_800C93B0` branches on a **byte at +0x50**, the "is in the scene" flag, and the two branches are
// two different 0x50-byte copies:
//   - flag clear: `bl fn_80045E18` (0x80045E18), which is ten `lfd`/`stfd` pairs and so copies the
//     whole 0x50 bytes **including** the flag word's neighbourhood, then `li r0,1` / `stb r0,80(r31)`
//     sets the flag. This is the first, registering, copy.
//   - flag set: `bl __as__6CLightFRC6CLight` (0x80046384, retail's weak `CLight` copy-assign helper),
//     which copies 0x4C bytes plus the byte at +0x4C and **stops short of +0x50** - it leaves the
//     flag alone, which is the whole difference between the two branches.
// Both callees are in the unclaimed range `auto_03_80045CDC_text.o` and are referenced by their
// retail names, so they are declared here rather than modelled: `CLight` has no header in this port
// (`include/Kyoto/` has no `CLight.hpp`), and the objects these two copy are the DOL's own bytes.
extern "C" void fn_80045E18(void* dst, const void* src);
extern "C" void __as__6CLightFRC6CLight(void* dst, const void* src);

extern "C" void fn_800C93B0(void* self, const void* src) {
  unsigned char* inScene = reinterpret_cast< unsigned char* >(self) + 0x50;
  if (*inScene == 0) {
    fn_80045E18(self, src);
    *inScene = 1;
  } else {
    __as__6CLightFRC6CLight(self, src);
  }
}

extern "C" void* fn_800C9380(void* self, const void* src) {
  fn_800C93B0(self, src);
  return self;
}

// `fn_800C084C` (0x800C084C, 0x74 = 29 insns) is retail's out-of-line
// `rstl::reserved_vector<EWakeEffectIndex, 64>::resize`, called once from
// `InitializeWakeEffects` (`bl 800c084c` at 0x800C05F0 - the only call site in the DOL,
// confirmed with `tools/who_calls.py 0x800C084C`) to size the wake-effect table to 64.
//
// **It is written out under an `extern "C"` name, like the four `fn_800D0xxx` fills above,
// because retail's object does not resolve the mangled name** (objdiff never pairs the two,
// so the function can only score by being emitted under the name retail used). Calling
// `self->resize(n, *value)` instead emits a weak
// `resize__Q24rstl21reserved_vector<EWakeEffectIndex,64>FiRCi` and leaves nothing at
// `fn_800C084C`; that weak symbol is what the unit emitted before this change, at 0%.
//
// The body is `rstl::reserved_vector`'s own `resize` for this instantiation, and it is
// byte-identical to retail's 29 instructions:
//
//   lwz r6,0(r3) / cmpw r6,r4 / beqlr      mCount == n -> return, count never stored
//   ble +0x3c                               mCount <= n -> fill, else shrink
//   slwi r5,r6,2 / slwi r0,r4,2 / add r6,r3,r5 / add r5,r3,r0
//   addi r5,r5,4 / addi r6,r6,4             +4 is `data()`: the walk is over elements
//   addi r5,r5,4 / cmplw r5,r6 / bne        destroy(data()+n, end()) - r5 is the cursor
//   subf. r7,r6,r4 / slwi r0,r6,2 / add r6,r3,r0 / lwz r0,0(r5)
//   mtctr r7 / addi r6,r6,4 / ble          uninitialized_fill_n, one store per trip
//   cmplwi r6,0 / beq / stw r0,0(r6) / addi r6,r6,4 / bdnz
//   stw r4,0(r3) / blr
//
// **Two spellings are load-bearing and both were measured, not assumed.**
//
// 1. **The `count > n` test comes first.** Written fill-first
//    (`if (mCount <= n) fill; else destroy;`) the branch is `bgt` into a *fall-through*
//    fill and the shrink walk is emitted after it - 0x48 bytes in the wrong order, 2 of 29
//    instructions. Retail's `ble` skips forward over the shrink walk to the fill, so
//    `destroy` is the fall-through. This is the same finding as
//    `GetGravityAcceleration` in `docs/goal-notes/cmorphball-three-tweak-bodies.md`.
//
// 2. **The destroy walk's register pair is `begin` in r5 and `end` in r6**, and only
//    `rstl::destroy(self->begin() + n, self->end())` produces that: both `destroy` calls
//    with `data() + n` / `data() + count` spelled out, and both with named locals, put
//    `begin` in r6 and swap the pair (measured, 2 of 29). `begin()` and `end()` build the
//    two iterators from the same `data() + mCount` expression, which is what fixes the
//    allocation.
//
// The element is a class, not one of the `RSTL_DECLARE_TRIVIALLY_CONSTRUCTIBLE` types -
// see the comment on `EWakeEffectIndex`. That is what keeps the fill's
// `construct`'s placement-new null test and stops the 8-wide unroll, and it is the whole
// difference between this body and retail's `fn_800D0170`.
extern "C" void fn_800C084C(SWakeEffectIndices* self, int n, const EWakeEffectIndex* value) {
  const int count = self->mCount;
  if (count == n) {
    return;
  }
  if (count > n) {
    rstl::destroy(self->begin() + n, self->end());
  } else {
    rstl::uninitialized_fill_n(self->data() + count, n - count, *value);
  }
  self->mCount = n;
}

// The nine `fn_800Cxxxx` / `fn_800D0640` bodies below are `TReservedAverage<T, N>`'s and
// `rstl`'s own out-of-line members, one per instantiation, emitted into this TU, and every one of
// them is written out under an `extern "C"` name for the reason `fn_800C084C` above gives:
// **retail's object does not resolve the mangled name**, so a call emits a weak
// `AddValue__22TReservedAverage<f,15>FRCf` / `erase__4rstl18reserved_vector...` and leaves the
// `extern "C"` name as a forwarder objdiff cannot pair with retail at all. The bodies are the
// headers' own, transcribed - `include/Kyoto/TReservedAverage.hpp`,
// `include/rstl/reserved_vector.hpp` and `include/rstl/vector.hpp` - and each is byte-identical to
// retail under its retail name, which is the evidence that the headers were right and only the
// symbols were missing.
//
// **Which instantiation is which is read off the object offsets the callers pass**, and for the
// `TReservedAverage` family those line up with this header's four members exactly:
//
//   0xE74  mBallOrientationAverage   TReservedAverage<CQuaternion, 5>   0x54
//   0xEC8  mBallPositionAverage      TReservedAverage<CVector3f, 5>    0x40
//   0xF08  mLiftSpeedAverage         TReservedAverage<float, 15>      0x40
//   0xF48  mLiftControlForceAverage  TReservedAverage<CVector3f, 15>  0xB8
//
// (`bl fn_800C5070` / `bl fn_800C5024` at 0x800C46C8 / 0x800C46D8 in `Render`, with `addi
// r4,r31,3784` / `addi r4,r31,3700`; `bl fn_800C5F3C` at 0x800C5A5C and `bl fn_800C1FAC` at
// 0x800C1C84, both with `addi r4,r30,3848`; `bl fn_800C21C4` / `bl fn_800C2070` at 0x800C1C30 /
// 0x800C1C3C with `addi r3,r30,3848` / `addi r3,r30,3912`; `bl fn_800CB380` / `bl fn_800CB22C` at
// 0x800CB1D0 / 0x800CB1E8 with `addi r3,r30,3700` / `addi r3,r30,3784`; `bl fn_800C2004` at
// 0x800C1C48 with `addi r4,r30,3912`.) The `N` is the `cmpwi` immediate - 15 or 5 - so it is a
// measurement too, and it is what separates the two same-stride pairs. The **element stride is the
// measurement** (`slwi ...,2` / `mulli ...,12` / `slwi ...,4`), the same evidence the `fn_800D0xxx`
// fills rest on. The element *type* is this file's choice where the instruction stream does not
// force it, and for `fn_800CB380` it is forced: one `lfs` and three `lwz` is a `CQuaternion` (a
// `float w` then three more words) and nothing narrower.
//
// The three `GetValue` bodies (0x800C5024 / 0x800C5070 / 0x800C5F3C) are one shape, and the shape
// is what the call convention forces: the return value is a **`rstl::optional_object<T>`**, whose
// `m_valid` byte sits immediately after the `sizeof(T)` payload
// (`include/rstl/optional_object.hpp`), and each one is
//
//   lwz r0,0(r4) / cmpw r5,r0 / blt -> in range:  `li r5,1; stb r5,<off>(r3)` then the element copy
//                                        out of range: `li r0,0; stb r0,<off>(r3); blr`
//
// so the flag is at +16 for the 16-byte element, +12 for the 12-byte one and +4 for the float one,
// and the early exit is a **separate `blr`**, i.e. `return optional_object_null()` written as an
// `if`, not as a ternary. `optional_object`'s converting constructor is
// `m_valid(true) { construct<T>(m_data, item); }`, which is what puts the `stb 1` *before* the
// element copy rather than after it.
//
// **The branch direction is load-bearing, and this is the one spelling that reached 100% on all
// three** (measured): the *out-of-range* return has to be the **`if` arm and the value the `else`
// arm**, i.e.
//
//   if (index >= self->mCount) { return rstl::optional_object_null(); } else { return self->data()[index]; }
//
// Writing it the other way round - `if (index < self->mCount) { return ...; } return null;` - emits
// `bge` over the value path with the null return as the fall-through, where retail has `blt` into
// it, and scores 53.08% on the float one (13 instructions, 6 of them in the wrong order). The
// `?:` spelling scores the same 53.08%: mwcceppc normalises the conditional expression back to the
// same branch it would have chosen for the `if`. `fn_800C1FAC` and `fn_800C2004` (the `GetAverage`
// pair) need the same polarity, `mCount == 0` first, for the same reason.
//
// `GetAverage` is distinguished from `GetValue` by its body, not by its offset: it delegates. The
// `float` one calls `GetAverageValue<float>` (0x80008B60, already emitted by `main.o` and
// `CVisorFlare.o`) and has no index argument, and the result arrives in `f1`, which is why the store
// is `stfs f1` and not `stfs f0`. The `CVector3f` one calls what is retail's `fn_8001C95C`, and
// its three argument registers are what identify it: `addi r3,r1+8` is a stack temporary, `addi
// r4,r4,4` is `data()`, and the count is already in `r5` from the `lwz r5,0(r4)` the guard did. A
// 12-byte class return goes through a hidden out-pointer in `r3` under this ABI, so
// `GetAverageValue<CVector3f>(data, mCount)` - `include/Kyoto/TAverage.hpp`'s own template,
// returning `T` by value - is the call, and it is emitted **weak and local** from that header, so
// it costs the port's undefined count nothing. (`fn_8001C95C` is retail's name for that same
// instantiation; it is undefined tree-wide, so calling *it* would have grown the port's undefined
// list, which is the failure this item's `reason` records for `fn_800CD460`.)
typedef rstl::optional_object< CQuaternion > SOptionalQuat;
typedef rstl::optional_object< CVector3f > SOptionalVec3;
typedef rstl::optional_object< float > SOptionalFloat;

// Retail 0x800D0640, 0x8C = 35 insns: the fifth link of the teardown chain that starts at
// `fn_800CEF2C`, and the only one whose dead work is a **loop** rather than a pair of stores.
//
// It is `fn_800CF02C` (written out above, 100.00%) with the `rstl::vector`'s own `mItems` pointer
// and `Free(mItems)` dropped: the walk runs off the **object itself** (`lwz r6,0(r31)` is
// `mCount` at +0, and the cursor starts at 0 rather than at a loaded buffer), and the
// `extsh. r0,r4` / `ble` gate on the `short deleting` flag leads straight to `Free(self)`. So the
// receiver is a **`rstl::reserved_vector`-shaped** object - a count at +0 and inline elements -
// and the loop is `rstl::destroy(begin(), end())` over an 8-byte element whose destructor is
// trivial, which is why the trip count and the unrolled `bdnz` skeleton survive with an **empty
// body**: exactly the dead walk `fn_800CF02C` and `fn_800C8D2C` above also have, and for the same
// measured reason.
//
// The `srwi r0,r0,3` / `mtctr` / `addi r3,r3,8` / `bdnz` unrolled block followed by the
// `subf r0,r3,r6` / `mtctr` / `cmpw r3,r6` / `bge` / `bdnz` remainder is MWCC's own 8-wide unroll
// prologue for that walk, and the element stride is 8 - the same `pair<TUniqueId, float>` this
// file's `SUniqueIdFloats` already names, which is why this one is written over that type rather
// than a fresh placeholder.
//
// **The walk is over the object's own bytes from +0, not over a loaded `mItems` pointer**, and that
// is the measurement that fixes the spelling: retail's cursor is `li r3,0` and steps `addi
// r3,r3,8`, compared against the `lwz r6,0(r31)` count directly, with no `lwz` of a buffer and no
// `add buffer,base`. An `rstl::vector` walk (as in `fn_800CF02C` above) would have to load `mItems`
// at +12 first. So the receiver is an **inline-storage** container whose count is at +0, and the
// loop is `reserved_vector::destroy_elements`' own **index** loop, whose `T* ptr = data()` local
// disappears precisely because `rstl::destroy` compiles to nothing for this element - the
// induction variable is left holding a plain index.
//
// `reserved_vector::destroy_elements` is private, so `SDead8Owner` re-spells it as a public method
// rather than reaching inside; the body is the header's own, transcribed.
typedef rstl::reserved_vector< SUniqueIdFloats::value_type, 8 > SDead8;

struct SDead8Owner : public SDead8 {
  void DestroyElements() {
    SDead8::value_type* ptr = data();
    for (int i = 0; i < this->mCount; ++i) {
      rstl::destroy(&ptr[i]);
    }
  }
};

extern "C" void* fn_800D0640(void* self, short deleting) {
  if (self != nullptr) {
    SDead8Owner* v = static_cast< SDead8Owner* >(self);
    v->DestroyElements();
    if (deleting > 0) {
      CMemory::Free(self);
    }
  }
  return self;
}

// `fn_800C88C0` (0x800C88C0, 0x5C = 23 insns) and `fn_800C33DC` (0x800C33DC, 0x5C = 23 insns)
// are the **deleting destructors of the two `SEffect`-shaped helpers `CMorphBall` builds on its
// stack**, and they are two of the three unwritten functions that stopped `tools/flip_test.sh` for
// this unit (measured this run: the link failed with `undefined: 'fn_800C88C0'`, `'fn_800C33DC'`,
// `'fn_800CD35C'`, `'CAnimRes::kDefaultCharIdx'`).
//
// The evidence that they are destructors of *this* unit's two local helpers, not of a `CMorphBall`
// member: **each stores two vtable pointers and nothing else.** `fn_800C88C0` stores
// `lbl_803B36F0` and then `lbl_803B1750`; `fn_800C33DC` stores `lbl_803B36FC` and then
// `lbl_803B1750`. `lbl_803B36F0`/`lbl_803B36FC` are the two `.data` objects
// `config/G2ME01/symbols.txt:18099-18100` (`size:0xC` each, an unclaimed `.data` gap - retail's
// bytes, which `tools/dol_read.py 0x803B36F0 0x10` confirms are `0, 0, 0x800C88C0, 0` and
// `0, 0, 0x800C33DC, 0`, i.e. **each one's third word is its own destructor**, the offset-to-top /
// RTTI pair MWCC puts in front of the first slot). `lbl_803B1750`
// (`symbols.txt:17971`, `size:0x10`) is `0, 0, 0x8000DF48, 0` - `fn_8000DF48` is the base
// destructor, and `tools/dis.sh 0x8000DF48 0x48` is the same 18-instruction shape: store the vtable,
// then `CMemory::Free` on the flag. So the **second store is the inlined base destructor and the
// first is the derived one**, which is why the second `beq` at +0x34 exists: `fn_800CEF84` and
// `fn_800CEF2C` above have the identical two-vtable-store + flag + `Free` + `mr r3,r31` shape and
// are already at 100%.
//
// Two other things this measured rather than assumed:
//   - the parameter is `short` and the test is `deleting > 0` - `extsh. r0,r4` / `ble`, the same
//     `fn_800CEF2C` conclusion above, here with no `mr r31,r4` because retail never copies r4;
//   - **the inner null test is real and must be written.** Retail's `beq +0x34` after the first
//     store tests the *same* CR0 the opening `mr. r31,r3` set, so it is unreachable, but mwcceppc
//     emits it when the second store is inside a second `if (self != nullptr)` - which is what an
//     inlined `Base::~Base()` with the standard deleting-destructor prologue looks like in source.
//     Writing the two stores flat (measured) drops it.
//
// **These are written under their retail `extern "C"` names rather than as class destructors** for
// the reason every other `fn_` in this file gives: retail's object names them, and a real `~X()`
// would emit `__dt__<mangled>` instead - a symbol retail's object does not define. The vtables are
// referenced as *objects* (`extern "C" char[]`) and the store written by hand, which is the
// arrangement `CAudioStateWinCtor.cpp` and `CConsoleOutputWindowCtor.cpp` use for the same reason:
// the classes have no key function here, so no vtable may be emitted for them.
extern "C" char lbl_803B36FC[];
extern "C" char lbl_803B36F0[];
extern "C" char lbl_803B1750[];

extern "C" void* fn_800C88C0(void* self, short deleting) {
  if (self != nullptr) {
    *reinterpret_cast< void** >(self) = lbl_803B36F0;
    if (self != nullptr) {
      *reinterpret_cast< void** >(self) = lbl_803B1750;
    }
    if (deleting > 0) {
      CMemory::Free(self);
    }
  }
  return self;
}

extern "C" void* fn_800C33DC(void* self, short deleting) {
  if (self != nullptr) {
    *reinterpret_cast< void** >(self) = lbl_803B36FC;
    if (self != nullptr) {
      *reinterpret_cast< void** >(self) = lbl_803B1750;
    }
    if (deleting > 0) {
      CMemory::Free(self);
    }
  }
  return self;
}

// The pair at 0x800C8CE0..0x800C8DC8 is `rstl::vector<TUniqueIdFloat>::erase(iterator)` and its
// out-of-line `erase(iterator, iterator)`, called once from `UpdateDeathBall`
// (`bl fn_800C8CE0` at 0x800C898C, with `addi r4,r28,6368` = this+0x18E8, `addi r3,r1,64` and
// `addi r5,r1,60`).
//
// **The element is 8 bytes and this file's `SUniqueIdFloats` value type**, read straight off the
// copy: `lhz r0,0(r7)` / `sth r0,0(r8)` / `lfs f0,4(r7)` / `stfs f0,4(r8)` - a 2-byte `TUniqueId`
// at +0 and a `float` at +4, the same `lhz`/`lfs` pair the `fn_800D0548` comment above already
// identifies as `TUniqueId` plus a float. The vector's own layout is confirmed by retail's field
// offsets: `lwz r8,12(r4)` and `stw r9,4(r4)` are `mItems` at +12 and `mCount` at +4, which is
// `rstl::vector`'s `mAllocator` / `mCount` / `mCapacity` / `mItems` order with a 4-byte allocator,
// and the same +4 / +12 pair the caller reads back at 0x800C89A8 / 0x800C89AC.
//
// **The bodies are `rstl::vector::erase`'s own, transcribed** (`include/rstl/vector.hpp` lines
// 256-270), for the same reason as every other `fn_` above: retail's object does not resolve the
// mangled name, so a call emits a weak `erase__...` and leaves the `extern "C"` name empty.
//
// Two things in retail's 39 instructions are measurements, not assumptions:
//
//   - the leading `destroy(first, last)` walk is **empty** - `lwz r7,0(r5)` / `lwz r0,0(r6)` /
//     `stw r7,8(r1)` / `stw r0,12(r1)` / `b` / `addi r7,r7,8` / `cmplw r7,r0` / `bne` with no body -
//     and so is the `destroy(&*it)` inside the move loop. Both are the same fact as the dead walk
//     in `fn_800CF02C` above: the element's destructor is trivial, so the call is dropped and only
//     the walk survives. The **stack temporaries are not**, which is why the two iterators are
//     passed by address here;
//   - the move loop keeps `construct`'s placement-new null test (`cmplwi r8,0` / `beq`) and its
//     one-store-per-trip body, i.e. the element is **not** registered
//     `RSTL_DECLARE_TRIVIALLY_CONSTRUCTIBLE` - exactly as the `EWakeEffectIndex` comment above
//     argues for `fn_800C084C`.
//
// `fn_800C8CE0` is `erase(iterator)` = `erase(it, it + 1)`, and the store its own body does not
// read (`stw r7,8(r1)`, between the two that are passed) is the third of the dead stack slots
// this unit's `pointer_iterator` arguments keep producing.
extern "C" void fn_800C8D2C(SUniqueIdFloats::iterator* out, SUniqueIdFloats* self,
                            SUniqueIdFloats::iterator* first, SUniqueIdFloats::iterator* last) {
  rstl::destroy(*first, *last);

  const SUniqueIdFloats::iterator::difference_type tmp = *first - self->begin();
  int newCount = tmp;

  for (SUniqueIdFloats::iterator it = *last, moved = self->mItems + tmp; it != self->end();
       ++moved, ++newCount, ++it) {
    rstl::construct(&*moved, *it);
    rstl::destroy(&*it);
  }
  self->mCount = newCount;

  *out = *first;
}

// Retail 0x800C8CE0, 0x4C = 19 insns: `rstl::vector::erase(iterator)` = `erase(it, it + 1)`.
//
// Retail's three stack slots are the measurement here: `stw r7,0x8(r1)` / `stw r7,0xc(r1)` /
// `stw r0,0x10(r1)`, with `r7 = *it + 8` and `r0 = *it`, and the call passes `r5 = r1+0x10`
// (`*it`) and `r6 = r1+0xc` (`*it + 1`). So the `it + 1` temporary is **built before** the copy of
// `it`, both are spilled, and the slot at +8 is a second copy of `*it + 1` that nothing reads -
// the same dead-stack-slot pattern as `fn_800C8D2C`'s own `destroy` call above. That is what a
// `pointer_iterator` argument built from a *named* local produces; passing the dereferenced
// expression directly drops the extra spill and the frame falls to 16 bytes (73.37%).
extern "C" void fn_800C8CE0(SUniqueIdFloats::iterator* out, SUniqueIdFloats* self,
                            const SUniqueIdFloats::iterator* it) {
  SUniqueIdFloats::iterator last = *it + 1;
  SUniqueIdFloats::iterator first = *it;
  SUniqueIdFloats::iterator lastCopy = last;
  fn_800C8D2C(out, self, &first, &last);
  (void)lastCopy;
}

// **The bodies are written out here, not delegated to `TReservedAverage::AddValue`,** for the same
// reason `fn_800C084C` is: retail's object does not resolve the mangled name, so a call to
// `self->AddValue(*value)` emits a weak `AddValue__22TReservedAverage<f,15>FRCf` and leaves the
// `extern "C"` name as a 0x10-byte forwarder that objdiff cannot pair with retail at all (5.68%).
// The **body this file writes is `include/Kyoto/TReservedAverage.hpp`'s own `AddValue`,
// transcribed** - and each of the four is byte-identical to retail's under its retail name,
// which is the evidence that the header's loop is right and only the symbol was wrong.
#define CMORPHBALL_WRITE_ADD_VALUE(name, T, N)                                  \
  extern "C" void name(TReservedAverage< T, N >* self, const T* value) {        \
    if (self->size() < N) {                                                     \
      self->push_back(*value);                                                 \
    }                                                                           \
    for (int i = self->size() - 1; i > 0; --i) {                                \
      self->operator[](i) = self->operator[](i - 1);                            \
    }                                                                           \
    self->operator[](0) = *value;                                               \
  }

// Retail 0x800CB380, 0x18C = 99 insns.
CMORPHBALL_WRITE_ADD_VALUE(fn_800CB380, CQuaternion, 5)

// Retail 0x800CB22C, 0x154 = 85 insns.
CMORPHBALL_WRITE_ADD_VALUE(fn_800CB22C, CVector3f, 5)

// Retail 0x800C21C4, 0x134 = 77 insns.
CMORPHBALL_WRITE_ADD_VALUE(fn_800C21C4, float, 15)

// Retail 0x800C2070, 0x154 = 85 insns.
CMORPHBALL_WRITE_ADD_VALUE(fn_800C2070, CVector3f, 15)

#undef CMORPHBALL_WRITE_ADD_VALUE

// Retail 0x800C7674, 0x78 = 30 insns: `rstl::reserved_vector<TUniqueId, N>::erase(iterator)`.
//
// **The element is 2 bytes** and that is a measurement, not a guess: the shift loop is
// `lhz r0,2(r6)` / `sth r0,0(r6)` / `addi r6,r6,2`, i.e. a 16-bit load-store pair advancing by
// two, and the trip bound is recomputed each trip as `slwi r0,count,1` / `add r3,self,r0` /
// `addi r0,r3,2` = `end() - 1` in bytes. `TUniqueId` is the 2-byte type in this codebase.
//
// The body is `rstl::reserved_vector::erase`'s own, as `include/rstl/reserved_vector.hpp`
// already spells it - `if (it >= begin() && it < end())`, shift every element down one from `it`,
// `destroy(end() - 1)`, `--mCount`, `return it`, and `return end()` otherwise. Retail's two range
// tests are `it < data()` -> out and `it >= end()` -> out, which is the same pair with the
// operands the other way round, and the shift loop recomputes `end() - 1` **inside** the loop
// rather than hoisting it - that is the indexed `for (j = it; j < end() - 1; ++j)` spelling and
// not a pointer-walk, and it is what puts the `slwi`/`add`/`addi` bound rebuild inside the body.
//
// The `destroy(end() - 1)` is **absent from retail's 30 instructions**, which is the measurement
// that the element is trivially destructible here: `TUniqueId` is registered
// `RSTL_DECLARE_TRIVIALLY_CONSTRUCTIBLE`, so the call is dropped and only the shift remains.
typedef rstl::reserved_vector< TUniqueId, 8 > SEraseIds8;

extern "C" SEraseIds8::iterator fn_800C7674(SEraseIds8* self, SEraseIds8::iterator it) {
  if (it >= self->begin() && it < self->end()) {
    for (SEraseIds8::iterator j = it; j < self->end() - 1; ++j) {
      *j = *(j + 1);
    }
    --self->mCount;
    return it;
  }
  return self->end();
}

// Retail 0x800C5F3C, 0x34 = 13 insns: `TReservedAverage<float, 15>::GetValue(int)`.
extern "C" SOptionalFloat fn_800C5F3C(const TReservedAverage< float, 15 >* self, int index) {
  if (index >= self->mCount) {
    return rstl::optional_object_null();
  } else {
    return self->data()[index];
  }
}

// Retail 0x800C5070, 0x44 = 17 insns: `TReservedAverage<CVector3f, 5>::GetValue(int)`.
extern "C" SOptionalVec3 fn_800C5070(const TReservedAverage< CVector3f, 5 >* self, int index) {
  if (index >= self->mCount) {
    return rstl::optional_object_null();
  } else {
    return self->data()[index];
  }
}

// Retail 0x800C5024, 0x4C = 19 insns: `TReservedAverage<CQuaternion, 5>::GetValue(int)`.
extern "C" SOptionalQuat fn_800C5024(const TReservedAverage< CQuaternion, 5 >* self, int index) {
  if (index >= self->mCount) {
    return rstl::optional_object_null();
  } else {
    return self->data()[index];
  }
}

// Retail 0x800C2004, 0x6C = 27 insns: `TReservedAverage<CVector3f, 15>::GetAverage()` - the same
// shape as `fn_800C1FAC` for the 12-byte element, and the caller confirms the member: `bl
// fn_800C2004` at 0x800C1C48 in `ComputeLiftForces` with `addi r4,r30,3912` = this+0xF48, which
// is `mLiftControlForceAverage`.
//
// **Retail calls `fn_8001C95C`, not the `float` instantiation of `GetAverageValue`**, and the
// three argument registers are what identify it: `addi r3,r1,8` is a stack temporary, `addi
// r4,r4,4` is `data()`, and the count is already in `r5` from the `lwz r5,0(r4)` the guard did.
// A 12-byte class return goes through a hidden out-pointer in `r3` under this ABI, so
// `GetAverageValue<CVector3f>(data, mCount)` - `include/Kyoto/TAverage.hpp`'s own template,
// returning `T` by value - is the call, and it is emitted **weak and local** from this header, so
// it costs the port's undefined count nothing. (`fn_8001C95C` is retail's name for that same
// instantiation; it is undefined tree-wide, so calling *it* would have grown the port's undefined
// list, which is the failure this item's `reason` records for `fn_800CD460`.)
extern "C" SOptionalVec3 fn_800C2004(const TReservedAverage< CVector3f, 15 >* self) {
  if (self->mCount == 0) {
    return rstl::optional_object_null();
  } else {
    return GetAverageValue< CVector3f >(self->data(), self->mCount);
  }
}

// The four `fn_800C2070` / `fn_800C21C4` / `fn_800CB22C` / `fn_800CB380` are
// `TReservedAverage<T, N>::AddValue(const T&)`, one per instantiation, emitted into this TU - the
// same reason the accessors above are written out rather than called.
//
// Which member is which is read off the call sites' object offsets, and they line up with this
// header's four `TReservedAverage` members exactly:
//
//   fn_800C21C4  r3 = this+3848 (0xF08)  `slwi r0,r0,2`     4-byte  -> float,      mLiftSpeedAverage
//   fn_800C2070  r3 = this+3912 (0xF48)  `mulli r0,r0,12`   12-byte -> CVector3f,  mLiftControlForceAverage
//   fn_800CB22C  r3 = this+3784 (0xEC8)  `mulli r0,r0,12`   12-byte -> CVector3f,  mBallPositionAverage
//   fn_800CB380  r3 = this+3700 (0xE74)  `slwi r0,r0,4`     16-byte -> CQuaternion, mBallOrientationAverage
//
// (`bl fn_800C21C4` / `bl fn_800C2070` at 0x800C1C30 / 0x800C1C3C in `ComputeLiftForces` with
// `addi r3,r30,3848` / `addi r3,r30,3912`; `bl fn_800CB380` / `bl fn_800CB22C` at 0x800CB1D0 /
// 0x800CB1E8 with `addi r3,r30,3700` / `addi r3,r30,3784`.) The `N` is the `cmpwi` immediate - 15
// for the first two, 5 for the last two - so it is a measurement too, and it is what separates
// the two same-stride pairs. The **element stride is the measurement** that pairs them up; the
// element type is this file's choice where the instruction stream does not force it, and for
// `fn_800CB380` it is forced: one `lfs` and three `lwz` is a `CQuaternion` (a `float w` then three
// more words) and nothing narrower.
//
// The body is `TReservedAverage::AddValue` as `include/Kyoto/TReservedAverage.hpp` already spells
// it - `push_back` while `size() < N`, then shift every element up one from the top down, then
// write the new value at 0 - and retail's instruction order is that spelling's: the copy into
// `data()[mCount]` and the `++mCount` come first (`lwz r5,0(r3)` / `addi r0,r5,1` / `stw r0,0(r3)`
// after the three `stfs`), then `i = mCount - 1` is computed into a register that the shift loop
// walks backwards from (`add r8,r3,r0` / `addi r8,r8,4` is `&data()[mCount - 1]`, and the stores
// descend `-12`/`-8`/`-4` at a time), and the unroll width is MWCC's: 4-wide for the 12-byte
// elements, 8-wide for the 4-byte one.
//

// Retail 0x800C1FAC, 0x58 = 22 insns: `TReservedAverage<float, 15>::GetAverage()`.
extern "C" SOptionalFloat fn_800C1FAC(const TReservedAverage< float, 15 >* self) {
  if (self->mCount == 0) {
    return rstl::optional_object_null();
  } else {
    return GetAverageValue< float >(self->data(), self->mCount);
  }
}

// Scaffold, not a reconstructed implementation.
void CMorphBall::RenderToShadowTex(CStateManager& mgr) {
  // TODO: Gather shadow receivers and render CMorphBallShadow with the player texture.
}

// Scaffold, not a reconstructed implementation.
void CMorphBall::DrawBallShadow(CStateManager& mgr) {
  // TODO: Draw the projected ball shadow against the gathered world/actor receivers.
}

// Guessed name.
void CMorphBall::InitializeWakeEffects() {
  // Called through the out-of-line `fn_800C084C` rather than as `resize`, so the call
  // target is the name retail's object has - see that function's comment.
  fn_800C084C(&sWakeEffectForMaterial, 64, &kNoWakeEffect);
  // The four writes go through one hoisted `data()` rather than four `operator[]` calls: retail
  // copies the vector's address into its own register once (`addi r18,r5,0` at 0x800C060C, from
  // the `lbl_8040D568` pair) and stores the value 0 into a *second* one (`li r19,0` at
  // 0x800C0618), while four separate `operator[]` calls give the constant 0 the lower register
  // instead (`li r18,0` / `stw r18,32(r19)`) and cost 7 of 133 instructions. Nothing else in
  // the function moves: the 176-byte frame, both local arrays and the six pool-relative loads
  // are byte-identical either way.
  EWakeEffectIndex* const indices = sWakeEffectForMaterial.data();
  indices[kMT_Phazon] = kWEI_Phazon;
  indices[kMT_Dirt] = kWEI_Dirt;
  indices[kMT_Organic] = kWEI_Organic;
  indices[kMT_Sand] = kWEI_Sand;
  const char* effects[] = {"PhazonWake",  "PhazonWakeOrange", "DirtWake",
                           "OrganicWake", "SandWake",         "RainWake"};
  const char* groups[] = {"PhazonWake_DGRP",  "PhazonWakeOrange_DGRP", "DirtWake_DGRP",
                          "OrganicWake_DGRP", "SandWake_DGRP",         "RainWake_DGRP"};
  for (int i = 0; i < 6; ++i) {
    const CDependencyGroupToken group(gpSimplePool->GetObj(groups[i]), *gpSimplePool);
    mWakeEffects.push_back(rstl::auto_ptr< CDeferredParticleEffect >(
        rs_new CDeferredParticleEffect(gpSimplePool->GetObj(effects[i]), group)));
  }
}

// Guessed name.
void CMorphBall::ResetScrewAttackExitAnimationTimer() { mScrewAttackExitAnimationFrames = 5; }

// Guessed name.
int CMorphBall::GetScrewAttackGroundedFrames() const { return mScrewAttackGroundedFrames; }

bool CMorphBall::InScrewAttackMode() const {
  return mBallState == kBS_ScrewAttack || mBallState == kBS_ScrewAttackWallJump ||
         mBallState == kBS_ScrewAttackRecovery;
}

bool CMorphBall::IsProjectile() const { return mBallState == kBS_Projectile; }

bool CMorphBall::IsBoostShieldActive() const { return mTimeNotInBoost < 1.f; }

float CMorphBall::GetBoostChargeTimer() const { return mBoostChargeTime; }

float CMorphBall::GetTimeNotInBoost() const { return mTimeNotInBoost; }

bool CMorphBall::IsBoosting() const {
  return mBallState == kBS_Boost || mBallState == kBS_SpiderBoost;
}

// Guessed name; workspace type name is a cross-game hypothesis.
void CMorphBall::PointGenerator(const CSkinnedModel& model, const SSkinningWorkspace& workspace,
                                void* context) {
  if (context) {
    static_cast< CRainSplashGenerator* >(context)->GeneratePoints(model, workspace);
  }
}

// Scaffold, not a reconstructed implementation.
void CMorphBall::StartLandingSfx() {
  // TODO: Choose and play the landing sound from velocity and selected surface material.
}

void CMorphBall::StopSounds() {
  if (mRollSfx) {
    CSfxManager::SfxStop(mRollSfx);
    mRollSfx.Clear();
  }
  if (mSpiderSfx) {
    CSfxManager::SfxStop(mSpiderSfx);
    mSpiderSfx.Clear();
  }
  if (mDeathBallSfx) {
    CSfxManager::SfxStop(mDeathBallSfx);
    mDeathBallSfx.Clear();
  }
  if (mScrewAttackSfx) {
    CSfxManager::SfxStop(mScrewAttackSfx);
    mScrewAttackSfx.Clear();
  }
}

// Retail 0x800C0B20, 0x84 = 33 insns.
// The existing handle is stopped first (re-read off `this`, so the test and the argument are
// two loads of 0x189C), then an emitter is added at the player's own position (CActor+0x54,
// `mPosition`) in the player's own area (CEntity+4, `m_areaId`), always looped and always
// with acoustics, and the new handle lands back in the same slot. The sfx id is the only
// thing the flag byte at 0x1904 changes: `rlwinm. r0,r0,25,31,31` rotates left by 25 and
// keeps bit 31, so it tests bit 6 (`0x40`) - `x1904_40_`, the same flag `LoadMorphBallModel`
// reads. `clrlwi r4,r4,16` is the narrowing to `ushort`, and `lha r9,-16604(r2)` is
// `CSfxManager::kMedPriority` read as a `short` (0x8041E2E4).
void CMorphBall::StartScrewAttackSfx() {
  if (mScrewAttackSfx) {
    CSfxManager::SfxStop(mScrewAttackSfx);
  }
  mScrewAttackSfx = CSfxManager::AddEmitter(x1904_40_ ? 9698 : 8707, mPlayer.GetTranslation(),
                                            mPlayer.GetCurrentAreaId().Value(), true, true,
                                            CSfxManager::kMedPriority);
}

// Scaffold, not a reconstructed implementation.
void CMorphBall::UpdateMorphBallSound(float dt, CStateManager& mgr) {
  // TODO: Maintain the roll, Spider, death-ball and Screw Attack emitters.
}

// Scaffold, not a reconstructed implementation.
void CMorphBall::SelectMorphBallSounds(const CMaterialList& material) {
  // TODO: Select roll/landing sound IDs for collision material and multiplayer mode.
}

void CMorphBall::TakeDamage(float damage) {
  if (damage <= 0.f) {
    mDamageEffect = 0.f;
    mDamageEffectDecaySpeed = 0.f;
    return;
  }

  if (damage >= 20.f) {
    mDamageEffectDecaySpeed = 0.25f;
  } else if (damage > 5.f) {
    mDamageEffectDecaySpeed = 1.f - 0.75f * ((damage - 5.f) / 15.f);
  } else {
    mDamageEffectDecaySpeed = 1.f;
  }
  mDamageEffect = 1.f;
}

CMorphBall::EBombJumpState CMorphBall::GetBombJumpState() const { return mBombJumpState; }

void CMorphBall::SetBallBoostState(EBallBoostState state) { mBoostState = state; }

CMorphBall::EBallBoostState CMorphBall::GetBallBoostState() const { return mBoostState; }

void CMorphBall::SetAsProjectile(bool projectile) {
  if (projectile) {
    mBallState = kBS_Projectile;
  } else if (mBallState == kBS_Projectile) {
    mBallState = kBS_Normal;
  }
}

void CMorphBall::TouchModel(const CStateManager& mgr) const {
  mBallModel->Touch(mgr, mBallModelShader);
  if (mPlayer.GetPlayerState()->HasPowerUp(CPlayerState::kIT_SpiderBall) &&
      mSpiderBallGlassModel.get()) {
    mSpiderBallGlassModel->Touch(mgr, mSpiderBallGlassModelShader);
  }
  mLowPolyBallModel->Touch(mgr, mLowPolyBallModelShader);
}

// Retail 0x800C12AC, 0x140 = 80 insns. The empty-name test is `bl __eq__(const rstl::string&,
// const char*)` against the pooled "" at 0x803A87B2 (0x803A86F0 + 0xC2), so the comparison
// argument is the literal, not a constructed `rstl::string` - writing `rstl::string("")` builds
// the temporary on the stack, saves r30 for it and costs the whole function.
CModelData* CMorphBall::GetMorphBallModel(const rstl::string& name, float radius) {
  // Retail calls the free `rstl::operator==(const string&, const char*)` **out of line**
  // (`bl __eq__4rstlF...` at 0x800C12D8; `__eq__4rstlFRC...PCc`,
  // `config/G2ME01/symbols.txt:2447`) and tests the returned `bool` with `clrlwi. r0,r3,24` /
  // `beq`; `rstl_string_eq_c` at the top of this file is that function, written out so the
  // call is a real `bl`. The `SObjectTag` is a **by-value** local (`stw r4,8(r1)` /
  // `stw r3,12(r1)` in retail, and the frame is 96 bytes) and the `2.f * radius` scale is
  // spelled out at each of the two `rs_new` sites (`lfs f0,-28784(r2)` = 0x8041B350 = 2.0f, then
  // `fmuls f0,f0,f31`, inside each branch) rather than hoisted into one named vector. All three
  // are measurements: taking the tag by pointer and hoisting the scale scores 84.14%.
  // Retail's `beq` at 0x800C12E0 is taken when `rstl_string_eq_c` returns **false**, i.e. it
  // branches *into* the body, so the empty name is what returns null: the constructor asks for
  // `kNoModelName` when it wants no model (`mSpiderBallGlassModel`, 0x800C0758), and every real
  // name it passes ("SamusBallCMDL", "SamusBallLowPolyCMDL", "SamusBallFrozenCMDL", the eleven
  // table entries) must come back with a model. mwcceppc branches to the continuation when the
  // `if` condition is false, so the condition here has to be the equality, not its negation -
  // written `!rstl_string_eq_c(...)` the function returns null for every *real* name and 100%
  // of its instructions except this one opcode are retail's.
  if (rstl_string_eq_c(name, "")) {
    return nullptr;
  }
  const SObjectTag tag = *gpResourceFactory->GetResourceIdByName(name.data());
  if (tag.type == 'CMDL') {
    return rs_new CModelData(
        CStaticRes(tag.id, CVector3f(2.f * radius, 2.f * radius, 2.f * radius)));
  }
  return rs_new CModelData(CAnimRes(tag.id, CAnimRes::kDefaultCharIdx,
                                   CVector3f(2.f * radius, 2.f * radius, 2.f * radius), 0,
                                   false));
}

// Retail 0x800C13EC, 0x32C = 203 insns. The suit comes from `CPlayerState`+0x54
// (`mCurrentSuit`, 0..2), and each of the eleven tables above is indexed by it with
// `lwzx` at a stride of 8 - so the table order in `.rodata` is part of the function.
// `mLoadedModelId` is `suit + 3 * state`, and the early-out when it already matches
// skips the `SetScale` too, which is why the compare branches straight to the epilogue
// rather than to the tail.
void CMorphBall::LoadMorphBallModel() {
  // Retail tests bit 6 of the flag byte at 0x1904 first and jumps straight to the `SetScale`
  // when it is set, so both power-up queries sit inside the `if`.
  if (!x1904_40_) {
    const CPlayerState& state = *mPlayer.GetPlayerState();
    const bool boost = state.HasPowerUp(CPlayerState::kIT_BoostBall);
    const bool spider = state.HasPowerUp(CPlayerState::kIT_SpiderBall);
    const int suit = state.GetCurrentSuitRaw();
    int id = suit;
    if (spider) {
      id = suit + 3;
    } else if (boost) {
      id = suit + 6;
    }
    // Retail 0x800C145C: when `mLoadedModelId` already equals `id` the branch goes to the
    // epilogue at 0x800C1704, past the `SetScale` - so the early-out is a `return`, not a
    // skipped reload. (The flag test above, by contrast, branches to the `SetScale`.)
    if (mLoadedModelId == id) {
      return;
    }
    mLoadedModelId = id;
    if (spider) {
      mBallModel = GetMorphBallModel(rstl::string_l(kSpiderBallModels[suit].name), mRadius);
      mBallModelShader = kSpiderBallModels[suit].shader;
      mLowPolyBallModel =
          GetMorphBallModel(rstl::string_l(kSpiderBallLowPolyModels[suit].name), mRadius);
      mLowPolyBallModelShader = kSpiderBallLowPolyModels[suit].shader;
      if (kSpiderBallCapsModels[suit].name) {
        mSpiderBallGlassModel =
            GetMorphBallModel(rstl::string_l(kSpiderBallCapsModels[suit].name), mRadius);
        mSpiderBallGlassModelShader = kSpiderBallCapsModels[suit].shader;
      } else {
        mSpiderBallGlassModel = nullptr;
        mSpiderBallGlassModelShader = 0;
      }
      mBallGlowColorIdx = kSpiderBallGlowColor[suit];
    } else if (boost) {
      mBallModel = GetMorphBallModel(rstl::string_l(kBoostBallModels[suit].name), mRadius);
      mBallModelShader = kBoostBallModels[suit].shader;
      mLowPolyBallModel =
          GetMorphBallModel(rstl::string_l(kBoostBallLowPolyModels[suit].name), mRadius);
      mLowPolyBallModelShader = kBoostBallLowPolyModels[suit].shader;
      mBallGlowColorIdx = kBoostBallGlowColor[suit];
    } else {
      mBallModel = GetMorphBallModel(rstl::string_l(kPlainBallModels[suit].name), mRadius);
      mBallModelShader = kPlainBallModels[suit].shader;
      mLowPolyBallModel =
          GetMorphBallModel(rstl::string_l(kPlainBallLowPolyModels[suit].name), mRadius);
      mLowPolyBallModelShader = kPlainBallLowPolyModels[suit].shader;
      mBallGlowColorIdx = kPlainBallGlowColor[suit];
    }
  }
  // Retail 0x800C16DC: `GetBallRadius()` once, multiplied by the 2.0f at -28784(r2) into a
  // three-component temporary, then `SetScale`. It sits outside the early-out above.
  const float scale = 2.f * GetBallRadius();
  mBallModel->SetScale(CVector3f(scale, scale, scale));
}

// Scaffold, not a reconstructed implementation.
void CMorphBall::FluidFXThink(CActor::EFluidState state, CScriptWater& water, CStateManager& mgr) {
  // TODO: Rate-limit water splashes using speed, fluid state and the fluid-plane manager.
}

// Retail 0x800C1864, 0xA4 = 41 insns. The result is `li r3,1` / `li r3,0` with a single
// `false` block out of line, i.e. the body is `return true` / `return false` at the leaves -
// folding the two tests into `height > 0.1f && height < ...` makes mwcceppc keep the bool in
// r31, which costs a fourth callee-saved register and a 64-byte frame against retail's 48.
bool CMorphBall::IsClimbable(const CCollisionInfo& collision) const {
  if (CMath::AbsF(collision.GetNormalLeft().GetZ()) < 0.7f) {
    const float height = GetBallPosition().GetZ() - collision.GetPoint().GetZ();
    if (height > 0.1f && height < GetBallRadius() - 0.05f) {
      return true;
    }
  }
  return false;
}

// The original body is genuinely empty.
void CMorphBall::Touch(CActor& actor, CStateManager& mgr) {}

// Retail 0x800C190C, 0x98 = 38 insns.
// `bl GetIsInHalfPipeMode__10CMorphBallCFv`; in half-pipe mode it takes
// `GetVelocityWR().Magnitude() * 1.5f` (r2-28852 = 1.5), then `rstl::max_val` against
// 0.01f (r2-28764) **first** and then `rstl::min_val` against 95.f (r2-28836). Retail
// loads both constants before the multiply (f0 = 1.5, f2 = 0.01, then `fmuls f0,f0,f1`),
// and its first `fcmpo cr0,f0,f2` / `bge` takes f0 (the product) when the product is the
// larger - a max. Otherwise it returns
// `gpTweakBall->GetBallTranslationMaxSpeed(mPlayer.GetSurfaceRestraint())`.
float CMorphBall::ComputeMaxSpeed() const {
  if (GetIsInHalfPipeMode()) {
    float maxSpeed = mPlayer.GetVelocityWR().Magnitude() * 1.5f;
    maxSpeed = rstl::max_val(maxSpeed, 0.01f);
    return rstl::min_val(maxSpeed, 95.f);
  }
  return gpTweakBall->GetBallTranslationMaxSpeed(mPlayer.GetSurfaceRestraint());
}

// Retail 0x800C19A4, 0xC0 = 48 insns. Retail stores `GetAngularVelocityWR().GetVector()`
// into the local at r1+0x14 and calls `Magnitude()` on *that*, so the vector is a named
// local; written as one expression the call is made on the returned reference, the six
// load/store instructions disappear and the frame shrinks from 80 to 64 bytes.
// The scalar has to be named too: `(speed - angularSpeed) * dt` left inline, mwcceppc
// allocates the three vector components f2/f1/f0 in x,y,z order and stores them x,y,z
// (`lfs f2,0` / `lfs f1,4` / `lfs f0,8`, then `stfs` to +8/+12/+16). Retail loads .y, .z, .x
// into f2/f1/f0 and stores .y, .x, .z - the same three multiplies, a different allocation.
// Naming the product fixes it; `direction * scale` and `scale * direction` both do.
void CMorphBall::SpinToSpeed(float speed, const CVector3f& direction, float dt) {
  const CVector3f angularVelocity = mPlayer.GetAngularVelocityWR().GetVector();
  const float angularSpeed = angularVelocity.Magnitude();
  const float scale = (speed - angularSpeed) * dt;
  mPlayer.ApplyTorqueWR(scale * direction);
}

void CMorphBall::ApplyGravity() {
  mPlayer.SetMomentumWR(CVector3f(0.f, 0.f, mPlayer.GetMass() * GetGravityAcceleration()));
}

// Retail 0x800C1AC0, 0x90 = 36 insns.
// `lwz r3,0(r3)` / `bl CheckSubmerged__7CPlayerCFv`; if submerged *and* not
// `li r4,25` (0x19 = kIT_GravityBoost, checked through `lwz r3,4884(r3)` =
// CPlayer+0x1314 = mPlayerState) it returns `GetBallWaterGravity`. Otherwise it tests
// `lwz r0,3200(r31)` (this+0xC80 = mBallState) against 4 then 5: 4 (kBS_ScrewAttack) ->
// GetScrewAttackGravity, 5 (kBS_ScrewAttackWallJump) -> GetScrewAttackWallJumpGravity,
// else GetBallGravity. It is an if/else chain, **not** a switch - written as a switch
// mwcceppc emits a `cmpwi 5` / `bge` / `cmpwi 4` tree in the opposite order and the
// function drops to 83%.
float CMorphBall::GetGravityAcceleration() const {
  if (mPlayer.CheckSubmerged() && !mPlayer.GetPlayerState()->HasPowerUp(CPlayerState::kIT_GravityBoost)) {
    return gpTweakBall->GetBallWaterGravity();
  }
  if (mBallState == kBS_ScrewAttack) {
    return gpTweakBall->GetScrewAttackGravity();
  }
  if (mBallState == kBS_ScrewAttackWallJump) {
    return gpTweakBall->GetScrewAttackWallJumpGravity();
  }
  return gpTweakBall->GetBallGravity();
}

// Retail 0x800C1B50, 0x98 = 38 insns.
// `lwz r3,0(r30)` / `bl GetSurfaceRestraint__7CPlayerCFv` / `bl GetBallTranslationFriction__10CTweakBallCFi`
// then, while `lhz r4,740(r3)` (CPlayer+0x2e4 = mAttachedActor) differs from the 2-byte SDA
// constant at -27740(r13) (kInvalidUniqueId), `fmuls f1,f1,2.f` (r2-28784 = 2.0). Then
// `lwz r0,752(r3)` (CPlayer+0x2f0 = mEnergyDrain.mSources.mCount); if it is > 0 the
// count is converted with the 2^52 + 2^31 trick and scaled by 1.5 (r2-28852).
float CMorphBall::CalculateSurfaceFriction() const {
  float friction = gpTweakBall->GetBallTranslationFriction(mPlayer.GetSurfaceRestraint());
  if (mPlayer.GetAttachedActor() != kInvalidUniqueId) {
    friction *= 2.f;
  }
  const int count = mPlayer.GetEnergyDrainSourceCount();
  if (count > 0) {
    friction *= count * 1.5f;
  }
  return friction;
}

// Scaffold, not a reconstructed implementation.
void CMorphBall::ComputeLiftForces(const CVector3f& controlForce, const CVector3f& velocity,
                                   const CStateManager& mgr) {
  // TODO: Update the lift averages and apply the contact-dependent upward force.
}

// Retail 0x800C22F8, 0x180 = 96 insns. The tree's previous body was 96.48% (8 of 96
// instructions) and the two causes were both measured, and both are in this body:
//
//  1. **The `- 0.f` is a `float` local, not a literal.** Retail's block is
//     `lfs f0,1/255` / `lfs f1,0.0` / `fmuls f2,f0,f2` / `lfs f0,1e-05` / `fsubs f1,f2,f1`
//     / `fabs` / `frsp`: a *separate* `lfs` of 0.0f and a *separate* `fsubs`, where the tree
//     emitted neither (mwcceppc folds `x - 0.0f`, so the whole subtract disappears - measured
//     with a throwaway `tools/probe_cc.sh` probe: `float f(float a){ return a - 0.0f; }`
//     compiles to a bare `blr`). **A non-`const` `float` local defeats the fold** - the
//     compiler must load the local, and the load of a `float` whose value is 0 is the same
//     `lfs f1,0(0)` retail has. Measured: literal `0.f` 8 differing instructions, `float zero
//     = 0.f` **3**. This is a codegen fact about mwcceppc, not a claim about what retail's
//     source said; `zero` is a faithful stand-in for the value retail subtracts.
//  2. **The two `AccumulateBounds` arguments go through one bound box.** Retail keeps the
//     optional's address in r31 (`addi r31,r1,72` / `mr r4,r31` / `addi r4,r31,12`), so the
//     second argument is the *same* object at +12, not an independently materialised
//     temporary: passing `trailBounds->GetMinPoint()` and `trailBounds->GetMaxPoint()` gives
//     `addi r4,r1,72` / `addi r4,r1,84` (3 differing), and naming the two references is no
//     better (3). `const CAABox& box = *trailBounds;` and reading both points off `box` is what
//     produces the shared base - **0 differing instructions, byte-exact.**
//     (The `const CVector3f* mn = &...; *(mn + 3)` spelling is 1 differing: `CVector3f` is 12
//     bytes, so `+3` is `+36`, and retail's `+12` is a `CVector3f`-independent stride.)
//
// Together these take the function from 96.479% to **100.00%**.
CAABox CMorphBall::GetRenderBounds(const CStateManager& mgr) const {
  const CVector3f center = GetBallPosition();
  const CVector3f extent(2.f * mRadius, 2.f * mRadius, 2.f * mRadius);
  CAABox bounds(center - extent, center + extent);
  // See point 1 above: `zero` is a non-`const` local on purpose, so mwcceppc emits retail's
  // `lfs` + `fsubs` instead of folding the subtract away.
  float zero = 0.f;
  if (!(CMath::AbsF(mSlowBlueTailSwooshGen->GetModulationColor().GetAlpha() - zero) < 1e-05f)) {
    const rstl::optional_object< CAABox > trailBounds = mSlowBlueTailSwooshGen->GetBounds();
    if (trailBounds.valid()) {
      // See point 2 above: both points come off one bound reference, which is what keeps
      // retail's single base in r31.
      const CAABox& box = *trailBounds;
      bounds.AccumulateBounds(box.GetMinPoint());
      bounds.AccumulateBounds(box.GetMaxPoint());
    }
  }
  return bounds;
}

// Scaffold, not a reconstructed implementation.
void CMorphBall::CollidedWith(const TUniqueId& id, const CCollisionInfoList& collisions,
                              CStateManager& mgr) {
  // TODO: Process contact materials/normals, boost damage, half-pipe and Screw Attack collisions.
}

// Scaffold, not a reconstructed implementation.
bool CMorphBall::BallCloseToCollision(const CStateManager& mgr, float distance,
                                      const CMaterialFilter& filter) const {
  // TODO: Test a swept sphere against the world's filtered collision geometry.
  return false;
}

void CMorphBall::DisableHalfPipeStatus() {
  SetIsInHalfPipeMode(false);
  SetIsInHalfPipeModeInAir(false);
  SetTouchedHalfPipeRecently(false);
  mTouchHalfPipeCooldown = 0.f;
  mDisableControlCooldown = 0.f;
  mPlayer.SetCollisionAccuracyModifier(5.f);
  mPrevHalfPipeNormal = CVector3f::Zero();
  mHalfPipeNormal = CVector3f::Zero();
}

void CMorphBall::SetTouchedHalfPipeRecently(bool touched) {
  mTouchedHalfPipeRecently = touched;
}

bool CMorphBall::GetTouchedHalfPipeRecently() const { return mTouchedHalfPipeRecently; }

void CMorphBall::SetIsInHalfPipeModeInAir(bool active) { mInHalfPipeModeInAir = active; }

bool CMorphBall::GetIsInHalfPipeModeInAir() const { return mInHalfPipeModeInAir; }

void CMorphBall::SetIsInHalfPipeMode(bool active) { mInHalfPipeMode = active; }

bool CMorphBall::GetIsInHalfPipeMode() const { return mInHalfPipeMode; }

// Scaffold, not a reconstructed implementation.
void CMorphBall::UpdateHalfPipeStatus(CStateManager& mgr, float dt) {
  // TODO: Expire half-pipe/contact cooldowns and adjust collision accuracy.
}

// Guessed name.
void CMorphBall::RenderScrewAttackJumpEffects() const {
  if (mScrewAttackJumpFlashGen.get()) {
    mScrewAttackJumpFlashGen->Render();
  }
  if (mScrewAttackWallJumpFlashGen.get()) {
    mScrewAttackWallJumpFlashGen->Render();
  }
}

// Scaffold, not a reconstructed implementation.
void CMorphBall::RenderDamageEffects(const CStateManager& mgr,
                                     const CTransform4f& transform) const {
  // TODO: Render the damage overlay using the ball's damage timer and glow color.
}

void CMorphBall::RenderIceBreakEffect(const CStateManager& mgr) const {
  if (mMorphBallIceBreakGen.get()) {
    mMorphBallIceBreakGen->Render();
  }
}

void CMorphBall::UpdateIceBreakEffect(float dt) {
  if (!mMorphBallIceBreakGen.get() && mMorphBallIceBreak.HasLock() &&
      mMorphBallIceBreak.IsLoaded()) {
    mMorphBallIceBreakGen = rs_new CElementGen(mMorphBallIceBreak);
    mMorphBallIceBreakGen->SetOrientation(mPlayer.GetTransform().GetRotation());
  }
  if (mMorphBallIceBreakGen.get()) {
    if (mMorphBallIceBreakGen->IsSystemDeletable()) {
      mMorphBallIceBreakGen = nullptr;
      mMorphBallIceBreak.Unlock();
    } else {
      mMorphBallIceBreakGen->SetGlobalTranslation(GetBallPosition());
      mMorphBallIceBreakGen->Update(dt);
    }
  }
}

void CMorphBall::ResetMorphBallIceBreak() {
  mMorphBallIceBreak.Lock();
  mMorphBallIceBreakGen = nullptr;
}

bool CMorphBall::IsMorphBallTransitionFlashValid() const {
  return mMorphBallTransitionFlashGen.get() != nullptr;
}

// Scaffold, not a reconstructed implementation.
void CMorphBall::RenderMorphBallTransitionFlash(const CStateManager& mgr) const {
  // TODO: Apply the suit-dependent transition color before rendering the generator.
}

void CMorphBall::UpdateMorphBallTransitionFlash(float dt) {
  if (!mMorphBallTransitionFlashGen.get() && mMorphBallTransitionFlash.HasLock() &&
      mMorphBallTransitionFlash.IsLoaded()) {
    mMorphBallTransitionFlashGen = rs_new CElementGen(mMorphBallTransitionFlash);
    mMorphBallTransitionFlashGen->SetOrientation(mPlayer.GetTransform().GetRotation());
  }
  if (mMorphBallTransitionFlashGen.get()) {
    if (mMorphBallTransitionFlashGen->IsSystemDeletable()) {
      mMorphBallTransitionFlashGen = nullptr;
      mMorphBallTransitionFlash.Unlock();
    } else {
      mMorphBallTransitionFlashGen->SetGlobalTranslation(GetBallPosition());
      mMorphBallTransitionFlashGen->Update(dt);
    }
  }
}

void CMorphBall::ResetMorphBallTransitionFlash() {
  mMorphBallTransitionFlash.Lock();
  mMorphBallTransitionFlashGen = nullptr;
}

// Scaffold, not a reconstructed implementation.
void CMorphBall::Render(const CStateManager& mgr, const CActorLights* lights) const {
  // TODO: Render the ball, glass, trails and Echoes Screw Attack/death-ball effects.
}

// Scaffold, not a reconstructed implementation.
void CMorphBall::PreRender(CStateManager& mgr, const CFrustumPlanes& frustum) {
  // TODO: Prepare model animation, rain-splash point generation, actor lights and the world shadow.
}

// Retail 0x800C55DC, 0x38 = 14 insns: `lwz r0,3200(r3)` is this+0xC80 = mBallState, `cmpwi r0,2`
// is kBS_Spider. On that branch it loads the float constant at -28856(r2) and skips the call
// entirely; otherwise `lwz r3,-28244(r13)` (`tools/sda.py -28244` = gpTweakBall) and
// `bl GetMinimumAlignmentSpeed__10CTweakBallCFv`.
float CMorphBall::GetMinimumAlignmentSpeed() const {
  if (mBallState == kBS_Spider) {
    return 0.f;
  }
  return gpTweakBall->GetMinimumAlignmentSpeed();
}

// Retail 0x800C5614, 0x100 = 64 insns is `pow()` twice (57.27% here). Two spellings measured
// this run and both are worse, so do not retry them: naming the world velocity as a local
// takes 45.50% (the three components then live in f29/f30/f31 and the frame grows 80 ->
// 128 bytes; retail spills them to the local at r1+0x18 instead), and keeping that local
// while also switching the angular half to `CAxisAngle::operator*=` (the `__amu__` call
// retail makes) takes 54.48%.
void CMorphBall::DampLinearAndAngularVelocities(float linearDamping, float angularDamping,
                                                float dt) {
  const float frames = 60.f * dt;
  const float linearScale = pow(1.f - linearDamping, frames);
  mPlayer.SetVelocityWR(linearScale * mPlayer.GetVelocityWR());
  const float angularScale = pow(1.f - angularDamping, frames);
  mPlayer.SetAngularVelocityWR(mPlayer.GetAngularVelocityWR() * angularScale);
}

// Retail 0x800C5714, 0xD0 = 52 insns. It materialises `mPlayer.GetVelocityWR()` into
// the local at r1+0x14, calls `Magnitude()` on it, then `fcmpo cr0,f31,f1` / `bge`
// against the `friction` parameter in f31 - i.e. the test is written `friction >=
// magnitude`, not `magnitude <= friction`: the latter makes mwcceppc emit
// `fcmpo cr0,f1,f31` / `cror eq,lt,eq` / `bne` (three instructions). On the taken path it
// re-calls `Magnitude()`, does `fsubs f31,f1,f31` (so the scalar lands back in f31), and
// only then calls `AsNormalized()` writing into the local at r1+0x8 - the order the two
// calls appear in the source is the order they are emitted.
void CMorphBall::ApplyFriction(float friction) {
  CVector3f velocity = mPlayer.GetVelocityWR();
  if (friction < velocity.Magnitude()) {
    velocity = velocity.AsNormalized() * (velocity.Magnitude() - friction);
  } else {
    velocity = CVector3f::Zero();
  }
  mPlayer.SetVelocityWR(velocity);
}

// Scaffold, not a reconstructed implementation.
bool CMorphBall::UpdateMarbleDynamics(CStateManager& mgr, float dt, const CVector3f& point) {
  // TODO: Apply marble alignment, rolling torque and contact-force response.
  return false;
}

// Scaffold, not a reconstructed implementation.
void CMorphBall::ApplyBoostBallDamage(CStateManager& mgr, TUniqueId id, const CDamageInfo& damage,
                                      float dt) {
  // TODO: Filter already-hit actors, scale damage and update the cooldown/history.
}

void CMorphBall::CancelBoosting() {
  mBoostChargeTime = 0.f;
  mBoostDrainTime = 0.f;
  if (mBallAnimationIndex == 1) {
    mBallAnimationIndex = 0;
    CSfxManager::SfxStop(mBoostChargeSfx);
    mBoostChargeSfx.Clear();
  }
}

void CMorphBall::LeaveBoosting() {
  if (IsBoosting()) {
    mBoostChargeTime = 0.f;
    mBallState = kBS_Normal;
  }
  mBoostDrainTime = 0.f;
}

// Scaffold, not a reconstructed implementation.
void CMorphBall::EnterBoosting(CStateManager& mgr, bool skipImpulse) {
  // TODO: Enter normal/spider boost, optionally apply the impulse, and reset damage history.
}

// Scaffold, not a reconstructed implementation.
void CMorphBall::ComputeBoostBallMovement(const CFinalInput& input, CStateManager& mgr, float dt) {
  // TODO: Handle charge, release, draining, Spider Boost direction and damage.
}

void CMorphBall::SetScrewAttackActive(bool active) { mForcedScrewJumpInput = active; }

// Scaffold, not a reconstructed implementation.
void CMorphBall::ApplyScrewAttackDamage(float dt, CStateManager& mgr) {
  // TODO: Build the swept contact list and apply Screw Attack damage.
}

// Scaffold, not a reconstructed implementation.
void CMorphBall::UpdateScrewAttackRecovery(float dt) {
  // TODO: Recover from recoil/collisions and request the player's exit animation.
}

// Scaffold, not a reconstructed implementation.
void CMorphBall::ComputeScrewAttackMovement(const CFinalInput& input, CStateManager& mgr,
                                            float dt) {
  // TODO: Handle jump/wall-jump input, speed/height limits and Screw Attack recovery.
}

// Scaffold, not a reconstructed implementation.
void CMorphBall::UpdateDeathBall(float dt, CStateManager& mgr) {
  // TODO: Update the multiplayer death-ball effects and per-object damage cooldowns.
}

// Scaffold, not a reconstructed implementation.
void CMorphBall::UpdateBallLight(float dt, CStateManager& mgr) {
  // TODO: Update the inner-glow light from the generator and ball lighting state.
}

// Scaffold, not a reconstructed implementation.
void CMorphBall::UpdateEffects(float dt, CStateManager& mgr) {
  // TODO: Update particle transforms, trail history, wake selection and light intensities.
}

void CMorphBall::StopParticleWakes() {
  mWallSparkGen->SetParticleEmission(false);
  for (int i = 0; i < 6; ++i) {
    mWakeEffects[i]->SetParticleEmission(false);
  }
}

void CMorphBall::LeaveMorphBallState(CStateManager& mgr) {
  LeaveBoosting();
  CancelBoosting();
  CSfxManager::SfxStop(mBoostChargeSfx);
  mBoostReleaseSfx.Clear();
  StopParticleWakes();
}

// Scaffold, not a reconstructed implementation.
void CMorphBall::EnterMorphBallState(CStateManager& mgr, EBallState state) {
  // TODO: Reset ball/spider/boost state, sounds, averages and lighting for the requested mode.
}

void CMorphBall::SetBallLightActive(CStateManager& mgr, bool active) {
  mBallLightActive = active;
}

void CMorphBall::DeleteLight(CStateManager& mgr) {
  if (mBallInnerGlowLight != kInvalidUniqueId) {
    mgr.DeleteObjectRequest(mBallInnerGlowLight);
    mBallInnerGlowLight = kInvalidUniqueId;
  }
}

// Retail 0x800C5B84, 0x58 = 22 insns: `cmpwi r6,27` on the event type (kUE_EventStart), then
// `lwz r0,908(r4)` = CPlayer+0x38C = mMorphBallState against 1 (kMS_Morphed), then
// `lwz r0,3200(r3)` = this+0xC80 = mBallState against 6 (kBS_ScrewAttackRecovery). All three
// tests are `bne`-to-`return false` in that order, and the taken path calls
// `CPlayer::fn_80184294(1)` and returns `li r3,1`.
bool CMorphBall::DoUserAnimEvent(CStateManager& mgr, const CInt32POINode& node,
                                 EUserEventType type) {
  if (type == kUE_EventStart && mPlayer.GetMorphballTransitionState() == CPlayer::kMS_Morphed &&
      mBallState == kBS_ScrewAttackRecovery) {
    mPlayer.fn_80184294(CPlayer::kMS_Morphed);
    return true;
  }
  return false;
}

// Scaffold, not a reconstructed implementation.
void CMorphBall::AcceptScriptMsg(CStateManager& mgr, const CScriptMsg& msg) {
  // TODO: Handle Echoes CScriptMsg creation/deletion of the inner-glow light.
}

// Scaffold, not a reconstructed implementation.
void CMorphBall::Update(float dt, CStateManager& mgr) {
  // TODO: Update effects, light, death-ball state, tire interpolation, damage decay, rain and
  // sound.
}

void CMorphBall::SwitchToTire() {
  mTireMode = true;
  mTireInterpolating = true;
  mBallTiltAngle = 0.f;
  mTireInterpolationSpeed = 1.f;
}

void CMorphBall::SwitchToMarble() {
  CVector3f lookDir = mPlayer.GetLookDir();
  CQuaternion tiltQ = CQuaternion::AxisAngle(
      CUnitVector3f(mPlayer.GetTransform().TransposeRotate(lookDir)),
      CRelAngle::FromRadians(mBallTiltAngle));
  mPlayer.SetTransform(mPlayer.GetTransform() * tiltQ.BuildTransform4f());
  mTireMode = false;
  mTireInterpolating = true;
  mTireInterpolationSpeed = -1.f;
}

// Scaffold, not a reconstructed implementation.
void CMorphBall::UpdateBallDynamics(CStateManager& mgr, float dt) {
  // TODO: Update contact orientation, tire/marble mode, damping and velocity history.
}

// Retail 0x800CE910, 0x48 = 18 insns, the same shape as `ForwardInput` with `li r4,3` /
// `li r4,4` (kC_TurnLeft / kC_TurnRight) in the two `GetAnalogInput` calls.
float CMorphBall::BallTurnInput(const CFinalInput& input) const {
  if (!IsMovementAllowed()) {
    return 0.f;
  }
  const float left = mPlayer.GetControlMapper().GetAnalogInput(CControlMapper::kC_TurnLeft, input);
  return left - mPlayer.GetControlMapper().GetAnalogInput(CControlMapper::kC_TurnRight, input);
}

bool CMorphBall::CalculateBallContactInfo(CVector3f& normal, CVector3f& point) const {
  if (mCollisionInfos.GetCount() > 0) {
    normal = mCollisionInfos[0].GetNormalLeft();
    point = mCollisionInfos[0].GetPoint();
    return true;
  }
  return false;
}

CTransform4f CMorphBall::CalculateSurfaceToWorld(const CVector3f& normal, const CVector3f& point,
                                                 const CVector3f& direction) const {
  if (direction.CanBeNormalized()) {
    const CVector3f forward = direction.AsNormalized();
    CVector3f right = CVector3f::Cross(direction, normal);
    if (right.CanBeNormalized()) {
      right.Normalize();
      const CVector3f up = CVector3f::Cross(right, forward).AsNormalized();
      return CTransform4f::FromColumns(right, forward, up, point + CVector3f(0.f, 0.f, 0.f));
    }
  }
  return CTransform4f::Identity();
}

CVector3f CMorphBall::GetBallPosition() const {
  return mPlayer.GetTranslation() + CVector3f(0.f, 0.f, mRadius);
}

CTransform4f CMorphBall::GetBallToWorld() const {
  // Retail inlines GetBallPosition() here (0x800CB764) and reads mRadius at this+12 rather
  // than calling GetBallRadius(), so the ball position is spelled out.
  const CTransform4f& playerXf = mPlayer.GetTransform();
  const CVector3f ballTranslation(0.f, 0.f, mRadius);
  const CVector3f& translation = mPlayer.GetTranslation();
  const CVector3f ballPos = translation + ballTranslation;

  return CTransform4f(CTransform4f::Translate(ballPos) * playerXf.GetRotation());
}

CTransform4f CMorphBall::GetSwooshToWorld() const {
  // Retail (0x800CB7F4) materialises the last operator* at this+216 and copy-constructs it
  // into the return slot, so the product is wrapped in a real CTransform4f temporary; a plain
  // `return <product>;` is RVO'd straight into sret and drops that copy.
  return CTransform4f(
      (CTransform4f::Translate(mPlayer.GetTranslation() +
                               CVector3f(0.f, 0.f, GetBallRadius())) *
       mSurfaceToWorld.GetRotation()) *
      CTransform4f::RotateY(CRelAngle::FromRadians(mBallTiltAngle)));
}

// Scaffold, not a reconstructed implementation.
void CMorphBall::ComputeMarioMovement(const CFinalInput& input, CStateManager& mgr, float dt) {
  // TODO: Compute camera-relative control, friction, lift, torque and contact response.
}

// Scaffold, not a reconstructed implementation.
void CMorphBall::TransformSpiderBallState(const CQuaternion& rotation,
                                          const CVector3f& translation) {
  // TODO: Rotate the saved physics forces/normals and transform the track point about the ball.
}

// Scaffold, not a reconstructed implementation.
void CMorphBall::CreateSpiderBallParticles(CStateManager& mgr, const CVector3f& ballPosition,
                                           const CVector3f& trackPoint) {
  // TODO: Emit/update the magnet effect along the ball-to-track segment.
}

float CMorphBall::GetSpiderBallSwingControllerMovementScalar() const {
  if (mSwingControlTime < 1.2f) {
    return 1.f;
  }
  return rstl::max_val(0.f, (2.4f - mSwingControlTime) / 1.2f);
}

void CMorphBall::UpdateSpiderBallSwingControllerMovementTimer(float movement, float dt) {
  if (CMath::AbsF(movement) < 0.05f) {
    ResetSpiderBallSwingControllerMovementTimer();
  } else if (mSwingControlDirection != CMath::Sign(movement)) {
    ResetSpiderBallSwingControllerMovementTimer();
    mSwingControlDirection = CMath::Sign(movement);
  } else {
    mSwingControlTime += dt;
  }
}

void CMorphBall::ResetSpiderBallSwingControllerMovementTimer() {
  mSwingControlDirection = 0.f;
  mSwingControlTime = 0.f;
}

// Retail 0x800CC758, 0x144 = 81 insns. It reads the same four mapped axes as
// `CalculateSpiderBallAttractionSurfaceForces` (backward first, then forward, then a fresh
// `lwz` of mPlayer for turn left / turn right), then
//   `fmr f1,f30` / `fmr f2,f31` / `bl atan2` / `frsp f2,f1` / `lfs f1,57.29578` / `fmuls f31,f1,f2`
// - the angle is scaled by 57.29578 (radian -> degree) and the `frsp` shows `atan2` is the
// **double** libm call demoted to float - and `CMath::SqrtF(t*t + f*f)` (which `-fp_contract on`
// fuses into one `fmadds`). The scale and the four thresholds are the `.sdata2` words
// 0x8041B4A0, 0x8041B4A4, 0x8041B4A8, 0x8041B4AC and 0x8041B4B0 (57.29578, -35, 125, -55, 145;
// `tools/sda.py s2:-28448` .. `s2:-28432`, `tools/dol_read.py 0x8041B4A0`).
//
// CORRECTED 2026-10-01: the comment above used to read `fmr f1,f31` / `fmr f2,f30`, i.e. the
// transposed pair, and the body was written to match *it* - `atan2(turnRightMinusLeft,
// forwardMinusBackward)`. **`tools/dis.sh 0x800CC758 0x144` says the opposite**:
// `fmr f1,f30` / `fmr f2,f31` at 0x800CC80C/0x800CC810, so retail's `atan2` is
// `atan2(forwardMinusBackward, turnRightMinusLeft)`. The call was written to the bytes.
// Measured: the transposed spelling 94.51%, this one 97.41%, and the two differ by exactly the
// `fmr` pair - the `fmr`s are the only instructions that changed.
//
// The old comment also claimed "the final `return` shares retail's `fneg` tail with the `-55`
// arm". **That was never true and is not true now**: retail's tail is
//   `fcmpo cr0,f31,f0(-55)` / `blt` / `fcmpo cr0,f31,f0(145)` / `ble` / `fneg` / `b` / `lfs f1,0.0f`
// - one `fneg`, reached both by the `-55` branch and by falling out of the `145` test, so
// mwcceppc tail-merged the two `return -magnitude` paths. About sixty spellings of the tail were
// measured (`tools/try_batch.py`, listed in `docs/goal-notes/cmorphball-wakeeffects-outofline-
// resize.md`); the `else` form below is the best of them at **3 differing instructions** and the
// residue is that one `fneg` plus the polarity of the `-55` test - mwcceppc emits `bge` (branch
// to the inner block) where retail emits `blt` (branch to the shared `fneg`), i.e. it keeps
// making the *first* source branch the fall-through. The three-statement form the old comment
// described scores 9. The body is left as the best measured, not as a guess.
float CMorphBall::GetSpiderBallControllerMovement(const CFinalInput& input) const {
  if (!IsMovementAllowed()) {
    return 0.f;
  }
  const CControlMapper& mapper = mPlayer.GetControlMapper();
  const float backward = mapper.GetAnalogInput(CControlMapper::kC_Backward, input);
  const float forwardMinusBackward =
      mapper.GetAnalogInput(CControlMapper::kC_Forward, input) - backward;
  const CControlMapper& turnMapper = mPlayer.GetControlMapper();
  const float turnLeft = turnMapper.GetAnalogInput(CControlMapper::kC_TurnLeft, input);
  const float turnRightMinusLeft =
      turnMapper.GetAnalogInput(CControlMapper::kC_TurnRight, input) - turnLeft;
  const float angle =
      57.29578f * static_cast< float >(atan2(forwardMinusBackward, turnRightMinusLeft));
  const float magnitude = CMath::SqrtF(forwardMinusBackward * forwardMinusBackward +
                                       turnRightMinusLeft * turnRightMinusLeft);
  if (angle > -35.f && angle < 125.f) {
    return magnitude;
  }
  // The `else` is load-bearing and is not a `0.f` boundary change: `angle > 145.f` inside it is
  // retail's "fell out of `angle <= 145.f`", so the four arms still mean
  // `-35 < angle < 125` -> magnitude, `angle < -55` -> -magnitude, `angle > 145` -> -magnitude,
  // otherwise 0 - identical to the three-statement spelling, which measures 9.
  if (angle < -55.f) {
    return -magnitude;
  } else {
    if (angle > 145.f) {
      return -magnitude;
    }
    return 0.f;
  }
}

void CMorphBall::SetSpiderBallSwingingState(bool swinging) {
  if (mSpiderBallSwinging != swinging) {
    ResetSpiderBallSwingControllerMovementTimer();
    mSpiderSwingInAir = true;
  }
  mSpiderBallSwinging = swinging;
}

// Scaffold, not a reconstructed implementation.
bool CMorphBall::FindClosestSpiderBallWaypoint(
    CStateManager& mgr, const CVector3f& center, CVector3f& trackPoint,
    CVector3f& interpolatedDirection, CVector3f& direction, float& distance, CVector3f& normal,
    ESpiderSurfaceType& surfaceType, TUniqueId& surfaceId, CTransform4f& surfaceTransform) const {
  // TODO: Search waypoint tracks, scripted surfaces and collision surfaces; populate the outputs.
  return false;
}

// Scaffold, not a reconstructed implementation.
bool CMorphBall::CheckForSwitchToSpiderBallSwinging(CStateManager& mgr) const {
  // TODO: Check Spider surface kind, attachment geometry and the player's movement state.
  return false;
}

// Scaffold, not a reconstructed implementation.
void CMorphBall::ApplySpiderBallRollForces(const CFinalInput& input, CStateManager& mgr, float dt) {
  // TODO: Find the attachment, project controls and apply Spider roll/attraction forces.
}

void CMorphBall::ResetSpiderBallForces() {
  mNormalizedSpiderSurfaceForces = CVector2f(0.f, 0.f);
  mSpiderTrackForceMagnitude = 0.f;
  mSpiderViewControlMagnitude = 0.f;
  mSpiderForcesReset = true;
}

// Retail 0x800CC7D4, 0x108 = 66 insns, returning an 8-byte struct so the hidden result
// pointer arrives in r3 and `this` in r4. It gates on `IsMovementAllowed` and, on the false
// path, loads the two floats of `CVector2f::skZeroVector` through SDA21 - so the early out
// is `return CVector2f::Zero();`, not `CVector2f(0.f, 0.f)`. The true path calls
// `CControlMapper::GetAnalogInput` four times off `mPlayer+5072`, keeping the **backward**
// reading (kC_Backward = 2) in f31 and the **forward** one (kC_Forward = 1) as the
// subtrahend - `fsubs f30,f1,f31` - then the same pair for turn left (3) / turn right (4),
// and finally `__ct__9CVector2fFff` with f1 = right-left and f2 = forward-backward. So the
// mapper address is hoisted into r31 by the `const CControlMapper&` local.
CVector2f CMorphBall::CalculateSpiderBallAttractionSurfaceForces(const CFinalInput& input) const {
  if (!IsMovementAllowed()) {
    return CVector2f::Zero();
  }
  const CControlMapper& mapper = mPlayer.GetControlMapper();
  const float backward = mapper.GetAnalogInput(CControlMapper::kC_Backward, input);
  const float forwardMinusBackward =
      mapper.GetAnalogInput(CControlMapper::kC_Forward, input) - backward;
  const CControlMapper& turnMapper = mPlayer.GetControlMapper();
  const float turnLeft = turnMapper.GetAnalogInput(CControlMapper::kC_TurnLeft, input);
  const float turnRightMinusLeft =
      turnMapper.GetAnalogInput(CControlMapper::kC_TurnRight, input) - turnLeft;
  return CVector2f(turnRightMinusLeft, forwardMinusBackward);
}

// Retail 0x800CB460, 0xA8 = 42 insns, returning a 12-byte struct so the hidden result pointer
// arrives in r3, `this` in r4, the force vector in r5 and `mgr` in r6. It reads the camera
// manager off the *player* (`lwz r3,4888(r4)` = CPlayer+0x1318), calls
// `CCameraManager::GetCurrentCamera(mgr, true)` - `li r5,1` is the selector - and copies the
// camera transform out with the copy constructor. It then uses only the .x and .y of each
// of the transform's three rows, against the .x and .y of the force vector:
//   out.x = m00*f.x + m01*f.y,  out.y = m10*f.x + m11*f.y,  out.z = m20*f.x + m21*f.y.
// The sum has to be **named**: returned straight from the expression, mwcceppc puts `lr` back
// before it reloads r30 and emits `lwz r31 / lwz r0 / lwz r30` in the epilogue, where retail
// emits `lwz r31 / lwz r30 / lwz r0`. The arithmetic is identical either way - it is the *named*
// local that moves the last use of r30 after the link register's.
CVector3f CMorphBall::TransformSpiderBallForcesXZ(CVector2f& forces, CStateManager& mgr) const {
  const CTransform4f camXf = mPlayer.GetCameraManager()->GetCurrentCamera(mgr, true)->GetTransform();
  const CVector3f res =
    CVector3f(camXf.Get00(), camXf.Get10(), camXf.Get20()) * forces.GetX() +
    CVector3f(camXf.Get01(), camXf.Get11(), camXf.Get21()) * forces.GetY();
  return res;
}

// Retail 0x800CB4C0, the same 42 instructions with .x and .z of each row:
//   out.x = m00*f.x + m02*f.y,  out.y = m10*f.x + m12*f.y,  out.z = m20*f.x + m22*f.y.
CVector3f CMorphBall::TransformSpiderBallForcesXY(CVector2f& forces, CStateManager& mgr) const {
  const CTransform4f camXf = mPlayer.GetCameraManager()->GetCurrentCamera(mgr, true)->GetTransform();
  const CVector3f res =
    CVector3f(camXf.Get00(), camXf.Get10(), camXf.Get20()) * forces.GetX() +
    CVector3f(camXf.Get02(), camXf.Get12(), camXf.Get22()) * forces.GetY();
  return res;
}

// Scaffold, not a reconstructed implementation.
void CMorphBall::ApplySpiderBallSwingingForces(const CFinalInput& input, CStateManager& mgr,
                                               float dt) {
  // TODO: Apply the radial constraint, swing input and gravity about the Spider track.
}

void CMorphBall::UpdateSpiderBall(const CFinalInput& input, CStateManager& mgr, float dt) {
  SetSpiderBallSwingingState(CheckForSwitchToSpiderBallSwinging(mgr));
  if (mSpiderBallSwinging) {
    ApplySpiderBallSwingingForces(input, mgr, dt);
  } else {
    ApplySpiderBallRollForces(input, mgr, dt);
  }
}

void CMorphBall::SetDamageTimer(float time) { mDamageTimer = time; }

void CMorphBall::SetDisableSpiderBallTime(float time) { mDisableSpiderBallTime = time; }

// Retail 0x800CE7D0, 0x94 = 37 insns. `fn_80215860` is not in this unit - it is
// `build/G2ME01/obj/auto_03_80215424_text.o`'s `lwz r3,0(r3)` / `lbz r3,319(r3)` / `blr`, i.e.
// `CTweakPlayerControls::mData->booleans.unknown_0x5282c47e` (SLdrTweakPlayerControls+0x13F;
// the 0x130 base is 4 + 75 * 4, measured from the struct's own field list). The other two
// callees are `CPlayer::GetTweakPlayerControls` (0x8000BF7C) and
// `CPlayer::IsMorphBallTransitioning` (0x80019DF8).
//
// The `clrlwi. r0,r3,24` / `bne` on each bool is the EABI `if (x)` shape, and the
// `lfs f1,6240(r31)` tail is the standard MWCC float-to-bool: `fcmpo cr0,f1,f0` / `mfcr` /
// `rlwinm r0,r0,2,31,31` / `cntlzw` / `srwi r3,r0,5`. 6240 = 0x1860, which
// `tools/probe_cc.sh` (this run) reports as `mDisableControlCooldown`.
//
// **The outer test is written negated on purpose.** Retail's `bne` at 0x800CE7F4 branches
// *into* the free-look pair, so the source condition is the negation of the polarity MWCC
// picks for a positive spelling: `A || (!B && !C)` lays the pair out with a fall-through
// `return false` retail does not have (measured 88.78%, 20 of 37 instructions wrong), while
// `!A && (B || C)` reproduces retail's shape exactly. The `B || C` form is also the one the
// bytes imply: CPlayer+0x5F1/0x5F2 are "free look engaged" and "look button down", so
// movement is blocked when *either* is set.
extern "C" bool fn_80215860(const CTweakPlayerControls* self);
bool CMorphBall::IsMovementAllowed() const {
  if (!fn_80215860(mPlayer.GetTweakPlayerControls()) &&
      (mPlayer.GetInFreeLook() || mPlayer.GetLookButtonHeld())) {
    return false;
  }
  if (mPlayer.IsMorphBallTransitioning()) {
    return false;
  }
  return !(mDisableControlCooldown > 0.f);
}

void CMorphBall::ComputeBallMovement(const CFinalInput& input, CStateManager& mgr, float dt) {
  switch (mBallState) {
  case kBS_Normal:
  case kBS_Boost:
  case kBS_Spider:
  case kBS_SpiderBoost:
  case kBS_Projectile:
    ComputeBoostBallMovement(input, mgr, dt);
    ComputeMarioMovement(input, mgr, dt);
    break;
  case kBS_ScrewAttack:
  case kBS_ScrewAttackWallJump:
    ComputeScrewAttackMovement(input, mgr, dt);
    break;
  case kBS_ScrewAttackRecovery:
    UpdateScrewAttackRecovery(dt);
    break;
  }
}

// Retail 0x800CE9A8, 0x48 = 18 insns. `bl IsMovementAllowed` then `clrlwi. r0,r3,24` /
// `bne` - the EABI `if (!bool)` shape - and the false path returns the SDA float 0.0. On the
// true path it calls `CControlMapper::GetAnalogInput` twice with `this+5072` in r3 (so the
// mapper is a *member* read off the player, not a static), `li r4,1` / `li r4,2`
// (kC_Forward / kC_Backward) and `li r6,0` (the default kFT_Filtered), keeps the first
// result in f31 and returns `fsubs f1,f31,f1`.
float CMorphBall::ForwardInput(const CFinalInput& input) const {
  if (!IsMovementAllowed()) {
    return 0.f;
  }
  const float forward =
      mPlayer.GetControlMapper().GetAnalogInput(CControlMapper::kC_Forward, input);
  return forward - mPlayer.GetControlMapper().GetAnalogInput(CControlMapper::kC_Backward, input);
}

// Retail 0x800CE9A4, 0x24 = 9 insns is exactly `lwz r3,-28244(r13)` /
// `bl GetBallTouchRadius__10CTweakBallCFv` and nothing else - `tools/sda.py -28244` resolves the
// `disp(r13)` to `gpTweakBall`, so this is the whole body. It was a scaffold until
// `src/MetroidPrime/PortCTweakBall.cpp` (which the port build lists and the DOL build does not)
// defined the callee; without that definition the call would be a 251st undefined symbol and
// `tools/link_check.sh --strict` would fail the gate against its 250 baseline.
float CMorphBall::GetBallTouchRadius() const { return gpTweakBall->GetBallTouchRadius(); }

float CMorphBall::GetBallRadius() const { return mPlayer.GetTweakPlayer()->GetBallRadius(); }

// Ownership cleanup is supplied by the members' destructors.
CMorphBall::~CMorphBall() {}

// Material 59 has no established semantic name in this checkout.
CMorphBall::CMorphBall(CPlayer& player, float radius, bool multiplayer)
: mPlayer(player)
, mLoadedModelId(-1)
, mBallGlowColorIdx(0)
, mRadius(radius)
, mBoostControlForce(CVector3f::Zero())
, mControlForce(CVector3f::Zero())
, mTireMode(false)
, mTireLeanAngle(0.f)
, mBallTiltAngle(0.f)
, mCollisionSphere(
      CSphere(CVector3f(0.f, 0.f, radius), radius),
      CMaterialList(kMT_Player, kMT_Unknown59, kMT_GroundCollider, kMT_NoPlayerCollision))
, mBallModel(GetMorphBallModel(multiplayer ? kMultiplayerBallModelName : "SamusBallCMDL", radius))
, mBallModelShader(0)
, mSpiderBallGlassModel(GetMorphBallModel(kNoModelName, radius))
, mSpiderBallGlassModelShader(0)
, mLowPolyBallModel(GetMorphBallModel("SamusBallLowPolyCMDL", radius))
, mLowPolyBallModelShader(0)
, mFrozenBallModel(GetMorphBallModel("SamusBallFrozenCMDL", radius))
, mLastWallCollisionFrame(-1)
, mLastFloorCollisionFrame(-1)
, mBallState(kBS_Normal)
, mPlayerToSpiderNormal(CVector3f::Zero())
, mSpiderPullMovement(1.f)
, mSpiderTrackPoint(CVector3f::Zero())
, mSpiderInterpBetweenPoints(CVector3f::Zero())
, mSpiderBetweenPoints(CVector3f::Zero())
, mLinearVelocityDamping(0.f)
, mAngularVelocityDamping(0.f)
, mSpiderNearby(false)
, mTouchingSpider(false)
, mSpiderBallSwinging(false)
, mSpiderSwingInAir(true)
, mSpiderSurfaceType(kSST_None)
, mSpiderSurfaceTransform(CTransform4f::Identity())
, mSpiderSurfacePivotAngle(0.f)
, mSpiderSurfacePivotTargetAngle(0.f)
, mRefPullVelocity(0.f)
, mPlayerToSpiderTrackDistance(0.f)
, mSwingControlDirection(0.f)
, mSwingControlTime(0.f)
, mNormalizedSpiderSurfaceForces(0.f, 0.f)
, mSpiderTrackForceMagnitude(0.f)
, mSpiderViewControlMagnitude(0.f)
, mDamageTimer(0.f)
, mSpiderForcesReset(false)
, mSurfaceToWorld(CTransform4f::Identity())
, mSlowBlueTailSwoosh(
      gpSimplePool->GetObj(multiplayer ? "SlowBlueTailSwoosh_MP" : "SlowBlueTailSwoosh"))
, mSlowBlueTailSwoosh2(
      gpSimplePool->GetObj(multiplayer ? "SlowBlueTailSwoosh2_MP" : "SlowBlueTailSwoosh2"))
, mJaggyTrail(gpSimplePool->GetObj(multiplayer ? "JaggyTrail_MP" : "JaggyTrail"))
, mSideSwoosh(gpSimplePool->GetObj("SideSwooshSide"))
, mWallSpark(gpSimplePool->GetObj("WallSpark"))
, mBallInnerGlow(gpSimplePool->GetObj("BallInnerGlow"))
, mSpiderBallMagnet(gpSimplePool->GetObj("SpiderBallMagnetEffect"))
, mBoostBallGlow(gpSimplePool->GetObj("BoostBallGlow"))
, mMorphBallTransitionFlash(gpSimplePool->GetObj("MorphBallTransitionFlash"))
, mMorphBallIceBreak(gpSimplePool->GetObj("Effect_MorphBallIceBreak"))
, mBoostEffect(gpSimplePool->GetObj("BoostEffect"))
, mDeathBallOuterShell(gpSimplePool->GetObj("DeathBallOuterShell"))
, mDeathBallSpikes(gpSimplePool->GetObj("DeathBallSpikes"))
, mScrewAttackJumpFlash(gpSimplePool->GetObj("ScrewAttackJumpFlash"))
, mSlowBlueTailSwooshGen(rs_new CParticleSwoosh(mSlowBlueTailSwoosh, 0))
, mSlowBlueTailSwooshGen2(rs_new CParticleSwoosh(mSlowBlueTailSwoosh, 0))
, mSlowBlueTailSwoosh2Gen(rs_new CParticleSwoosh(mSlowBlueTailSwoosh2, 0))
, mSlowBlueTailSwoosh2Gen2(rs_new CParticleSwoosh(mSlowBlueTailSwoosh2, 0))
, mJaggyTrailGen(rs_new CParticleSwoosh(mJaggyTrail, 0))
, mSideSwooshGen(multiplayer ? nullptr : rs_new CParticleSwoosh(mSideSwoosh, 0))
, mSideSwooshGen2(multiplayer ? nullptr : rs_new CParticleSwoosh(mSideSwoosh, 0))
, mWallSparkGen(rs_new CElementGen(mWallSpark))
, mBallInnerGlowGen(rs_new CElementGen(mBallInnerGlow))
, mSpiderBallMagnetGen(rs_new CElementGen(mSpiderBallMagnet))
, mBoostBallGlowGen(rs_new CElementGen(mBoostBallGlow))
, mBoostEffectGen(nullptr)
, mMorphBallTransitionFlashGen(nullptr)
, mMorphBallIceBreakGen(nullptr)
, mDeathBallOuterShellGen(nullptr)
, mDeathBallSpikesGen(nullptr)
, mScrewAttackJumpFlashGen(nullptr)
, mScrewAttackWallJumpFlashGen(nullptr)
, mWakeEffectIndex(-1)
, mBallInnerGlowLight(kInvalidUniqueId)
, mBallLightActive(false)
, mWorldShadow(rs_new CWorldShadow(16, 16, false))
, mActorLights(
      rs_new CActorLights(8, CVector3f::Zero(), 4, 4, 0.1f, false, false, false, false))
, mRainSplashGen(rs_new CRainSplashGenerator(mBallModel->GetScale(), 40, 2, 0.15f, 0.5f))
, mTireFactor(0.f)
, mMaxTireFactor(0.5f)
, mTireInterpolationSpeed(1.f)
, mTireInterpolating(false)
, mBoostOverLightFactor(0.f)
, mBoostLightFactor(0.f)
, mSpiderLightFactor(0.f)
, mBallOrientationAverage(CQuaternion::NoRotation())
, mBallPositionAverage(CVector3f::Zero())
, mLiftSpeedAverage(0.f)
, mLiftControlForceAverage(CVector3f::Zero())
, mFailsafeCounter(0)
, mVelocityBeforeFailsafe(CVector3f::Zero())
, mVelocityAfterFailsafe(CVector3f::Zero())
, mBoostEnabled(true)
, mTouchedFloorDuringBoost(false)
, mBoostChargeTime(0.f)
, mTimeNotInBoost(1000.f)
, x1028_(0.f)
, mBoostDrainTime(0.f)
, mBoostEffectTime(0.f)
, mBoostDamageScale(1.f)
, mDisableSpiderBallTime(0.f)
, mHasSpiderBoostDirection(false)
, mBoostTrailFadeTimer(0.f)
, mInHalfPipeMode(false)
, mInHalfPipeModeInAir(false)
, mTouchedHalfPipeRecently(false)
, mBallCloseToCollision(false)
, mCloseToCollisionTime(0.f)
, mTouchHalfPipeCooldown(0.f)
, mDisableControlCooldown(0.f)
, mTouchedHalfPipeRecentCooldown(0.f)
, mPrevHalfPipeNormal(CVector3f::Zero())
, mHalfPipeNormal(CVector3f::Zero())
, mBallAnimationIndex(0)
, mRollSfxId(0xffff)
, mLandSfxId(0xffff)
, mWallSparkFrameCountdown(1)
, mEndScrewAttackRequested(false)
, mTouchingWall(false)
, mPendingRecoil(false)
, mRecoiling(false)
, mWallJumpInputPending(false)
, mCollidedDuringRecovery(false)
, x18a8_30_(false)
, mForcedScrewJumpInput(false)
, mScrewAttackJumpCount(0)
, mWallJumpCount(0)
, mScrewAttackExitAnimationFrames(0)
, mScrewAttackGroundedFrames(0)
, mTimeSinceScrewAttackJump(0.f)
, mWallContactTime(0.f)
, mScrewAttackRecoveryCollisionTime(0.f)
, mWallNormal(CVector3f::Zero())
, mScrewAttackDirection(CVector3f::Zero())
, mBoostState(kBBS_BoostAvailable)
, mBombJumpState(kBJS_BombJumpAvailable)
, mDamageEffect(0.f)
, mDamageEffectDecaySpeed(0.f)
, mDamageTime(0.f)
, mMultiplayer(multiplayer)
, mShadow(nullptr) {
  mSpiderBallMagnetGen->SetParticleEmission(false);
  mSpiderBallMagnetGen->Update(double(1.f / 60.f));
  sBallCloseToCollisionDistance = GetBallRadius() + 0.2f;
  InitializeWakeEffects();
  mDeathBallDamageCooldowns.reserve(16);
  // TODO: recover the single-player material preparation calls (see research notes).
  mPlayer.SetCollisionAccuracyModifier(5.f);
}

// Retail 0x800C02A4, 0x3C = 15 insns.
//
// **`CreateBallShadow` (and with it `DeleteBallShadow`) is declared here, at the end of the file,
// not at retail's own position among the first five functions - deliberately.** mwcceppc emits
// function bodies in reverse source order and `@stringBase0` is ordered by first use in that same
// order, so where a function sits in this file decides where its string literals land in the pool.
// Retail's pool has `"??"(??)"` (the `operator new` placement string, used by every `rs_new`) at
// offset 378 and `TXTR_BallFade` at 385 - after `InitializeWakeEffects`' wake names and before the
// constructor's `SlowBlueTailSwoosh*` names. Declared up with the other shadow functions, the pair
// interned at 682/689 instead, which moves the `addi r4,rX,378` that every `rs_new` site computes
// and costs four functions their final instruction. Measured: with this move the pool matches
// retail byte-for-byte through offset 385 and these three functions go to 100.00%.
void CMorphBall::DeleteBallShadow() { mShadow = nullptr; }

void CMorphBall::CreateBallShadow() {
  if (!mShadow.get()) {
    mShadow = rs_new CMorphBallShadow(64, 64, gpSimplePool->GetObj("TXTR_BallFade"));
  }
}
