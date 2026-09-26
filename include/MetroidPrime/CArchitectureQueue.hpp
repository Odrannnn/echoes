#ifndef _CARCHITECTUREQUEUE
#define _CARCHITECTUREQUEUE

#include "types.h"

#include "MetroidPrime/CArchitectureMessage.hpp"

#include "rstl/list.hpp"

class CArchitectureQueue {
public:
  void Push(const CArchitectureMessage& msg); // { x0_queue.push_back(msg); }
  // **Out of line on purpose.** Retail's is `fn_800495F0` (0x800495F0, 0xB0 = 176 bytes) and
  // `CIOWinManager::PumpMessages` (0x800496A0) reaches it with `bl fn_800495F0`; mwcceppc inlines an
  // inline member at -O4 and `PumpMessages` would then carry a 176-byte expansion where retail has
  // one call. The definition is in `src/MetroidPrime/CIOWinManagerPumpMessages.cpp`. Nothing else
  // in the tree calls `Pop`, so no other unit is affected.
  CArchitectureMessage Pop();
  void Clear() { x0_queue.clear(); }
  bool IsEmpty() const { return x0_queue.empty(); }

private:
  rstl::list< CArchitectureMessage > x0_queue;
};

#endif // _CARCHITECTUREQUEUE
