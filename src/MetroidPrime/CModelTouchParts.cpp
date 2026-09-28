#include "types.h"

#include "Kyoto/Graphics/CModel.hpp"

// Retail 0x80027AE8 .. 0x80027B68: the two loops that make a `CModel` resident, given the holder
// one of the selectors in `CModelDataModelSlots.cpp` returns.
//
//   0x80027B44  0x24 bytes: touch one part
//   0x80027AE8  0x5C bytes: touch every part, 0..x1c_numParts
//
// Each has exactly one caller in the whole DOL, and both are the ones already written:
//
//   0x80027B44 <- 0x800E5D54, inside `fn_800E5D20` (`CGunEffectTouch.cpp`, `Matching`)
//   0x80027AE8 <- 0x800E5CB4, inside `fn_800E5C78` (`CGunEffectTouchAll.cpp`)
//
// which is why they are separate from those two units: a `configure.py` unit claims one
// contiguous range, and 0x80027xxx is 0xB7000 bytes away from 0x800E5xxx. It is also why they
// were not on the link gap list at all until the selectors' consumers were compiled: nothing in
// `src/` referenced them, and a symbol nothing references is not a gap however undefined it is.

// The address of one of a `CModelData`'s three `rstl::optional_object< TCachedToken< CModel > >`
// members: the valid flag is at +0xC and the `CModel*` at +8.
struct SModelHolder {
  char x0_pad[8];
  CModel* x8_model;
  char xc_pad[4];
};

// The loop bound `fn_80027AE8` walks to: one `int` at **+0x1C** of `CModel`.
//
// `x1c_numParts` was the pre-merge name for it; upstream's `Kyoto/Graphics/CModel.hpp` names the
// member that occupies that offset, and the mapping is one-to-one:
//
//   offset  member                          size
//   0x00    mData          single_ptr        0x04
//   0x04    mDataLen       uint              0x04
//   0x08    mSurfaces      vector<void*>     0x10  (mAllocator, mCount@0x0C, mCapacity@0x10, mItems@0x14)
//   0x18    mMatSets       vector<SShader>   0x10  (mAllocator, **mCount@0x1C**, mCapacity@0x20, mItems@0x24)
//   0x28    mModelInstance single_ptr        0x04
//   0x2C    mLastFrame     uint              0x04
//   0x30    mCurrentMatxIdx:16 + two 1-bit fields
//          => CHECK_SIZEOF(CModel, 0x34), and the only `int` at 0x1C is `mMatSets.mCount`.
//
// Two retail measurements agree on what that count *means*, independently of the layout arithmetic:
// `CModelData::GetNumShaders` (0x800E4BF0) does `this->x10->x118` then `->x8` then `->x1C` and
// returns it as a shader count, and `CModel::Touch(int)`'s parameter is a shader index (retail
// passes it to `VerifyCurrentShader__6CModelCFi`, 0x803115F8) - so the "part" index these two
// functions walk is a material-set index, and 0x1C is `mMatSets.mCount`. See
// `docs/research/unidentified.md:168` and `src/MetroidPrime/CModelDataModelSlots.cpp:24`.
//
// **The local shape is deliberate, and it is what keeps the unit `Matching`.** `mMatSets` is
// private and upstream exposes no accessor for its size, so naming the member is not available
// without an additive edit to a header the matching build compiles - and, as in
// `CModelDataModelSlots.cpp`, a unit that claims only the words it actually reads is
// independent of the rest of the class. Only the one word is claimed here; `fn_80027B44` and
// `fn_80027AE8` read nothing else out of `CModel`.
struct SShaderCount {
  char x0_pad[0x1c];
  int mNumShaders;
};


// **The holder parameter is `const`, and that is load-bearing.** Retail schedules the
// `lwz rX,8(rX)` *between* `mflr r0` and the prologue's `stw r0,20(r1)`; a non-`const` holder
// parameter puts the store first and costs two instructions of difference in both functions.
// Measured: mwcceppc GC/2.7 at `-O0` through `-O5`, with and without `#pragma inline_max_size`,
// and twenty body shapes; a `const` holder is the only thing found that reproduces the order.
// `CFilePreload::Read` (`src/Kyoto/Streams/CFilePreload.cpp`, 36 bytes, 100%) has the same
// nine-instruction shape, a `const` holder and the same order, which is where the idiom was
// found.

// **Both bodies moved to `src/MetroidPrime/CAnimData.cpp`**, which upstream's
// `config/G2ME01/splits.txt` gives 0x80027AE8..0x80027B68 to - a range may only belong to one
// unit.  The port build compiles both files, so keeping the definitions here as well would be a
// duplicate; the object is left empty on purpose and this note is the whole translation unit.
