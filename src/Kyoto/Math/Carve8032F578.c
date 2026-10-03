// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the address and
// size come from `config/G2ME01/symbols.txt:14964`, the instructions are the ones dtk itself
// emitted into `build/G2ME01/asm/auto_03_8032F31C_text.s:185-196`, and the body below is the C
// those bytes are the compilation of.  The byte evidence is the pristine disc, not our own build:
// `python3 tools/dol_read.py 0x8032F578 0x28 orig/G2ME01/sys/main.dol`.
//
// .text 0x8032F578..0x8032F5A0, 0x28 = 40 bytes, 1 function:
//
//   fn_8032F578  0x8032F578  0x28  10 instructions
//                                    stwu r1,-0x10(r1) / mflr r0 / mr r5,r4 /
//                                    stw r0,0x14(r1) / lwz r4,0x8(r3) / bl <insert> /
//                                    lwz r0,0x14(r1) / mtlr r0 / addi r1,r1,0x10 / blr
//
// **It is a byte-shape twin of `fn_80007AA0`** (`.text 0x80007AA0`, `size:0x28`,
// `symbols.txt:149`, already matched in `src/MetroidPrime/main.cpp:979-982`), which is
// `rstl::list<CArchitectureMessage>::push_back(const CArchitectureMessage&)` written out under
// retail's placeholder name for the same reason this file is: mwcceppc otherwise emits exactly
// these 40 bytes as the weak COMDAT
// `push_back__Q24rstl55list<20CArchitectureMessage,Q24rstl17rmemory_allocator>FRC20CArchitectureMessage`,
// which objdiff cannot pair with `fn_80007AA0`.  `./tools/dis.sh 0x80007AA0 0x28` and the disc
// bytes at
// 0x8032F578 are **the same ten instructions and the same `bl` displacement** `0x15`, because in
// both copies the callee is the very next function: the twin's
// `do_insert_before__Q24rstl55list<20CArchitectureMessage,Q24rstl17rmemory_allocator>FPQ34rstl55list<20CArchitectureMessage,Q24rstl17rmemory_allocator>4nodeRC20CArchitectureMessage`
// (0x80007AC8, `symbols.txt:150`) sits +0x28 from its caller, and `fn_8032F5A0` (0x8032F5A0,
// `symbols.txt:14965`, `size:0xA8`) sits +0x28 from this one.
//
// **The header is the global at `lbl_803E0574`, and `+0x8` is its end sentinel.**  The `lwz
// r4,0x8(r3)` is `end().get_node()` - the stored node pointer, hence the load and not an `addi` -
// which the twin spells `self->end().get_node()`.  Two siblings in this same neighbourhood read the
// same header and fix its shape: `src/Kyoto/Math/Carve8032F648.c` reads `+0x4` and `+0x8` of the
// object it is handed, and `fn_8032F31C` (0x8032F31C..0x8032F3CC, the pump above this claim in the
// same dtk range, handed `lbl_803E0574` in `r30` at 0x8032F33C) reads `+0x14` as the element count
// and then, per node, `+0x8` and `+0xc` (two words) and `+0x10` (a halfword).  So the header is
// retail's 24-byte `rstl::list` header: two sentinel nodes at `+0`/`+4` and `+0x8`/`+0xc`, and the
// count at `+0x14`.  The one word in between, `+0x10`, is the allocator in retail's header, but
// nothing measured here reads it - `fn_8032F5A0` allocates through
// `allocate__Q24rstl17rmemory_allocatorFi` in `r3` with no operand read from the header - so this
// file claims nothing about it.
//
// **What the one call site says the arguments are**, measured at 0x8032F54C inside `fn_8032F404`
// (`build/G2ME01/asm/auto_03_8032F31C_text.s:170`): `r3` is `lbl_803E0574`, and `r4` is the address
// of a 10-byte temporary built there by `stw r28,0x8(r1) / stw r29,0xc(r1) / sth r0,0x10(r1)` -
// so the element is a 10-byte `{word, word, halfword}` record, passed by address because mwcceppc
// passes an aggregate of that size that way.  `fn_8032F5A0` copies exactly those three stores into
// the new node at `+0x8`, and `fn_8032F31C` reads them back from the same offsets.  `r5` is dead at
// the call: this function takes two parameters, and `mr r5,r4` is the value pointer moving to the
// insert's third argument.
//
// **None of that has to be spelled, and that is what lets this be a `.c`.**  The function reads
// one word of the header and forwards two pointers; the element type is never named, never sized
// and never dereferenced here, so `void*` is the whole of what the bytes require - no class has to
// be invented for it.  `tools/project.py:1020` gives a `.c` unit `-lang=c`, and retail's symbol map
// carries the `fn_<addr>` placeholder (`symbols.txt:14964`), which a C definition reproduces
// verbatim where a C++ one would mangle to `_Z<len>fn_<addr>v` and objdiff would pair nothing.
//
// **The `mr r5,r4` before the `lwz` is the whole of the argument order**, and reading the fields
// in source order is what produces it: `val` arrives in `r4`, the load of `+0x8` clobbers `r4`, so
// the value pointer has to move to `r5` (the insert's third argument) before `end()` is loaded.
// Written as `fn_8032F5A0(list, list->x8_end, val)` that is what mwcceppc emits - measured with
// `tools/carve_diff.sh 0x8032F578 0x28 build/G2ME01/src/Kyoto/Math/Carve8032F578.o`, whose only
// reported difference is the `bl` displacement, because an object is compared before the link
// fills it in; the displacement lands on `0x15` and the whole claim on retail's bytes once
// `mwldeppc` links (`87 files OK`, and `tools/flip_test.sh Kyoto/Math/Carve8032F578.c`).
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object `.text` verbatim, so an
// ascending file is a permuted `.text` - 100.00% per function, objdiff still happy,
// `unit_fit.sh` still "fits", the link still succeeding, and a broken DOL.  Only
// `tools/flip_test.sh` catches that.  There is one function here, so there is nothing to order,
// but the rule is checked anyway with
// `python3 tools/check_decl_order.py --unit Kyoto/Math/Carve8032F578.c`.
//
// Its own unit because a claim may not span an unclaimed gap and a unit may not claim two
// discontiguous ranges in one section.  The claim starts at `fn_8032F578`, which is exactly where
// `fn_8032F404` (0x8032F404, `size:0x174`) ends, and stops at 0x8032F5A0, which is where
// `fn_8032F5A0` begins - so the insert itself, and everything above it to the sibling carve's
// start, stays unclaimed, and dtk splits that range in two around this claim: measured after the
// build, `auto_03_8032F31C_text` is now 0x8032F31C..0x8032F578 and a new `auto_03_8032F5A0_text`
// holds 0x8032F5A0..0x8032F648.  No existing unit boundary is touched, which is the case that
// would fail `dtk dol split` with a link-order cycle (`docs/RUNNING_THE_DECOMP.md`, "The carve
// vein").
//
// The directory is retail's own, taken from the nearest claimed ranges: below is
// `Kyoto/Math/Carve8032F2B8.c` (`.text` 0x8032F2B8..0x8032F31C) and above is
// `Kyoto/Math/Carve8032F648.c` (0x8032F648..0x8032F6EC), so this address sits in the
// `Kyoto/Math` neighbourhood - the same reasoning `src/Kyoto/Math/Carve8032F2B8.c:86-89` records.
// `grep -rn "fn_8032F578" src/ include/` outside this file finds nothing, so there is no
// `PortLinkStubs.cpp` duplicate to remove.

/** The list header at `lbl_803E0574`.  Only `x8_end` is read here; `x0_sentinel` and `x4_begin` are
 *  declared so that `x8_end` is at retail's `+0x8`, and the same three-word shape is spelled by
 *  the sibling carve `src/Kyoto/Math/Carve8032F648.c`. */
struct SWorkspaceList8032F578 {
  void* x0_sentinel;
  void* x4_begin;
  void* x8_end;
};

/** `fn_8032F5A0` - retail `.text:0x8032F5A0`, 0xA8 = 168 bytes (`symbols.txt:14965`): this list's
 *  `do_insert_before`.  Its own body allocates a 20-byte node, copies the 10-byte element to `+0x8`
 *  of it, relinks it between `pos` and its successor and bumps the count at `+0x14`; none of that
 *  is in this claim.  Declared only: the DOL link resolves the name from dtk's
 *  `auto_03_8032F5A0_text`, the *retail* split object the function still lives in, and the host
 *  definition is at the bottom of this file. */
extern void fn_8032F5A0(void* list, void* pos, const void* val);

/** `fn_8032F578` - retail `.text:0x8032F578`, 0x28 = 40 bytes: a 0x10 frame, the value pointer
 *  moved out of `r4` into `r5`, the end sentinel loaded from the header's `+0x8`, one call and the
 *  frame out.  Twin of `fn_80007AA0` (0x80007AA0, 0x28), which is `rstl::list`'s `push_back` for
 *  a different element type; the body below is that `push_back` spelled in C for this copy. */
void fn_8032F578(struct SWorkspaceList8032F578* list, const void* val) {
  fn_8032F5A0(list, list->x8_end, val);
}

#ifdef TARGET_PC
/* The port's link is the half of this unit that `tools/link_check.sh` gates, and it lacks the one
 * thing the DOL build gets from elsewhere: `fn_8032F5A0` lives in `auto_03_8032F5A0_text`, a
 * *retail* split object that only `mwldeppc` links, so a host build asks for a symbol nothing
 * defines, and `tools/probe_sources.sh` fails when that count grows - measured on this branch head
 * as 287 undefined (`build/goal/judge/undef.base.count`), with neither `fn_8032F5A0` nor
 * `fn_8032F578` among them (`build/goal/judge/undef.base.txt`), so the call above would open one.
 * The host definition goes here, next to the declaration it completes, and not in
 * `PortLinkStubs.cpp`: that file is generated (`tools/gen_link_stubs.py`).  This is the same trade
 * `src/Kyoto/Math/Carve8032F648.c` makes for `fn_8032F884`, the same list's unlink-one-and-free,
 * one carve below.
 *
 * It **is** a stand-in and it cannot honestly be otherwise from here: inserting a node means
 * allocating 20 bytes through `allocate__Q24rstl17rmemory_allocatorFi`, which has no host
 * definition anywhere in `src/` (only declarations, in the carves that call it), so writing the
 * real body would move the gap onto the allocator instead.  So it announces itself, which is the
 * one thing it can do without inventing behaviour. */
#include <stdio.h>
void fn_8032F5A0(void* list, void* pos, const void* val) {
  printf("[port-stub] fn_8032F5A0 list=%p pos=%p val=%p\n", list, pos, val);
}
#endif
