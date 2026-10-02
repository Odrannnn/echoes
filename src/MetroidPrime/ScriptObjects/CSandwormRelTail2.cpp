// CSandwormRelTail2.cpp - Sandworm's (module 56) out-of-line template tail, upper half, .text
// 0x13EA4..0x13F74: the three functions above the lower half's claim, all of them the same three
// shapes `CSandwormRelTail.cpp` writes:
//
//   0x13EA4 fn_56_13EA4  0x58  `rstl::single_ptr<CCollisionActorManager>` - deletes `mPtr`, frees
//   0x13EFC fn_56_13EFC  0x3C  nothing to tear down: the flag test and the free
//   0x13F38 fn_56_13F38  0x3C  as 0x13EFC
//
// **This is a second unit rather than a second range in the first because one unit cannot claim two
// discontiguous ranges** - `dtk dol split` fails with a link-order cycle when it tries, which is
// why `ScriptCoin` has six files. `fn_56_13D8C` and `fn_56_13E18` sit in the gap between the two
// claims and stay retail's; `CSandwormRelTail.cpp`'s header says why.
//
// Bodies are read off `build/G2ME01/Sandworm/asm/auto_00_000000DC_text.s`, names from
// `config/G2ME01/rels/Sandworm/symbols.txt`. All three are unreferenced, so all three are in the
// module's `force_active` list in `config/G2ME01/config.yml`.
//
// **The bodies are inside `#ifdef __MWERKS__` and the host branch is empty, so listing this file in
// `files.cmake` adds no undefined reference** - the arrangement `CSandBossRelTail.cpp` uses.
//
// Definitions are in descending retail text order: mwcceppc emits definitions in reverse source
// order and mwldeppc keeps the object's `.text` order verbatim.

#ifdef __MWERKS__

#include "MetroidPrime/CCollisionActorManager.hpp"
#include "rstl/single_ptr.hpp"

/** 0x802CE388, `symbols.txt`: `CMemory::Free(void const*)`. Declared under retail's own emitted
 *  spelling so the call needs no header. */
extern "C" void Free__7CMemoryFPCv(const void* ptr);

/** 0x13F38, 0x3C. The stock deleting destructor of an object with nothing to tear down, under its
 *  own symbol - `fn_55_1064C`'s body in `src/MetroidPrime/ScriptObjects/CSandBossRelTail.cpp`,
 *  already built inside another REL module. */
extern "C" void* fn_56_13F38(void* self, short flag) {
  if (self) {
    if (flag > 0) {
      Free__7CMemoryFPCv(self);
    }
  }
  return self;
}

/** 0x13EFC, 0x3C. `fn_56_13F38`'s body under its own symbol. */
extern "C" void* fn_56_13EFC(void* self, short flag) {
  if (self) {
    if (flag > 0) {
      Free__7CMemoryFPCv(self);
    }
  }
  return self;
}

/** 0x13EA4, 0x58. `rstl::single_ptr<CCollisionActorManager>::~single_ptr()`, as
 *  `fn_56_13CF8` is over `CProjectedShadow`: `include/MetroidPrime/CCollisionActorManager.hpp`
 *  declares `~CCollisionActorManager()` and does not define it, so the `delete` emits the call to
 *  `__dt__22CCollisionActorManagerFv` and no copy of the destructor. */
extern "C" void* fn_56_13EA4(rstl::single_ptr< CCollisionActorManager >* self, short flag) {
  if (self) {
    delete self->get();
    if (flag > 0) {
      Free__7CMemoryFPCv(self);
    }
  }
  return self;
}

#endif