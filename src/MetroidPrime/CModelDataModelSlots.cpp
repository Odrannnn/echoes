#include "MetroidPrime/CModelData.hpp"

#include "Kyoto/Graphics/CModel.hpp"

// Retail 0x800E4E50 .. 0x800E4EE4: the two "which model does this draw/touch use" selectors.
// They are a pair, and they must be a pair, because every caller of one calls the other with the
// same index and the same `this` - `CModelData::IsDefinitelyOpaque` (0x800E531C) and
// `CModelData::GetNumShaders` (0x800E4BF0) each branch on `x10` and pick one or the other.
//
//   0x800E4E9C  2 -> the xray member if it is loaded, 1 -> the infra member if it is loaded,
//                anything else (or an unloaded slot) -> the normal member
//   0x800E4E50  the same three choices, but reached through `x10`, and the fallback is reached
//                through `x10` as well
//
// `this` is a `CModelData`, and the three holders the two selectors return are its
// `mNormalModel` (`x1c_normalModel`), `mEchoModel` (`x2c_xrayModel`) and `mDarkModel` (`x3c_infraModel`) - each a
// `rstl::optional_object< TCachedToken< CModel > >`, which is 0x10 bytes with the valid flag at
// +0xC and the `CModel*` at +8 (`TCachedToken::x8_item`). Three independent confirmations:
//
//   * the default constructor (0x800E6AD0) clears exactly three bytes, 0x28, 0x38 and 0x48, which
//     are those three members' +0xC;
//   * `CModelData::IsDefinitelyOpaque` (0x800E531C) tests `this->x28` and then reads `this->x24`,
//     i.e. the flag and the value of the first member, in that order;
//   * `CModelData::GetNumShaders` (0x800E4BF0) does `this->x10->x118` then `->x8` then `->x1C`,
//     and 0x1C is `CModel::x1c_numParts` - the same count the touch loops in
//     `CModelTouchParts.cpp` loop to.
//
// The index is not `EWhichModel`: 2 selects the *xray* member and 1 the *infra* member, so the
// port's `kWM_XRay = 1` is not the value these two test. Every caller passes 0, 1 and 2, and
// nothing in the tree names the enumeration, so the parameter is left an `int`.
//
// **`CModelData` at 0x10 is `xc_animData.x4_item`, the `CAnimData*`, and this hole is not a
// hole.** `CHECK_SIZEOF(CModelData, 0x4c)` sees the whole class; the port's header models
// `xc_animData` as an `rstl::auto_ptr<CAnimData>` of 8 bytes at 0x0C, so 0x0C is its `x0_has` and
// **0x10 is its `x4_item`**. Four independent measurements, all in retail's own code:
//
//   * `CModelData`'s **destructor** (0x800E6810) tests the byte at **0x0C** and, when it is set,
//     calls `fn_8002C340(this->x10, 1)` - a `CAnimData` destructor taking a "deleting" flag;
//   * `CModelData::AdvanceParticles` (0x800E53F4) and `CModelData::RenderParticles` (0x800E5C4C)
//     both null-test **0x10** and then call `fn_8002B240` / `fn_800295BC` on it, both inside
//     `CAnimData`'s own address range;
//   * the **copy constructor** (0x80018FBC) copies 0x0C and 0x10 and then zeroes the source's
//     0x0C - which is `rstl::auto_ptr`'s stealing copy constructor, already spelled that way in
//     `include/rstl/auto_ptr.hpp`;
//   * the **default constructor** (0x800E6AD0) stores 0 and 0 to 0x0C and 0x10.
//
// **The three words this file reads at 0x118, 0x13C and 0x144 through `CModelData::mAnimData` (was `x0c_animData`)
// are retail's `CAnimData` members, and the header's ladder is now measured against retail - so
// they are identified.** `sizeof(CAnimData)` is **0x5B8**, pinned by `li r3,1464` at 0x80030184
// (the `operator new` in `fn_8002FED8`, whose result becomes `CModelData::x10`) and by the last
// store of the constructor `fn_8002D178`, `stfs f0,1460(r25)` at 0x8002D598. The evidence table
// is at the bottom of `include/MetroidPrime/CAnimData.hpp`; the three that matter here:
//
//   * **0x118 is a bare 4-byte pointer**, not a holder. Its only writer is `fn_8002ACC4` at
//     0x8002ACEC (`stw r0,280(r30)`). `GetNumShaders` (0x800E4BFC) does `x10->x118` then `->x8`
//     then `->x1C`, and 0x1C is `CModel::x1c_numParts`, so the pointee has a `CModel*` at +8.
//   * **0x13C and 0x144 are `rstl::rc_ptr`, two words each** - object at +0, refcount pointer at
//     +4. The constructor stores 0 into the first word and a freshly `new`ed `int(1)` into the
//     second (0x8002D2A8/0x8002D2CC and 0x8002D2DC/0x8002D2FC); the assign sites compare, release,
//     store both halves and increment the refcount (0x8002AC6C..0x8002AC9C for 0x13C,
//     0x8002AB88..0x8002ABB8 for 0x144); the destructor calls `fn_8002F270` on each separately
//     (0x8002C53C, 0x8002C54C).
//
// So the `SModelHolder` below is **not** the retail shape - and it does not need to be. Both
// selectors only *load* the one word at 0x118/0x13C/0x144 and return it, and that is true of an
// `rc_ptr`'s first word just as much as of a pointer to a holder, so the emitted code is
// byte-exact. `char x11c_pad[0x20]` is the two `optional_object` at 0x11C and 0x12C and
// `char x140_pad[4]` is the refcount word of 0x13C. Keeping the local shape is deliberate: it
// makes the unit independent of the rest of `CAnimData`, which is what lets it stay `Matching`.
//
// Note also that retail's *own* members are **not** the three `optional_object<TLockedToken<CModel>>`
// the two selectors use: the index-2 arm here returns `&self->EchoModel()` and the index-1 arm
// `&self->DarkModel()`, both of which are `CModelData`'s own, while 0x13C/0x144 are the
// `CAnimData`'s. Two levels of model slots, and the argument picks the same one in both.

// The address of one of those three `optional_object`s. `CGunEffectTouch.cpp` and
// `CGunEffectTouchAll.cpp` each declare their own copy of this shape; there is no shared header
// for a class retail does not name.
struct SModelHolder {
  char x0_pad[8];
  CModel* x8_model;
  char xc_pad[4];
};

struct SBeamModelSlots {
  char x0_pad[0x118];
  SModelHolder* x118;
  char x11c_pad[0x20];
  SModelHolder* x13c;
  char x140_pad[4];
  SModelHolder* x144;
};

// The word at 0x10, as a member, so that the load is the `lwz 16(r3)` retail emits and not an
// `addi`-formed address. `x10` is a `CAnimData*`; it is typed as the shape this file needs
// because this tree's `CAnimData` does not have that shape yet (above).
struct SModelDataFront {
  char x0_pad[0x10];
  SBeamModelSlots* x10;
};

extern "C"
SModelHolder* fn_800E4E9C(CModelData* self, int which) {
  // A `switch`, not an `if`/`else if`: retail's `cmpwi 2 / beq / bge / cmpwi 1 / bge / b` is
  // MWCC's binary decision tree over the cases {1, 2}, and an `if`/`else if` chain gives
  // `cmpwi 2 / bne` with the tests in the other order. Measured, 6 variants:
  // tools/try_batch.py, 0 differing instructions for this shape and >= 4 for every other.
  // The three slots are upstream's `mNormalModel` (0x1C), `mEchoModel` (0x2C) and `mDarkModel`
  // (0x3C) - the same three `optional_object<TLockedToken<CModel>>` members at the same offsets,
  // with `x2c_xrayModel` -> `mEchoModel` and `x3c_infraModel` -> `mDarkModel` being the Echoes
  // rename and the index-2/index-1 arms above still selecting them in that order. They are private
  // and the validity flag is private inside `optional_object`, so both are reached through the
  // `#ifdef TARGET_PC` accessors `CModelData.hpp` adds for
  // `src/MetroidPrime/CModelDataDefaultCtor.cpp`; `optional_object::valid()` is the flag as a
  // read, and it is the same byte the default constructor's three `stb`s at 0x28/0x38/0x48 clear.
  switch (which) {
  case 2:
    if (self->EchoModel().valid()) {
      return reinterpret_cast< SModelHolder* >(&self->EchoModel());
    }
    break;
  case 1:
    if (self->DarkModel().valid()) {
      return reinterpret_cast< SModelHolder* >(&self->DarkModel());
    }
    break;
  default:
    break;
  }
  return reinterpret_cast< SModelHolder* >(&self->NormalModel());
}

extern "C"
SModelHolder* fn_800E4E50(CModelData* self, int which) {
  // A `switch` for the same reason as `fn_800E4E9C`, and **no local for the pointee**: retail
  // re-reads `this->x10` inside each arm and again for the fallback, so `x10` must not be named.
  // With a named local MWCC keeps it in a register and never reloads it.
  SModelHolder* res = nullptr;
  switch (which) {
  case 2:
    res = reinterpret_cast< SModelDataFront* >(self)->x10->x13c;
    break;
  case 1:
    res = reinterpret_cast< SModelDataFront* >(self)->x10->x144;
    break;
  default:
    break;
  }
  if (res != nullptr) {
    return res;
  }
  return reinterpret_cast< SModelDataFront* >(self)->x10->x118;
}
