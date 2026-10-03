// CIngBoostBallGuardian3790.cpp - IngBoostBallGuardian's (module 30) three null-guarded
// destructors, `.text` 0x3790..0x388C: one chain, 0xFC bytes.
//
//   0x3790 fn_30_3790  0x50  20 instructions  releases the rstl::string at +0
//   0x37E0 fn_30_37E0  0x58  22 instructions  releases the sub-object at +4
//   0x3838 fn_30_3838  0x54  21 instructions  frees the pointer at +0xC
//
// Addresses and sizes are `config/G2ME01/rels/IngBoostBallGuardian/symbols.txt:67-69`; the
// instructions are the ones dtk itself emits into
// `build/G2ME01/IngBoostBallGuardian/obj/auto_00_000020A0_text.o`, where this range is section
// offset 0x16F0, and they are byte-identical to `orig/G2ME01/files/RelProd/IngBoostBallGuardian.rel`'s
// `.text 0x3790..0x388C` (the module's `.text` starts at file offset 0xDC).
//
// **One deleting-destructor chain, and the `short` flag is load-bearing.** All three are
// `if (self) { <teardown>; if (flag > 0) Free__7CMemoryFPCv(self); } return self;` - MWCC's
// deleting-destructor convention, the same shape `src/MetroidPrime/Carve8000447C.cpp:193` and
// `src/MetroidPrime/Cameras/Carve801E7C14.c:98` reproduce in the DOL. Three measured facts about
// it:
//   - `flag > 0` compiles to `extsh. r0,r31 / ble` with the flag still in r31 across the inner
//     call, so the flag is a **short**; an `int` would give `cmpwi r31,0`.
//   - the receiver is `mr. r30,r3 / beq <epilogue>` and `return self` is the `mr r3,r30` in the
//     middle of the epilogue - so all three return their receiver, they are not void;
//   - the `beq` lands on the epilogue, i.e. it guards the teardown **and** the flag test. That is
//     what tells the teardown apart from a destructor call: an explicit `p->~T()` compiles to its
//     own null test that skips *only* the `bl` (measured: 0x54 bytes against retail's 0x50 for
//     `fn_30_3790` alone). Retail's inner calls are unguarded member/direct calls, so the string
//     teardown below is written as the member call it is rather than as a destructor call.
//
// - `fn_30_3790`'s teardown is `bl internal_dereference__Q24rstl66basic_string<c,...>Fv` on **r3
//   unchanged** - the string is at +0 and `include/rstl/string.hpp:193` spells `~basic_string()`
//   as `{ internal_dereference(); }`. That name is `config/G2ME01/symbols.txt:13842` (0x802FE9B8,
//   0x40) and it is **not declarable**: the `<`, `,` and `:` in it are not identifier characters,
//   so neither a `.c` nor a `.cpp` can declare the symbol, and mwcceppc has no `__asm__` symbol
//   renaming (`__asm__("...")` on the declarator is read as the global-register-variable extension
//   and fails with "type cannot be made into a global register variable" - measured). What works
//   is the arrangement `src/MetroidPrime/Carve8000447C.cpp:96-127` already uses for `rstl::rc_ptr`:
//   **declare the class template locally, with only the member the bytes call, and never define
//   it**, so the one call mangles to retail's own MWCC symbol and resolves against the DOL's
//   definition. The local `char_traits` / `rmemory_allocator` are there because the mangled name
//   spells the template arguments; the built object's `.rela.text` naming
//   `internal_dereference__Q24rstl66basic_string<c,Q24rstl14char_traits<c>,Q24rstl17rmemory_allocator>Fv`
//   is the proof that the name came out right, and nothing is emitted for the declaration.
// - `fn_30_37E0` passes `addi r3,r30,4 / li r4,-1` - the member at +4 and MWCC's explicit
//   "destroy but do not free" flag - so the callee is the next function down in this file.
// - `fn_30_3838` is the only one that reads the object: `lwz r3,0xC(r30)` then
//   `bl Free__7CMemoryFPCv`, so +0xC is a pointer the receiver owns outright, not a
//   reference-counted one (an `rstl::rc_ptr` release branches before it calls).
//
// The layout below is **only what the bytes read**: nothing here claims the class is named, and
// `SRelSub`'s three opaque words before +0xC exist so that `&owner->mSub` lands on +4 and
// `sub->mOwned` lands on +0xC. Both are measurable; the names are not.
//
// **`fn_30_3790` needs a `force_active:` entry, and that is measured.** `nm` over every object
// dtk writes into `build/G2ME01/IngBoostBallGuardian/obj/` finds no `U fn_30_3790` - the chain is
// entered from nowhere in the module - so mwldeppc dead-strips it out of this unit's `.text` and
// every function above it shifts by its 0x50 bytes. Measured without the entry: the module came
// out with `.text` 0x16698 against retail's 0x166E8, 80 bytes short, and the byte stream from
// 0x3790 on was retail's from 0x37E0 on. `config/G2ME01/config.yml`'s module-30 block now carries
// `force_active: [fn_30_3790]`, which is where mwldeppc's FORCEACTIVE list comes from - the same
// fix FlyerSwarm's `fn_21_1708` and PlantScarabSwarm's `fn_49_2C00` use there. `fn_30_37E0` and
// `fn_30_3838` need nothing: four objects name `fn_30_37E0` and `fn_30_37E0` names `fn_30_3838`.
//
// Source order is **descending by retail text offset** and that is load-bearing: mwcceppc emits
// definitions in reverse source order and mwldeppc keeps the object's `.text` order verbatim, so
// an ascending file permutes the module's bytes and breaks its sha1 with objdiff still at 100%.
// Only `tools/flip_test.sh` catches that; with one function the question does not arise, with
// three it does.
//
// In `files.cmake` with an empty host branch, as `CIngBoostBallGuardianBits.cpp` explains, so the
// port link sees no undefined symbol from the two calls: both `Free__7CMemoryFPCv`
// (`src/Kyoto/Alloc/PortMwccNew.cpp`) and `internal_dereference__Q24rstl66basic_string<...>Fv`
// (`src/rstl/rstl_strings.cpp:150`) are already defined by units that are in `files.cmake`.

#ifdef __MWERKS__

namespace rstl {

/** The one member of `rstl::char_traits` / `rstl::rmemory_allocator` this unit's mangled name
 *  needs: both appear in retail's
 *  `internal_dereference__Q24rstl66basic_string<c,Q24rstl14char_traits<c>,Q24rstl17rmemory_allocator>Fv`
 *  and nowhere else in this file.  Declared, never defined. */
template < typename T > struct char_traits;
class rmemory_allocator;

/** `rstl::basic_string` carrying only what `fn_30_3790` calls.  **Declared, never defined**
 *  here, deliberately: the real `include/rstl/string.hpp` makes `internal_dereference` private,
 *  and reaching it through the real header means writing an explicit destructor call, which costs
 *  a second `beq` (measured, 0x54 against retail's 0x50).  Retail's own bytes have no such test,
 *  so retail called the member function - and a local declaration is the way to say so, the
 *  arrangement `src/MetroidPrime/Carve8000447C.cpp` documents for `rstl::rc_ptr`.  Nothing is
 *  emitted for the declaration and no `rstl` header is included, so no other unit sees this. */
template < typename T, typename Traits, typename Alloc >
class basic_string {
public:
  void internal_dereference();
};

} // namespace rstl

typedef rstl::basic_string< char, rstl::char_traits< char >, rstl::rmemory_allocator > RelString;

// 0x802CE388, `config/G2ME01/symbols.txt:12992`: `CMemory::Free(void const*)`, claimed by
// `Kyoto/Alloc/CMemory.cpp` in the DOL.  Declared, never defined here.
extern "C" void Free__7CMemoryFPCv(const void* ptr);

// The +4 sub-object `fn_30_37E0` hands to `fn_30_3838`; only +0xC is ever read.
struct SRelSub {
  unsigned int mWord00;
  unsigned int mWord04;
  unsigned int mWord08;
  void* mOwned;
};

// Its owner: the sub-object sits at +4, which is the whole of what `fn_30_37E0` says about it.
struct SRelOwner {
  unsigned int mWord00;
  SRelSub mSub;
};

extern "C" SRelSub* fn_30_3838(SRelSub* self, short flag);
extern "C" SRelOwner* fn_30_37E0(SRelOwner* self, short flag);
extern "C" RelString* fn_30_3790(RelString* self, short flag);

// .text 0x3838, 0x54 bytes.  Frees the owned pointer at +0xC, then the receiver behind the flag.
extern "C" SRelSub* fn_30_3838(SRelSub* self, short flag) {
  if (self) {
    Free__7CMemoryFPCv(self->mOwned);
    if (flag > 0) {
      Free__7CMemoryFPCv(self);
    }
  }
  return self;
}

// .text 0x37E0, 0x58 bytes.  The `li r4,-1` is MWCC's explicit destructor call: the sub-object is
// torn down but not freed, because the receiver's own flag is what decides that.
extern "C" SRelOwner* fn_30_37E0(SRelOwner* self, short flag) {
  if (self) {
    fn_30_3838(&self->mSub, -1);
    if (flag > 0) {
      Free__7CMemoryFPCv(self);
    }
  }
  return self;
}

// .text 0x3790, 0x50 bytes.  The string is at +0, so the teardown is the member call and nothing
// else - no address arithmetic, and no null test of its own.
extern "C" RelString* fn_30_3790(RelString* self, short flag) {
  if (self) {
    self->internal_dereference();
    if (flag > 0) {
      Free__7CMemoryFPCv(self);
    }
  }
  return self;
}

#endif // __MWERKS__
