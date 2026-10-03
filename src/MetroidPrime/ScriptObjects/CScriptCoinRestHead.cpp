// CScriptCoinRestHead.cpp - module ScriptCoin (REL id 58), `.text` 0x1BA4..0x1C38:
// `fn_58_1BA4`, which is `CScriptCoin`'s own `CActor::Touch` override.
//
// **The claim, and why it is a separate unit.** The old claim was one `NonMatching` range,
// `.text 0x1BA4..0x32B8` plus the module's whole `.rodata` and `.data`, with no source file. Ten
// functions sat in a range no object could reproduce, so none of them could be promoted. This unit
// now claims the single function at the bottom of that range and is `Matching`; the rest moved to
// `MetroidPrime/ScriptObjects/CScriptCoinRestBody.cpp`, `NonMatching` with no source, which is how a
// named range keeps retail's bytes. **The `.rodata`/`.data` claims had to move with it**: a
// `Matching` unit's claim must be exactly what its own object reproduces, and this object emits
// `.text` only. The unit is named `ScriptCoin/MetroidPrime/ScriptObjects/CScriptCoinRestHead`
// (module-qualified, `source=` carrying the real path) because the queue's target for it is
// module-qualified and `tools/goal_check.sh` resolves a `match` target against `configure.py`.
//
// **What the function is.** `lbl_58_data_40` is `CScriptCoin`'s vtable, and its eighteenth entry -
// `+0x8C`, index 17 counting the two leading words - is `fn_58_1BA4`, immediately after
// `fn_58_1B24` at index 16, which is the `GetTouchBounds` override
// (`src/MetroidPrime/ScriptObjects/CScriptCoinTouchBounds.cpp`). `CActor.hpp:92-93` declares
// `GetTouchBounds()` then `Touch(CActor&, CStateManager&)` in that order, so index 17 is `Touch`
// and the two arguments arrive in r4 and r5.
//
// **The body is `CScriptDebris::Touch`, instruction for instruction** -
// `src/MetroidPrime/ScriptObjects/CScriptDebris.cpp:388-393`, whose retail copy
// `Touch__13CScriptDebrisFR6CActorR13CStateManager` at 0x800D3294 (0x8C bytes) is this function
// with one substitution and nothing else:
//
//   0x800D32CC  A0 0D 93 A4  lhz r0, kInvalidUniqueId@sda21(r0)      <- the DOL's small-data form
//   0x00001BDC  3C 60 00 00  lis r3, kInvalidUniqueId@ha            <- and the module's: no
//   0x00001BE0  3C 80 44 45  lis r4, 0x4445                         sdata21, so the address is
//   0x00001BE4  38 A3 00 00  addi r5, r3, kInvalidUniqueId@l        formed in two instructions
//   0x00001BE8  7F C3 F3 78  mr r3, r30                              and the halfword is read
//   0x00001BEC  A0 05 00 00  lhz r0, 0x0(r5)                        through r5
//
// which is the whole 0x94 against 0x8C, and the reason the two `beq`s reach 0x58/0x48 here where
// `CScriptDebris`'s reach 0x50/0x40. Every other instruction is identical, including the two
// `sth r0,0x8(r1)` / `sth r0,0xc(r1)` of the `DeleteObjectRequest` argument: that is MWCC staging
// `GetUniqueId()`'s by-value return into the outgoing slot, and `CHUDBillboardEffect`'s
// `mgr.DeleteObjectRequest(GetUniqueId())` (0x800ED928-0x800ED93C) emits the same pair. The flag byte
// is the same `+0x2f9` bit `CScriptCoinTouchBounds` tests, and it is `CScriptDebris`'s
// `mDieOnProjectile` at the same offset in that class.
//
// **The spellings that are load-bearing**, all of them the ones the DOL's copy already needs:
//   * the cast is `TCastToPtr< CGameProjectile >(other)` on a **reference** - retail's
//     `bl "TCastToPtr<15CGameProjectile>__FR7CEntity"` is the `CEntity&` overload, so taking
//     `CEntity& other` and using the pointer overload would emit a different import;
//   * the message state is `kSS_Dead` (= 0x44454144), which retail materialises as
//     `lis r4,0x4445 / addi r4,r4,0x4144`;
//   * `SendScriptMsgs` is called with **all four arguments written out**. The two trailing
//     defaults in `CEntity.hpp:30-31` are not interface, they are codegen: with them MWCC fills
//     the arguments in after the frame layout, and the `TUniqueId` lands in the single outgoing
//     slot at `r1+0x10` that retail uses. `mgr` is still live in r31 across both calls;
//   * `DeleteObjectRequest(GetUniqueId())` takes `self->GetUniqueId()` **by value** -
//     `GetUniqueId()` is `CEntity`'s inline returning `mUniqueId`, and retail reads it as
//     `lhz r0,0x8(r30)` off the object r30 rather than through a pointer parameter.
//
// **`mw_version` is not overridden**, unlike `ScriptCoin/.../CScriptCoinRest.cpp` next to it: the
// module's default `GC/1.3.2` emits all 37 instructions at retail's displacements, which
// `tools/unit_fit.sh` reports as `.text claimed 148 ours 148 retail 148` and `build/report.json`
// as `fn_58_1BA4` at 100.00% with the unit `complete`.
//
// **`fn_58_1BA4` is reachable, which is why this claim links at all.** A `Matching` unit's `.text`
// is dead-stripped unless `FORCEACTIVE` references it (structural fact 3 in the module recipe), and
// this function is the vtable's `Touch` entry, so `build/G2ME01/ScriptCoin/ldscript.lcf` lists it.
// Of the nine functions the body range still claims, that leaves two more that a claim could
// promote - `fn_58_1C38` (the `Think` override, 0x9C8) and `fn_58_27D0` (`~CScriptCoin`, 0xDC),
// the other two `FORCEACTIVE` entries in the range. The remaining seven (`fn_58_2600`, `fn_58_263C`,
// `fn_58_2694`, `fn_58_27C8`, `fn_58_28AC`, `fn_58_3228`, `fn_58_327C`) are referenced by nothing in
// the module, and mwldeppc drops them from a `Matching` object.
//
// **The bodies are inside `#ifdef __MWERKS__` and the host branch is empty, so listing this file in
// `files.cmake` adds no undefined reference.** That is the arrangement `CSandBossRelTail3.cpp` and
// `CSandBossRelTail2.cpp` use, and it is load-bearing here rather than cosmetic: the call to
// `TCastToPtr< CGameProjectile >(CEntity&)` is a DOL import the module needs, and the port has no
// definition for it - measured, listing the file with the body unguarded took the port's link gap
// from 287 to 288 and failed `link_check.sh --strict` against
// `docs/research/port_link_baseline.txt`.
//
// Definitions are in descending retail text order: mwcceppc emits definitions in reverse source
// order and mwldeppc keeps the object's `.text` order verbatim, so ascending would permute the
// module's bytes with objdiff still at 100% and only the module's sha1 would catch it. The body is
// read off `build/G2ME01/ScriptCoin/asm/MetroidPrime/ScriptObjects/CScriptCoinRestHead.s`.

#ifdef __MWERKS__
#include "types.h"

#include "MetroidPrime/CEntity.hpp"
#include "MetroidPrime/CEntityInfo.hpp"
#include "MetroidPrime/CStateManager.hpp"
#include "MetroidPrime/TCastTo.hpp"
#include "MetroidPrime/TGameTypes.hpp"

/** Retail's own names are reproduced verbatim, so every definition stays C:
 *  `config/G2ME01/rels/ScriptCoin/symbols.txt` carries the `fn_<addr>` placeholder, and the
 *  linked module's symbol table is part of the file the sha1 covers, so a C++ definition would
 *  mangle to `_Z<len>fn_58_1BA4...` and put a name where retail has none. Only the template
 *  `TCastToPtr` keeps its C++ spelling, because the mangled name in the import table is exactly
 *  what retail wrote - `TCastToPtr<15CGameProjectile>__FR7CEntity` - and it is the call target the
 *  `bl` has to carry. The arrangement `CScriptCoinRest.cpp` and `CBacteriaSwarmRelTail.cpp` use. */
class CGameProjectile;

extern "C" void fn_58_1BA4(CEntity* self, CEntity& other, CStateManager& mgr) {
  if (*reinterpret_cast< const uchar* >(reinterpret_cast< const char* >(self) + 0x2f9) & 1) {
    if (TCastToPtr< CGameProjectile >(other)) {
      self->SendScriptMsgs(kSS_Dead, mgr, kInvalidUniqueId, kSM_None);
      mgr.DeleteObjectRequest(self->GetUniqueId());
    }
  }
}
#endif