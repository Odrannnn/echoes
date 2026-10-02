// Host definition of `fn_8022A5B4` (retail 0x8022A5B4, 0x8C = 140 bytes), the control-hint
// query that `CPlayerGunBase::ProcessInput` (retail 0x801DE29C) calls once, at 0x801DE330:
//
//   mr r3,r31 / bl GetControlHintManager / mr r5,mgr / li r4,1 / bl fn_8022A5B4
//
// so the signature is `(CHintManager* self, int mask, CStateManager& mgr)` and a **true** result
// is the one that *blocks* weapon input (the caller's `clrlwi. r0,r3,24 / beq` at 0x801DE334
// branches over the clearing block).
//
// **Not a configure.py unit.** Retail's copy is at 0x8022A5B4, inside
// `build/G2ME01/obj/auto_03_8022A5AC_text.o` - an unclaimed auto-split range no unit in
// `config/G2ME01/splits.txt` covers - so there is no source file a lane could list it in. This is
// the `Port*.cpp` arrangement the repo already uses for exactly that case
// (`PortCTweakPlayerControls.cpp`, `PortCTweakBall.cpp`, `PortTweakGlobals.cpp`), and the same
// caveat applies: **only this file is in `files.cmake`, and no build lists both.** Nothing here
// can reach `main.dol` or any of the 86 REL modules; the DOL linker resolves the name out of the
// retail object instead.
//
// Retail's body, from `./tools/dis.sh 0x8022A5B4 0x8C`:
//
//   r0  = self[0x18]                       count    (rstl::vector::mCount at +8)
//   r30 = self[0x20]                       data     (rstl::vector::mItems at +12)
//   r31 = r30 + count * 112                end
//   loop: lhz id, 4(entry) -> mgr.GetObjectById(id) -> TCastToPtr<CUnknown46>(that)
//         if (that != 0 && (that[0x1A8] & mask) != 0) return true
//         entry += 112; if (entry != end) goto loop
//   return false
//
// so it is "is there an active hint whose control flags include this bit".
//
// **Every offset below is a member, not a number**, which is why this file needs no
// `docs/research/raw_offsets.md` section. Measured with
// `tools/probe_cc.sh .tmp/opencode/probe_hint2.cpp` under mwcceppc:
//
//   CHintManager::mHints          0x14   -> mCount 0x18, mItems 0x20   (retail: 0x18 / 0x20)
//   CHintManager::SHint           0x70   (retail: mulli by 112)
//   CHintManager::SHint::mId      0x04   (retail: lhz 4(entry))
//   CUnknown46::mControlFlags     0x1a8  (retail: lwz 424(r3))
//
// `fn_801BA3EC` (0x801BA3EC) and `fn_801BA428` (0x801BA428) walk the same table with the same
// `mulli 112` and the same `lhz 4(entry)`, so the layout is pinned by three independent
// functions rather than one. `CUnknown46` is retail's control-hint actor, `EEntityType` 46 - the
// `LoadControlHint` / `CTLH` loader is `10CUnknown46` (`docs/research/missing_classes.md`) - and
// `CGameHint` is 0x1A8, so its first member is the word `fn_8022A5B4` tests.

#include "MetroidPrime/CHintManager.hpp"
#include "MetroidPrime/CGameHint.hpp"
#include "MetroidPrime/CStateManager.hpp"
#include "MetroidPrime/TCastTo.hpp"

extern "C" bool fn_8022A5B4(const CHintManager* self, int mask, CStateManager& mgr) {
  const int numHints = self->GetNumHints();
  for (int i = 0; i < numHints; ++i) {
    // Retail's is `GetObjectById` (the `const` overload, 0x80041998) - the same one
    // `fn_801B9480` uses - so the lookup goes through the const accessor and the cast takes the
    // `const_cast` that accessor implies. Nothing here is written through it.
    CUnknown46* hint =
        TCastToPtr< CUnknown46 >(const_cast< CEntity* >(mgr.GetObjectById(self->GetHint(i).mId)));
    if (hint != nullptr && (hint->GetControlFlags() & mask) != 0) {
      return true;
    }
  }
  return false;
}
