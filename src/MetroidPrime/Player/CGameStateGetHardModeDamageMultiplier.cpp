// Retail `GetHardModeDamageMultiplier__10CGameStateCFv` =
// `_ZNK10CGameState27GetHardModeDamageMultiplierEv`, .text 0x80142498..0x801424BC, 0x24 = 36
// bytes:
//
//     stwu r1,-16(r1) / mflr r0 / stw r0,20(r1)
//     lwz  r3,-28240(r13)      ; 0x80418F30 = gpTweakGame
//     bl   0x80216D38          ; fn_80216D38
//     lwz  r0,20(r1) / mtlr r0 / addi r1,r1,16 / blr
//
// So it forwards to `fn_80216D38(gpTweakGame.get())` and returns f1 unchanged - a 16-byte frame for one
// tail call, which is what mwcceppc emits for a forwarding wrapper whose callee already takes its
// argument in r3 and answers in f1 (the same shape as `Kyoto/Math/CMathSqrtF.cpp`).
//
// `fn_80216D38` is retail's own unnamed accessor, 12 bytes at 0x80216D38:
// `lwz r3,0(r3) ; lfs f1,84(r3) ; blr` - the float at +0x54 of the block `*gpTweakGame` points
// at. It is a member of the same `Tweaks` accessor family as `fn_80216D2C` (+0x58),
// `fn_80216D44` (+0x34) and `fn_80216D50` (+0x30), all four of which are one `lwz`/`lfs`/`blr`
// apart in `docs/research/port_link_gap.md`'s neighbourhood. **It stays `extern "C"`, and it is
// not this unit's to define**: renaming it is a `symbols.txt` change with module-hash
// consequences, and defining it here would need the `Tweaks` block layout, which is not what
// this unit is for. Since 2026-10-02 all four are defined by the `Matching` carve
// `src/MetroidPrime/Tweaks/Carve80216D2C.c`, so the declaration below resolves against it.
// (This unit itself is superseded and nothing builds it - see tools/check_files_cmake.py -
// upstream's `CGameState.cpp` holds `GetHardModeDamageMultiplier` now.)
//
// `gpTweakGame` is `extern CTweakGame*` in `MetroidPrime/Tweaks/CTweakGame.hpp` and is defined by
// `src/MetroidPrime/Tweaks/Tweaks.cpp`.
#include "MetroidPrime/Player/CGameState.hpp"

#include "MetroidPrime/Tweaks/CTweakGame.hpp"

extern "C" float fn_80216D38(CTweakGame* tweakGame);

float CGameState::GetHardModeDamageMultiplier() const { return fn_80216D38(gpTweakGame.get()); }
