// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the addresses
// and sizes come from `config/G2ME01/symbols.txt:14966-14967`, the instructions are the ones dtk
// itself emitted into `build/G2ME01/asm/auto_03_8032F31C_text.s`, and the body below is the C
// those bytes are the compilation of.  The byte evidence is the pristine disc, not our own build:
// `python3 tools/dol_read.py 0x8032F648 0xA4 orig/G2ME01/sys/main.dol`.
//
// .text 0x8032F648..0x8032F6EC, 0xA4 = 164 bytes, 2 functions:
//
//   fn_8032F648  0x8032F648  0x40  64 bytes   16 instructions
//   fn_8032F688  0x8032F688  0x64  100 bytes  25 instructions
//
// **Both are byte-shape twins of matched code elsewhere in this tree, apart from the two call
// targets, and both are byte-exact** - checked against the disc word by word, with only the two
// `bl` displacements differing (they are address-relative and are filled by the link).  The twins
// are both in `build/G2ME01/asm/MetroidPrime/CIOWinManager.s`:
//
//   fn_8032F688  is `rstl::list<CArchitectureMessage, rstl::rmemory_allocator>::erase(const
//                iterator&, const iterator&)` (`include/rstl/list.hpp:315-322`, retail
//                `0x8004958C`, 0x64 bytes) - the same 25 instructions, including the rotated
//                `while` (the `b` goes to the compare, the body sits above it), the `r0` / `r31`
//                / `r30` / `r29` assignment order, and `mr r0,r3` after the call.  Its
//                `do_erase` call is this copy's `fn_8032F884`.
//   fn_8032F648  is the same list's `clear()` (`include/rstl/list.hpp:53`, retail `0x8004954C`,
//                0x40 bytes) - the same 16 instructions: the `mr r4,r3`, the three `addi`s
//                materialising `&result` / `&begin` / `&end` at 0x10 / 0xc / 0x8, and the two
//                load-then-store pairs reading `+0x8` before `+0x4`.
//
// **The four registers `fn_8032F688` sets up in its first ten instructions are what fixes the
// parameter list.**  Retail puts `*start` in `r0`, `*end` in `r31`, the list in `r30` and the
// result pointer in `r29`, in that order, so the function takes four explicit pointers and returns
// void: the twin's 4-byte `iterator` return travels in the hidden return pointer that `r3`
// becomes, which is also why `fn_8032F648` has to move its own argument out of `r3` into `r4`
// before building `&result`.
//
// **`void* const*` is load-bearing, and it is the whole difference between 24 and 25
// instructions.**  The loads of `*start` and `*end` sit *above* the register-save block in retail
// (`lwz r0,0(r5)` right after the `lr` store, `stw r31` then `lwz r31,0(r6)`), interleaved with it
// - one save, one fill, one save, one fill.  Written with plain `void**` parameters, mwcceppc emits
// the whole save block first (`stw r31 / stw r30 / mr r30,r4 / stw r29 / mr r29,r3`) and only then
// both loads, which is 25 instructions arranged differently and therefore a 0-byte match.  Adding
// the `const` to the *pointee* is the whole fix; measured, on this body:
//
//   void** start, void** end           -> saves first, both loads after   (wrong)
//   void* const* start, void* const* end -> load, save, load, save, ...   (retail)
//
// The same `const` is what the twin gets from its parameters being `const iterator&`, and it is
// also why the `*end` value has to be in a named local (`last`): written as `while (it != *end)`
// mwcceppc keeps the *pointer* in `r31` and reloads `0(r31)` on every iteration, and the function
// comes out longer and wrong.
//
// **`fn_8032F648` reads `+0x4` and `+0x8` of the header and nothing else**, and the two stores
// into 0xc and 0x8 are the copies the `erase` call takes the address of - the twin's `begin()` and
// `end()` each return an `iterator` by value, so each needs a temporary.  Two orderings are in
// tension and only one satisfies both halves, so the locals are declared and then *assigned*
// separately:
//
//   * the stack slots follow **declaration** order - `r5` (the `start` argument) must be 0xc and
//     `r6` (the `end` argument) must be 0x8, which is `start` declared first;
//   * the loads follow **statement** order - `+0x8` is read and stored before `+0x4`, which is
//     `end` assigned first.
//
// Writing `void* start = ..., void* end = ...;` in one declaration gets the slots right and the
// loads backwards; reversing the declarations gets the loads right and the slots backwards.
// Measured, both.  Written against the fields directly (`&list->x4_begin`) there are no
// temporaries at all, and the two loads disappear.
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object `.text` verbatim, so an
// ascending file is a permuted `.text` - 100.00% per function and a broken DOL.  Only
// `tools/flip_test.sh` catches that.  Check it with
// `python3 tools/check_decl_order.py --unit Kyoto/Math/Carve8032F648.c`.
//
// Retail names none of these.  `symbols.txt` carries the `fn_<addr>` placeholders and this file
// reproduces those symbols verbatim, so the definitions have to stay C: a C++ one would mangle to
// `_Z<len>fn_<addr>v` and objdiff would pair nothing.  That is also why the unit is a `.c` rather
// than a `.cpp` (`tools/project.py:1020` gives a `.c` unit `-lang=c`).  The C++ spelling was
// measured too, because the twin is C++ and the argument looked like it might need one: an
// `extern "C"` copy with a class `iterator` and `const iterator&` parameters reaches retail's
// bytes as well, and a plain-C copy is kept because it needs no class at all.
//
// Its own unit because a claim may not span an unclaimed gap and a unit may not claim two
// discontiguous ranges in one section.  The claim starts at `fn_8032F648`, which is exactly where
// `fn_8032F5A0` (0x8032F5A0, 0xA8) ends, and stops at `fn_8032F6EC`, which `symbols.txt:14968`
// gives a size of 0x88 - so nothing above the item's two functions is taken.  Both neighbours stay
// in dtk's `auto_03_8032F31C_text`, which now runs 0x8032F31C..0x8032F648 and
// 0x8032F6EC..0x8032F8F8 as two objects.
//
// The directory is retail's own, taken from the nearest claimed ranges: below is
// `Kyoto/Math/Carve8032F2B8.c` (`.text` 0x8032F2B8..0x8032F31C) and above is
// `Kyoto/Math/Carve803359F4.c` (0x803359F4..0x80335A0C), so this address sits in the
// `Kyoto/Math` neighbourhood - the same reasoning `src/Kyoto/Math/Carve8032F2B8.c:86-89` records.

/** The header at `lbl_803E0574`: `+0` the sentinel node `fn_8032F884` relinks through, `+4` the
 *  first node, `+8` the end sentinel, `+0x14` the count `fn_8032F884` decrements.  `fn_8032F648`
 *  reads the middle two and nothing else; `x0_sentinel` is declared only to hold the offset of
 *  the first field `fn_8032F648` does read. */
struct SWorkspaceList8032F648 {
  void* x0_sentinel;
  void* x4_begin;
  void* x8_end;
};

/** `fn_8032F884` - retail `.text:0x8032F884`, 0x74 bytes (`symbols.txt:14971`): the list's
 *  unlink-one-and-free.  It takes the header in `r3` and the node in `r4`, returns the node's
 *  successor in `r3` (the `mr r3,r31` after the unlink, `r31` being the successor loaded at
 *  0x8032F8A0), decrements the count at +0x14 and is the callee of the loop in `fn_8032F688`.  It
 *  is defined in dtk's `auto_03_8032F6EC_text` - a *retail* split object only mwldeppc links - so
 *  declaring it extern opens nothing in the DOL link and the host definition is at the bottom of
 *  this file.  The port already spells the same body out by hand as `EraseNode` in
 *  `src/Kyoto/Graphics/CGraphicsHostWorkspace.cpp:47`, in an anonymous namespace, so there is no
 *  duplicate symbol to remove. */
extern void* fn_8032F884(void* list, void* node);

void fn_8032F688(void** result, void* list, void* const* start, void* const* end);

void fn_8032F648(struct SWorkspaceList8032F648* list);

void fn_8032F688(void** result, void* list, void* const* start, void* const* end) {
  void* it = *start;
  void* last = *end;
  while (it != last) {
    it = fn_8032F884(list, it);
  }
  *result = it;
}

void fn_8032F648(struct SWorkspaceList8032F648* list) {
  void* result;
  void* start;
  void* end;
  end = list->x8_end;
  start = list->x4_begin;
  fn_8032F688(&result, list, &start, &end);
}

#ifdef TARGET_PC
/* The port's link is the half of this unit that `tools/link_check.sh` gates, and it has neither
 * of the two things the DOL build gets from elsewhere: `fn_8032F884` lives in dtk's
 * `auto_03_8032F6EC_text.o`, a *retail* split object that only `mwldeppc` links.  A host build
 * asks for a symbol nothing defines and `tools/probe_sources.sh` fails when that count grows, so
 * the host definition goes here, next to the declaration it completes, and not in
 * `PortLinkStubs.cpp`: that file is generated (`tools/gen_link_stubs.py`).  This is the same trade
 * `src/MetroidPrime/Carve8019C394.c:95-120` makes for the same reason.
 *
 * It **is** a stand-in: unlinking a node means rewriting two neighbours and freeing the node, and
 * there is no honest way to run that on the host from here.  So it announces itself and returns
 * the node it was handed, which is the one thing it can do without inventing behaviour. */
#include <stdio.h>
void* fn_8032F884(void* list, void* node) {
  printf("[port-stub] fn_8032F884 list=%p node=%p\n", list, node);
  return node;
}
#endif
