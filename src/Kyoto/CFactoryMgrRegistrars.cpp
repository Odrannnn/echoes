#include "Kyoto/CFactoryMgr.hpp"

// The 36 factory registrations of CGameGlobalObjects::AddPaksAndFactories (retail
// 0x80007504-0x80007864, 864 bytes, 24 bytes per entry) go through exactly two of these.
//
// **This file is port-only: `configure.py` does not declare it, so mwcceppc never compiles it and
// it is not a decompilation unit.** That is deliberate, and it is the same arrangement as
// `src/MetroidPrime/PortGlobals.cpp`. Two reasons:
//
//  * The bodies are read out of the DOL, not decompiled. Retail has them at 0x802F963C and
//    0x802F96E0 (0xA4 bytes each), both unnamed in `config/G2ME01/symbols.txt`, and both are
//    `insert if absent` over an `rstl::red_black_tree`. Making them a `Matching` unit would mean
//    renaming `fn_802F963C`/`fn_802F96E0` in `symbols.txt` so objdiff can pair them, and a DOL-wide
//    symbol rename is not a change this file should make on its own.
//  * They are worth **0** on the port's link either way, and that was measured, not assumed -
//    see below.
//
// Both take the same `this` - `CResFactory`+0x74, which is `CFactoryMgr`+0x00 - and differ only in
// which table they use. 33 of the 36 registrations call the fourCC-keyed one; the 3 that use the
// other are CMDL, AGSC and PATH, and retail's second table is keyed on the manager rather than on
// the type.
//
// **What the 36 registrations cost, measured.** `tools/link_gap.py`, with all 36 factory
// functions and both registrars referenced exactly as the block would reference them:
// 491 MISSING -> 525 MISSING. **Gross closed 0, gross opened 34, net +34** - the 33 unnamed
// `fn_*` factories plus `FStringTableFactory`. The two that cost nothing are `FRuleSetFactory`
// (src/MetroidPrime/CRuleSet.cpp) and `FDependencyGroupFactory` (src/Kyoto/CDependencyGroup.cpp),
// both already in the port. The registrars themselves are free: they resolve against
// `rstl::map.cpp`/`rstl/misc.cpp`, which already define `rstl::rbtree_rebalance` and
// `rstl::rmemory_allocator::allocate`.
//
// So the block is not written, and it should not be until the 33 factories exist. What *is* done
// is the reason it could not be written before: `CFactoryMgr` was `uchar pad[0x38]`, and a
// registration written against that is precisely the raw-offset access
// `tools/check_raw_offsets.py` exists to fail on. All 36 are now expressible.

// Declared descending by retail offset, for the day this file becomes a decompilation unit.
void CFactoryMgr::RegisterFactoryByOwner(uint owner, CFactoryFn factory) {
  if (x14_factoriesByOwner.find(owner) == x14_factoriesByOwner.end()) {
    x14_factoriesByOwner.insert(rstl::pair< uint, CFactoryFn >(owner, factory));
  }
}

void CFactoryMgr::RegisterFactoryByTypeIdx(uint typeIdx, CFactoryFn factory) {
  if (x0_factoriesByType.find(typeIdx) == x0_factoriesByType.end()) {
    x0_factoriesByType.insert(rstl::pair< uint, CFactoryFn >(typeIdx, factory));
  }
}
