// CSandwormRelTail.cpp - Sandworm's (module 56) out-of-line template tail, lower half, .text
// 0x13CBC..0x13D8C: three deleting destructors of the one shape the module emits out of line -
// the receiver guard, the sign-extended "deleting" flag test, the payload teardown the class
// owns, then `CMemory::Free(self)`:
//
//   0x13CBC fn_56_13CBC  0x3C  nothing to tear down: the flag test and the free
//   0x13CF8 fn_56_13CF8  0x58  `rstl::single_ptr<CProjectedShadow>` - deletes `mPtr`, then frees
//   0x13D50 fn_56_13D50  0x3C  as 0x13CBC
//
// Every body is read off `build/G2ME01/Sandworm/asm/auto_00_000000DC_text.s`, and every name is the
// module's own, from `config/G2ME01/rels/Sandworm/symbols.txt`.
//
// **The claim stops short at 0x13D8C, and the two functions above that are left unclaimed on
// purpose.** `fn_56_13D8C` and `fn_56_13E18` (0x8C each) are `~reserved_vector()` instantiations
// whose bodies this compiler reproduces instruction for instruction but for the register choice -
// it keeps the induction variable in r5 and the peeled trip count in r3, retail has them the other
// way round (IV r3, temp r5), and MW's allocator does not move between the eight `GC/*` compilers
// in `build/compilers`. Claiming a range the object does not reproduce takes those bytes out of the
// module and breaks its sha1, so the gap stays unclaimed and dtk fills it from retail; see
// `docs/goal-notes/progress-twin-rel-sandworm.md` for the spellings measured. The upper half of
// the same run is `CSandwormRelTail2.cpp` (0x13EA4..0x13F74), a second unit because one unit
// cannot claim two discontiguous ranges.
//
// **Nothing in the module calls any of the three**, so they need the module's `force_active` list
// in `config/G2ME01/config.yml`; without it mwldeppc dead-strips them and the module links short.
// Same arrangement, and same reason, as `CSandBossRelTail.cpp`'s two.
//
// **The bodies are inside `#ifdef __MWERKS__` and the host branch is empty, so listing this file in
// `files.cmake` adds no undefined reference** - the arrangement `CSandBossRelTail.cpp` uses. Every
// call here leaves the module (`CMemory::Free` and `CProjectedShadow`'s destructor), and a host body
// would make the port link names it does not have.
//
// Definitions are in descending retail text order: mwcceppc emits definitions in reverse source
// order and mwldeppc keeps the object's `.text` order verbatim, so ascending would permute the
// module's bytes with objdiff still at 100% and only the module's sha1 would catch it.

#ifdef __MWERKS__

#include "MetroidPrime/CProjectedShadow.hpp"
#include "rstl/single_ptr.hpp"

/** 0x802CE388, `symbols.txt`: `CMemory::Free(void const*)`, claimed by `Kyoto/Alloc/CMemory.cpp`
 *  in the DOL. Declared under retail's own emitted spelling so the call needs no header. */
extern "C" void Free__7CMemoryFPCv(const void* ptr);

/** 0x13D50, 0x3C. The stock deleting destructor of an object with nothing to tear down: the
 *  receiver guard, the sign-extended flag test and `CMemory::Free(self)`. Its twin in
 *  `build/report.json` is `__dt__5CMainFv`, i.e. `CMain::~CMain() {}` in
 *  `src/MetroidPrime/main.cpp`, and `fn_55_1064C` in
 *  `src/MetroidPrime/ScriptObjects/CSandBossRelTail.cpp` is the same 60 bytes already built inside
 *  another REL module. The incoming flag is a **`short`**, not a `bool` - retail's test is
 *  `extsh.`, and `if (flag > 0)` has to be **inside** `if (self)` so the `beq` lands on the
 *  epilogue rather than on the `extsh.`. */
extern "C" void* fn_56_13D50(void* self, short flag) {
  if (self) {
    if (flag > 0) {
      Free__7CMemoryFPCv(self);
    }
  }
  return self;
}

/** 0x13CF8, 0x58. `rstl::single_ptr<CProjectedShadow>::~single_ptr()`: retail loads `mPtr`, sets the
 *  deleting flag and calls the element destructor, and only then tests the incoming flag and frees.
 *  The `delete` is the spelling that produces that `lwz r3,0(self) / li r4,1 / bl` triple, and it
 *  is the only one: retail's callee symbol is `__dt__16CProjectedShadowFv`, which no C++
 *  identifier can hold and which therefore cannot be declared. `~CProjectedShadow()` is declared
 *  and not defined in `include/MetroidPrime/CProjectedShadow.hpp`, so the `delete` emits the call
 *  and no copy of the destructor into this object. */
extern "C" void* fn_56_13CF8(rstl::single_ptr< CProjectedShadow >* self, short flag) {
  if (self) {
    delete self->get();
    if (flag > 0) {
      Free__7CMemoryFPCv(self);
    }
  }
  return self;
}

/** 0x13CBC, 0x3C. `fn_56_13D50`'s body under its own symbol. */
extern "C" void* fn_56_13CBC(void* self, short flag) {
  if (self) {
    if (flag > 0) {
      Free__7CMemoryFPCv(self);
    }
  }
  return self;
}

#endif