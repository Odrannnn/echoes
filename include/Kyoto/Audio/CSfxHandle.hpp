#ifndef _CSFXHANDLE
#define _CSFXHANDLE

#include "types.h"

class CSfxHandle {
public:
  CSfxHandle() : mID(0) {}
  CSfxHandle(uint value);

  int GetIndex() const { return mID & 0xFFF; }
  static CSfxHandle NullHandle() { return CSfxHandle(); }
  // **No user-declared `operator=`, on purpose: it is what stops a whole-object copy of a
  // 8-byte struct containing a `CSfxHandle` from being a 2-word block move.** Measured on
  // `main/MetroidPrime/CActor`: with the user-declared `operator=` MWCC 2.7 expands an assignment
  // member by member (4 + 1 + 1 bytes), which is what `CActor::RemoveLoopedSoundAt` emitted and
  // what retail's `reserved_vector<SSound, 2>` fill constructor also emits. With the implicit one
  // the *assignment* becomes `lwz / lwz / stw / stw` over the 8 bytes, which is what retail's
  // `CActor::RemoveLoopedSoundAt` does (`lhz` for the pair's `first`, then two words for
  // `second`); that function only reaches 100% this way. **Construction is unaffected** - the
  // copy constructor still goes member by member, so the `reserved_vector` fill constructors keep
  // matching. Measured on the whole report: no unit's matched count moves and no function anywhere
  // regresses (tools/report_diff.py over all 2057 units).
  bool operator==(const CSfxHandle& other) const { return mID == other.mID; }
  bool operator!=(const CSfxHandle& other) const { return mID != other.mID; }
  operator bool() const { return mID != 0; }
  void Clear() { mID = 0; }

private:
  uint mID;
  static uint mRefCount;
};
CHECK_SIZEOF(CSfxHandle, 0x4)

#endif // _CSFXHANDLE
