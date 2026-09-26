// Layout probe: writes sizeof/offsetof of the members you list into `.data`, so the numbers can
// be read back with `objdump -s`. **Read them against retail, not against the header's comments.**
//
//   tools/probe_cc.sh tools/probe_offsets.cpp build/probe_offsets.o
//   build/binutils/powerpc-eabi-objdump -s -j .data build/probe_offsets.o
//
// The host compiler cannot be used: rstl::string and friends are 64-bit there and 32-bit under
// mwcceppc. `#define private public` around the include is what lets offsetof reach a private
// member; `offsetof` on a *bitfield* is an "illegal operand" under mwcceppc, so leave those out
// and infer the flag byte from the member before them.
//
// Why this exists: two of the largest single wins on the DOL side (CStateManager +17 matched
// functions, CAnimData +1 and three units' percentages) were both a header comment that had
// recorded **mwcceppc's own output** and been read as if it were retail's. A member laid out
// `0x34` where retail has `0x2C` moves every later member, and every function that touches one
// lands at 96-99% with a *uniform offset delta* - which is the signature to look for. See
// `tools/offset_shift.py`, which reports that delta for every function just under 100%.
#include "MetroidPrime/CStateManager.hpp"
#include "Kyoto/Math/CAABox.hpp"

#define O(m) (unsigned int) offsetof(CStateManager, m)

unsigned int g_sizes[] = {
  O(m_scriptIdMap),
  O(x1638),
  O(x1684),
  O(x1688),
  O(x1698),
  O(m_saveGameScreen),
  O(m_nextAreaId),
  O(x16a8),
  O(m_updateFrameIdx),
  O(m_objectDrawToken),
  O(m_pauseHudMessage),
  O(m_planes),
  O(x2900),
  O(x2938),
  O(x2944),
  O(x2948),
  (unsigned int) sizeof(CStateManager),
  (unsigned int) sizeof(CStateManager::TIdList),
  (unsigned int) sizeof(rstl::rc_ptr< void >),
  (unsigned int) sizeof(CFrustumPlanes),
  (unsigned int) sizeof(CAABox),
  (unsigned int) sizeof(CColor),
  (unsigned int) sizeof(CVector3f),
};

unsigned int g_n = sizeof(g_sizes) / sizeof(g_sizes[0]);
