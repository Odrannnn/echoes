#ifndef _CMODEL
#define _CMODEL

// This header was never self-contained - it uses `uint` (line 7) and every .cpp that included
// it had to include `types.h` first. `src/Kyoto/Graphics/CModelTouch.cpp` hit that.
#include "types.h"

class CModelFlags;

class CModel {
  static uint sTotalMemory;

public:
  void Touch(int) const;
  void Draw(const CModelFlags&) const;
  bool IsLoaded(int matIdx) const;

  static void AddToTotal(uint amt) { sTotalMemory += amt; }
  static void RemoveFromTotal(uint amt) { sTotalMemory -= amt; }

  // The bound `Touch` walks: `fn_80027AE8` (0x80027AE8) is
  //     for (int i = 0; i < model->x1c_numParts; i++) model->Touch(i);
  // and `fn_80027B44` (0x80027B44) is the same loop for one index. Nothing else in the tree
  // names a member here and nothing includes this header yet, so the offset is the only claim
  // being made; `Touch`'s parameter is the part index.
  char x0_pad[0x1c];
  int x1c_numParts;
  // `CModel::Touch` (0x803112DC) does `lwz r3,40(r30) ; bl fn_802BBDB8`, so there is a pointer at
  // +0x28 and the class is at least 0x2c bytes. Nothing in the tree names the type - the callee
  // reads a byte flag at +0x40 of it and that is the whole of what is measured - so it is opaque.
  // **This is the only member added to a class a `Matching` unit reads**: `CModelTouchParts.cpp`
  // uses +0x1c and nothing else, and neither of its two functions' frames depends on sizeof, so
  // `tools/gate.sh` is what confirms it.
  char x20_pad[8];
  void* x28_touchTarget;
};

#endif // _CMODEL
