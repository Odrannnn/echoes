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
// `x1c_normalModel`, `x2c_xrayModel` and `x3c_infraModel` - each a
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
// The three holders this file reads at 0x118, 0x13C and 0x144 are therefore *`CAnimData`'s*, not
// an unidentified class's - and **they are not where this tree's `CAnimData` puts them.** Measured
// with mwcceppc on 2026-09-26, `sizeof(CAnimData)` is **0x630** (the header's own commented-out
// `CHECK_SIZEOF(CAnimData, 0x434 + 0x144)` says 0x578, so the class is 0xB8 out), and 0x118 in it
// is `x108_aabb`, a `CAABox`, with `x120_unk` at 0x130. Two of its member *names* are also 0x10
// below where the members are: `x178_particleDB` is at **0x188** and `x1d8_selfId` at **0x268**.
// That is why this unit cannot simply `reinterpret_cast` a real `CAnimData*` - and it is also
// why its own code is unaffected: the two selectors only *load* the word at 0x10 and index
// through it, so they are byte-exact whatever class it belongs to. The byte at 0x2AC with its
// bit-5 run (`fn_800E5374`) is retail `CAnimData`'s, for the same reason it is not in this
// tree's layout yet.
//
// What the two selectors genuinely need is a *shape*, and the shape is spelled by
// `SBeamModelSlots` below. Nothing about retail's `CAnimData` has to be right for that.

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
  switch (which) {
  case 2:
    if (self->x2c_xrayModel.m_valid) {
      return reinterpret_cast< SModelHolder* >(&self->x2c_xrayModel);
    }
    break;
  case 1:
    if (self->x3c_infraModel.m_valid) {
      return reinterpret_cast< SModelHolder* >(&self->x3c_infraModel);
    }
    break;
  default:
    break;
  }
  return reinterpret_cast< SModelHolder* >(&self->x1c_normalModel);
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
