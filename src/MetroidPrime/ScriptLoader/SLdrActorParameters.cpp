// `__dt__19SLdrActorParametersFv` - `SLdrActorParameters`'s deleting destructor, DOL
// `.text:0x8023F4C4`, 0x70 = 112 bytes, 28 instructions (`config/G2ME01/symbols.txt:10166`).
//
// **Carved out of an unclaimed dtk `auto_*` range, in four files in one change**: this source,
// the `Object(...)` line in `configure.py`, the claim in `config/G2ME01/splits.txt`, and the
// `files.cmake` line. The range 0x8023F4C4..0x8023F534 was inside
// `auto_03_8023E5F4_text` (0x8023E5F4..0x80241C90), dtk's own object, so nothing of ours
// defined the symbol at all: `grep -rn SLdrActorParametersFv src/ include/` was empty, while
// module `ScriptCoin`'s `fn_58_BC8` names it as an import and `docs/goal-notes/`
// `progress-scriptcoin-fn58-35d0-rodata-58.md:130-138` records that this one DOL destructor is
// the only thing between that carve and `fn_58_C2C`. Claiming it splits the auto unit in two -
// `auto_03_8023E5F4_text` (0x8023E5F4..0x8023F4C4) and `auto_03_8023F534_text`
// (0x8023F534..0x80241C90) - which `tools/gate.sh`'s per-function diff scores as a split, not a
// loss: `total_functions` stays 28465.
//
// **What the body is.**  It is the class's own destructor, member teardowns in reverse
// declaration order and MWCC's delete-flag tail, and the header in
// `include/MetroidPrime/ScriptLoader/Structs/SLdrActorParameters.hpp` already has that member
// order - `lighting`, `scannable`, four `CAssetId`s, `useGlobalRenderTime`, two floats, `visor`
// and the rest - and that layout is retail's, read off `__ct__19SLdrActorParametersFv` (0x8023F534):
// `bl fn_802405CC` on the receiver itself, `addi r3,r31,64` for `scannable`,
// `stw r0,68(r31)..stw r0,80(r31)` for the four asset ids at +0x44..+0x50, `stb r6,84(r31)` for
// `useGlobalRenderTime` at +0x54, `stfs` at +0x58/+0x5C for the two floats, `addi r3,r31,96` for
// `visor`, and the last words at +0x70/+0x74. So the three offsets this body needs are the
// header's own: `lighting` at +0, `scannable` at +0x40, `visor` at +0x60, which are the three
// `addi r3,r30,N` / `mr r3,r30` in retail's bytes.
//
// The teardown order is `visor`, `scannable`, `lighting`, i.e. reverse declaration order over
// the only three members that have destructors; the members between them are `CAssetId`s,
// `bool`s and `float`s with nothing to run.
//
// **The three callees are named by retail, so they are called by retail's names and not as C++
// member destructors.**  `SLdrVisorParameters::~SLdrVisorParameters` is retail's `fn_8023F6C8`
// (0x8023F6C8, 0x3C) and `SLdrLightParameters::~SLdrLightParameters` is `fn_80240590`
// (0x80240590, 0x3C) - `symbols.txt` gives both the `fn_` placeholder, so neither
// `__dt__22SLdrVisorParametersFv` nor `__dt__22SLdrLightParametersFv` exists in the binary and
// writing `self->visor.~SLdrVisorParameters()` would emit a name mwldeppc cannot resolve. Only
// `SLdrScannableParameters`' is named (`__dt__23SLdrScannableParametersFv`, 0x80241530). All
// three are 60-byte deleting-destructor steps: `mr. r31,r3` / `beq`, `extsh. r0,r4` / `ble`, the
// receiver returned in r3 - the same 15 instructions as `__dt__5CMainFv` that
// `src/MetroidPrime/ScriptLoader/Carve8023E5A8.c` already reproduces.
//
// `Free__7CMemoryFPCv` (0x802CE388) is claimed by `Kyoto/Alloc/CMemory.cpp` in the DOL and
// defined under this name for the host by `src/Kyoto/Alloc/PortMwccNew.cpp`, so it is declared
// here and never defined: this unit adds no undefined symbol of its own for it.
//
// The flag is a **short** - `extsh. r0,r31`, not `extsb.` - so `flag` is `short`, and the
// epilogue's `mr r3,r30` is what makes the return type `void*`: as `void` mwcceppc drops the
// move, 27 instructions / 108 bytes, and the function is 4 bytes short of its claim.
// `if (flag > 0)` sits **inside** `if (self)`, so the receiver's `beq` lands on the epilogue.
//
// Source order is **descending by address** and load-bearing (mwcceppc emits definitions in
// reverse source order and mwldeppc keeps the object's `.text` verbatim), and this unit has one
// function, so the point does not arise here; `tools/check_decl_order.py --unit
// main/main/MetroidPrime/ScriptLoader/SLdrActorParameters` is the cheap check.

#include "MetroidPrime/ScriptLoader/Structs/SLdrActorParameters.hpp"

/** 0x8023F6C8, `symbols.txt:10169`, 0x3C = 60 bytes: `SLdrVisorParameters`' deleting-destructor
 *  step, retail's `fn_8023F6C8`.  Unclaimed, so dtk's own object defines it for this link;
 *  declared, never defined here for the matching build. */
extern "C" void fn_8023F6C8(void* self, short flag);

/** 0x80241530, `symbols.txt:10205`, 0x3C = 60 bytes: `SLdrScannableParameters`' deleting
 *  destructor, retail's own name.  Unclaimed; declared, never defined here. */
extern "C" void __dt__23SLdrScannableParametersFv(void* self, short flag);

/** 0x80240590, 0x3C = 60 bytes: `SLdrLightParameters`' deleting-destructor step, retail's
 *  `fn_80240590`.  Unclaimed; declared, never defined here. */
extern "C" void fn_80240590(void* self, short flag);

/** 0x802CE388, `symbols.txt:12992`, 0x64 = 100 bytes: retail's `CMemory::Free(void const*)`.
 *  Claimed by `Kyoto/Alloc/CMemory.cpp` (`.text` 0x802CE224..0x802CE72C) and defined under this
 *  name for the host link by `src/Kyoto/Alloc/PortMwccNew.cpp`.  Declared, never defined here. */
extern "C" void Free__7CMemoryFPCv(const void* ptr);

/** `__dt__19SLdrActorParametersFv` - retail `.text:0x8023F4C4`, 0x70 = 112 bytes: the three member
 *  teardowns in reverse declaration order, then MWCC's delete-flag free of the receiver itself.
 *  `extern "C"` is what keeps `__dt__19SLdrActorParametersFv` unmangled, which is the name
 *  `symbols.txt:10166` carries, so objdiff pairs it against retail's own. */
extern "C" void* __dt__19SLdrActorParametersFv(SLdrActorParameters* self, short flag) {
  if (self) {
    fn_8023F6C8(&self->visor, -1);
    __dt__23SLdrScannableParametersFv(&self->scannable, -1);
    fn_80240590(&self->lighting, -1);
    if (flag > 0) {
      Free__7CMemoryFPCv(self);
    }
  }
  return self;
}

#ifndef __MWERKS__
// Three host-only stand-ins, empty bodies, and they **are** empty: the three member destructors
// this body calls are unclaimed retail functions no unit of ours implements, and the port's link
// would otherwise ask for three names nothing defines. `docs/research/port_link_gap_list.md` does
// not carry `fn_8023F6C8`, `fn_80240590` or `__dt__23SLdrScannableParametersFv`, so defining
// them here keeps the port's undefined count where it is instead of adding three.
// The guard is `__MWERKS__`, not `TARGET_PC`, for the reason
// `src/MetroidPrime/Cameras/Carve801E7C14.c` records: the matching build must take all three from
// dtk's own `auto_03_8023F534_text.o`, and a second definition there would be a duplicate.
// They are deliberately not logged at run time: they are not reachability stubs, they are the
// host's stand-ins for three functions retail has and this tree has not decompiled yet, and
// nothing in the port calls into them except the destructor above, which nothing calls either.
void fn_8023F6C8(void* self, short flag) {
  (void)self;
  (void)flag;
}

void __dt__23SLdrScannableParametersFv(void* self, short flag) {
  (void)self;
  (void)flag;
}

void fn_80240590(void* self, short flag) {
  (void)self;
  (void)flag;
}
#endif