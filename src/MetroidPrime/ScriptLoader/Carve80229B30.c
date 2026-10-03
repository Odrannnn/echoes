// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the addresses
// and sizes come from `config/G2ME01/symbols.txt:9816`, the instructions are the ones dtk itself
// emitted into `build/G2ME01/asm/auto_03_8022956C_text.s:460-472`, and the body below is the C
// those bytes are the compilation of.
//
// .text 0x80229B30..0x80229B58, 0x28 = 40 bytes, 1 function:
//
//   fn_80229B30    0x80229B30  0x28    10 instructions  a frame, one call, the frame out
//
// **What it is.**  A `push_back` on an `rstl::list<CEntity*>`, with that list's `do_insert_before`
// out of line at a `fn_` placeholder of its own - the same arrangement as `fn_80007AA0`
// (`src/MetroidPrime/main.cpp:979-982`, `rstl::list<CArchitectureMessage>`, 100.00% in
// `build/report.json`) and for the same reason: retail keeps `list<T>::push_back` out of line
// and reaches the element's `do_insert_before` by `bl`, so the body is a frame, the value into
// r5, `mEnd` into r4, the call, and the frame out.
//
//   80229B30  stwu r1,-0x10(r1) / mflr r0 / mr r5,r4 / stw r0,0x14(r1)
//   80229B40  lwz r4,0x8(r3)          <- self+8 is mEnd (mAllocator 0, mStart 4, mEnd 8)
//   80229B44  bl fn_80043A5C
//   80229B48  lwz r0,0x14(r1) / mtlr r0 / addi r1,r1,0x10 / blr
//
// The twin is word for word, including the operand schedule: the value reaches r5 *before* `mEnd`
// is loaded into r4, which is what "the second argument is computed after the third is placed"
// emits as, and the epilogue carries no callee-saved register.  The one difference is the `bl`
// displacement (`48 00 00 15` there, `4B E1 9F 19` here) and nothing else.
//
// **The element is 4 bytes wide, read off `fn_80043A5C` itself** (0x80043A5C, 0x90 = 144 bytes,
// `symbols.txt:1260`, `build/G2ME01/asm/MetroidPrime/CStateManager.s:15788-15826`): it allocates
// `li r3,0xc` (`:15784`), i.e. a 12-byte node = the 8 bytes of links plus a 4-byte item, and it
// stores the dereferenced argument with `lwz r0,0x0(r30)` / `stw r0,0x8(r3)` (`:15794`,
// `:1579c`) - a load and a store of one word through the reference.  A pointer is that width,
// and the caller settles it: `fn_80229AE0` (0x80229AE0, 0x50) at 0x80229B10-0x80229B18 does
// `addi r3,r31,0x4 / addi r4,r1,0x8 / bl fn_80229B30`, passing `&(a CEntity* it had just stored
// at r1+8 with `stw r4,0x8(r1)` at 0x80229AF4)`.  So the value is a pointer and the receiver is
// `this+4`.
//
// **The receiver is `CFilteredObjectList::mObjects`.**  `include/MetroidPrime/CFilteredObjectList.hpp:9-20`
// declares that class as a vtable pointer at +0, `rstl::list<CEntity*> mObjects` at +4 and
// `bool x1c_` at +0x1c, `CHECK_SIZEOF(CFilteredObjectList, 0x20)`.  `+4` is this function's
// receiver offset, and `fn_80229AE0`'s own prologue reads a vtable out of `r3` and calls its slot
// at +0xc (`lwz r12,0x0(r3)` / `lwz r12,0xc(r12)` / `mtctr r12` / `bctrl`, 0x80229AF8-0x80229B04)
// - the third slot, which for a class whose first two virtuals are the destructor pair is
// `IsQualified(const CEntity&)` at `:13` - then tests the returned pointer's tag byte
// (`clrlwi. r0,r3,24` / `beq`, 0x80229B08-0x80229B0C) before the push.  `fn_80229AE0` is retail's
// `CFilteredObjectList::AddObjectIfAbsent`, and this is the push inside it.  The class is named
// from the offset and the virtual slot rather than from the placeholder, which is why the body
// below takes a plain `struct` and not the class: nothing observable here needs the name.
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object `.text` verbatim, so an
// ascending file is a permuted `.text` - 100.00% per function and a broken DOL.  Only
// `tools/flip_test.sh` catches that.  With one function there is no order to get wrong, and
// `tools/check_decl_order.py --unit MetroidPrime/ScriptLoader/Carve80229B30.c` has nothing to
// check either.
//
// Retail names none of this.  `symbols.txt:9816` carries the placeholder
// (`fn_80229B30 = .text:0x80229B30; // type:function size:0x28`) and this file reproduces that
// symbol verbatim, so the definition has to stay C: a C++ one would mangle to
// `_Z13fn_80229B30PvPKv` and objdiff would pair nothing.  That is also why the unit is a `.c`
// rather than a `.cpp`, and why `rstl::list` is spelled out below instead of included -
// `include/rstl/list.hpp` is C++.
//
// Its own unit because a unit may not claim two discontiguous ranges in one section (dtk
// `dol split` fails with "Cyclic dependency ... link order").  The range below is still dtk's
// `auto_03_8022956C_text`: `fn_80229AE0` (0x80229AE0, 0x50 = 80 bytes) ends exactly at 0x80229B30
// and is not twin-shaped, and the one above, `fn_80229B58` (0x80229B58, 0x64 = 100 bytes), is a
// 32-instruction list walk that is not either.  0x80229B30..0x80229B58 is the whole writable run
// and this unit claims exactly it.  No claim spans a gap, and the nearest claimed boundaries are
// `MetroidPrime/ScriptLoader/Carve80229568.c` ending at 0x8022956C and
// `MetroidPrime/ScriptLoader/Carve80229BBC.c` starting at 0x80229BBC - neither adjacent, which is
// what would create the dtk link-order cycle `docs/RUNNING_THE_DECOMP.md` records at
// "0x80302BAC..0x80302BBC".
//
// The directory is retail's own, taken from the nearest claimed range: both boundaries above and
// below are `MetroidPrime/ScriptLoader/` units.  For an anonymous function that is the only
// evidence there is, and it beats a lane picking the directory it happened to own.

/** `rstl::list<CEntity*>`, spelled out because `include/rstl/list.hpp` is C++ and this unit has
 *  to be C.  The six members and their offsets are `include/rstl/list.hpp:228-233` verbatim:
 *  `rmemory_allocator` is an empty class and so contributes no bytes, which is why `mStart` is at
 *  +4 and not +8.  Only `mEnd` is read here; the rest are named so the offsets can be checked
 *  against the header rather than assumed. */
struct SCarve80229B30List {
  int mAllocatorEmpty; /* rstl::rmemory_allocator: empty, 0 bytes */
  void* mStart;
  void* mEnd;
  void* mEmpty_prev;
  void* mEmpty_next;
  int mCount;
};

/** 0x80043A5C, `config/G2ME01/symbols.txt:1260`, 0x90 = 144 bytes: retail's out-of-line
 *  `rstl::list<CEntity*>::do_insert_before`, which lives inside `CStateManager.cpp`'s own claim
 *  (`config/G2ME01/splits.txt:173`, `.text` 0x80036200..0x80044CA4).  That unit is `NonMatching`,
 *  so the DOL link takes the bytes from dtk's `build/G2ME01/obj/MetroidPrime/CStateManager.o`
 *  (`nm` puts `T fn_80043A5C` at `+0xd85c` there) and `main.elf` resolves this `bl` against it.
 *  Declared, never defined here for the matching build. */
extern void fn_80043A5C(struct SCarve80229B30List* self, void* n, const void* val);

/** `fn_80229B30` - retail `.text:0x80229B30`, 0x28 = 40 bytes: the frame, the value into r5,
 *  `mEnd` into r4, one `bl` to `fn_80043A5C`, the frame out.  `self` is `CFilteredObjectList +
 *  0x4` and `val` is a `CEntity*` by reference - 4 bytes, which is what `fn_80043A5C`'s 12-byte
 *  node and its one-word load/store say.  The order the two arguments are set up in is retail's
 *  and is what the twin `fn_80007AA0` reproduces too: r5 first, then the load into r4. */
void fn_80229B30(struct SCarve80229B30List* self, const void* val) {
  fn_80043A5C(self, self->mEnd, val);
}

#ifndef __MWERKS__
// Host-only stand-in, empty body, and it **is** one: `rstl::list<CEntity*>::do_insert_before`
// is not decompiled here - its own 144 retail bytes are inside `MetroidPrime/CStateManager.cpp`'s
// claim, that unit is a `files.cmake` source but names none of this symbol, and nothing else in
// `src/` declares or calls it (`grep -rn 'fn_80043A5C' src/ include/` matches only the
// declaration above).  It exists so the host link resolves the `bl` above, the same announced
// trade `src/MetroidPrime/Cameras/Carve801E7C14.c:119-140` makes for its `__dt__17CCameraShakerDataFv`.
// Without it the port's flat link - which carries our sources and not dtk's objects - loses
// `fn_80043A5C`, and `tools/link_check.sh --strict`, whose verdict `tools/probe_sources.sh`
// reports, names it as a new undefined symbol against a baseline of 287.  The guard is
// `__MWERKS__` rather than `TARGET_PC` for the reason `Carve801E7C14.c` records: the matching
// build must take the symbol from dtk's `CStateManager.o`, and a second definition there would be
// a duplicate.
void fn_80043A5C(struct SCarve80229B30List* self, void* n, const void* val) {
  (void)self;
  (void)n;
  (void)val;
}
#endif