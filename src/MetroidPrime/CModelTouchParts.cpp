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


// **The holder parameter is `const`, and that is load-bearing.** Retail schedules the
// `lwz rX,8(rX)` *between* `mflr r0` and the prologue's `stw r0,20(r1)`; a non-`const` holder
// parameter puts the store first and costs two instructions of difference in both functions.
// Measured: mwcceppc GC/2.7 at `-O0` through `-O5`, with and without `#pragma inline_max_size`,
// and twenty body shapes; a `const` holder is the only thing found that reproduces the order.
// `CFilePreload::Read` (`src/Kyoto/Streams/CFilePreload.cpp`, 36 bytes, 100%) has the same
// nine-instruction shape, a `const` holder and the same order, which is where the idiom was
// found.
extern "C"
void fn_80027B44(const SModelHolder* holder, int part) {
  holder->x8_model->Touch(part);
}

extern "C"
void fn_80027AE8(const SModelHolder* holder) {
  const CModel* const model = holder->x8_model;
  const int count = model->x1c_numParts;
  for (int part = 0; part < count; ++part) {
    model->Touch(part);
  }
}
