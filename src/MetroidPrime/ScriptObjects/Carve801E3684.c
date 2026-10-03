// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the addresses and
// sizes come from `config/G2ME01/symbols.txt:7799-7800`, the instructions are the ones dtk itself
// emitted into `build/G2ME01/asm/auto_03_801E3414_text.s:186-198` before the claim existed, and the
// body below is the C those bytes are the compilation of.  They are still readable with
// `build/binutils/powerpc-eabi-objdump -d --start-address=0x801E3684 --stop-address=0x801E36b0
// build/G2ME01/main.elf`; `build/G2ME01/asm/MetroidPrime/ScriptObjects/Carve801E3684.s` is this
// unit's own generated listing, i.e. our compile, so it can only confirm what retail had.
//
// .text 0x801E3684..0x801E36AC, 0x28 = 40 bytes, 1 function:
//
//   fn_801E3684    0x801E3684  0x28 = 40 bytes  10 instructions  `rstl::list<T>::push_back`
//
// **What it is.**  Retail names nothing here, so it is read off the ten instructions, which are a
// 0x10 frame, the link register, one unconditional `bl`, the epilogue, and **two instructions
// that are the whole argument marshalling**:
//
//   801E3684  stwu r1,-0x10(r1)   801E3698  bl   0x801E36AC <fn_801E36AC>
//   801E3688  mflr r0             801E369C  lwz  r0,0x14(r1)
//   801E368C  mr   r5,r4           801E36A0  mtlr r0
//   801E3690  stw  r0,0x14(r1)    801E36A4  addi r1,r1,0x10
//   801E3694  lwz  r4,0x8(r3)     801E36A8  blr
//
// `mr r5,r4` moves the incoming second argument into the *third* register and `lwz r4,0x8(r3)`
// loads the receiver's word at +8 into the second, so the call is `(receiver, word-at-+8,
// second-argument)` - one receiver, one node, one value, and no arithmetic, no test, no returned
// value.  **The +8 is `rstl::list::mEnd`** (`include/rstl/list.hpp:230`, after an empty allocator at
// +0 and `mStart` at +4), which is also what the neighbouring carve says: `Carve801E3864.c`
// measures `mStart` +4, `mEnd` +8, `mEmpty_prev` +0xC, `mEmpty_next` +0x10, `mCount` +0x14 off
// the same two lists' `do_erase`/`~list` bodies, and `Carve801E3334.cpp` writes `mStart` at +4.
//
// **The twin the seed named is exact, ten instructions out of ten.**  `fn_80007AA0` (0x80007AA0,
// `symbols.txt:73`, 0x28 = 40 bytes, `src/MetroidPrime/main.cpp:979-982`, a `Matching` unit's
// function) is these same ten words, and the only differing one is the `bl` word - both are
// `48 00 00 15`, because both call the function immediately above themselves.  That twin is
// `rstl::list<CArchitectureMessage>::push_back` written as
// `self->do_insert_before(self->end().get_node(), val)`, i.e. it is exactly the marshalling above
// with the node taken through `end()` and the callee named by its mangled template symbol; this
// file writes the same one call with retail's own placeholder name for the callee.
//
// **The callee is retail's `rstl::list<T>::do_insert_before(node*, const T&)`, measured, not
// assumed.**  `fn_801E36AC` (0x801E36AC, `symbols.txt:7800`, 0xA0 = 160 bytes, 40 instructions)
// is the same forty instructions, in the same order, as `fn_801E3374` (0x801E3374,
// `src/MetroidPrime/ScriptObjects/Carve801E3334.cpp:105`, `Matching`, 100.00%), which that unit
// has already matched as `do_insert_before` for
// `rstl::list<rstl::pair<TUniqueId, float>, rstl::rmemory_allocator>`; the two differ only in the
// value copy - `lhz r0,0(r30) / lfs f0,4(r30) / sth r0,0(r4) / stfs f0,4(r4)` there against
// `lhz r4,0(r30) / lwz r0,0(r30+4) / sth r4,0(r5) / stw r0,0x4(r5)` here, and in the one `bl`
// target.  So the callee's argument order is `(self, node, value)` in r3, r4, r5, the node is
// 0x10 bytes of `{mPrev, mNext}` with the 8-byte element at +8 (`li r3,0x10` is the allocation and
// the `addic. r5,r3,8` the placement-new offset inside it), and `mCount` at +0x14 is the last
// thing it touches.  **The element is therefore `rstl::pair<TUniqueId, int>`** - the 2-byte key
// `fn_801E36AC` copies with `lhz` and a whole word with `lwz` - and that is the one thing this
// claim does not need to spell, since `fn_801E3684` only forwards the address of the caller's
// value.
//
// **Who the receiver belongs to, and who calls this.**  `grep -rn 'bl fn_801E3684'
// build/G2ME01/asm/` finds exactly one call site, 0x801E34F4 inside `fn_801E3414` (0x801E3414,
// 0x270 bytes, unclaimed), which sets `addi r3,r30,0x44` at 0x801E34E0 - so the receiver is the
// list at +0x44 of that class - and `addi r4,r1,0x70` at 0x801E34E4, i.e. **the value arrives as a
// pointer to an 8-byte stack aggregate** it builds at 0x801E34DC..0x801E34F0 (`sth r5,0x20(r1)`
// / `sth r5,0x70(r1)` / `stw r0,0x74(r1)`, with `r5` the halfword `lhz r5,0x2(r31)` and `r0 = 0`),
// which is the 8-byte copy the callee makes from it.  `Carve801E3864.c` establishes the rest from
// the other side: the lists at +0x44 and +0x5C are members of one `CEntity`-derived class whose deleting
// destructor (`fn_801E2E30`) stores vtable `lbl_803B7640` at +0, and **the class is still
// unnamed** - nothing claims 0x801E2AF0 and no unit claims that vtable, so the directory below is
// the only thing the class has a name for.  Nothing claimed depends on this claim: the only caller
// is unclaimed dtk text.
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object `.text` order verbatim, so
// an ascending file is a permuted `.text` - 100.00% per function, `unit_fit.sh` still saying
// "fits", the link still succeeding, and a broken DOL.  Only `tools/flip_test.sh` catches that.
// One function, so the order is trivially satisfied here;
// `python3 tools/check_decl_order.py --unit MetroidPrime/ScriptObjects/Carve801E3684.c` is the
// check for it.
//
// Retail names none of these.  `symbols.txt` carries the `fn_<addr>` placeholders and this file
// reproduces those symbols verbatim, so the definitions have to stay C: a C++ one would mangle to
// `_Z<len>fn_<addr>v` and objdiff would pair nothing.  That is also why the unit is a `.c` rather
// than a `.cpp`, and why the types are spelled out here instead of being `rstl`'s - a `.c` cannot
// see a template.  The file is compiled as C for the retail build and, like every other source in
// `files.cmake`, is syntax-checked as C++ by `tools/probe_sources.sh`; the explicit `void*` is
// compile-time only and leaves the object byte-identical.
//
// Its own unit because a claim may not span an unclaimed gap, and because the functions on either
// side of this one are not trivial: below, `fn_801E3414` (0x801E3414, `symbols.txt:7798`, 0x270 =
// 624 bytes) is unclaimed, and above,
// `fn_801E36AC` (0x801E36AC, 0xA0) and `fn_801E374C` (0x801E374C, 0x118) start the next run - so the
// claim also splits that one dtk object in two, `auto_03_801E3414_text` (0x801E3414..0x801E3684,
// 624 B, 1 function) and `auto_03_801E36AC_text` (0x801E36AC..0x801E3864, 440 B, 2 functions)
// around this unit.  **The claim sits in the middle of the auto object, not at either end of it,
// so no unit boundary is created and there is no dtk link-order cycle** - the nearest claimed
// ranges are `Carve801E3334.cpp` (ends
// 0x801E3414, below) and `Carve801E3864.c` (starts 0x801E3864, above); see "The carve vein" in
// `docs/RUNNING_THE_DECOMP.md` for the carve that does fail that way, whose range began exactly
// where a neighbouring `Matching` unit's `.text` ended.
//
// The directory is retail's own, taken from the nearest claimed range: below is
// `MetroidPrime/ScriptObjects/Carve801E3334.cpp` (0x801E3334..0x801E3414) and above
// `MetroidPrime/ScriptObjects/Carve801E3864.c` (0x801E3864..0x801E3A34), so this address sits in
// that unit's neighbourhood.  For an anonymous function that is the only evidence there is, and it
// beats a lane picking the directory it happened to own.
//
// **Port.**  The host build has no `fn_801E36AC` to call - the callee's 160 bytes are retail-only
// and no host source names this function - so the `#else` half below is an announced no-op rather
// than a guess at the real work, and the `#ifndef` half is what the DOL actually compiles.  The
// declaration is inside the guard for the same reason: an unconditional one would give the port
// link a 288th undefined symbol, and `docs/research/port_link_baseline.txt` is pinned at 287.

#ifndef TARGET_PC

/** `rstl::list<T>::node` as `include/rstl/list.hpp` lays it out and `fn_801E36AC` fills it: the
 *  two links at +0/+4 and the 8-byte element at +8, which is the `addic. r5,r3,8` of the
 *  `li r3,0x10` allocation.  The element itself is `rstl::pair<TUniqueId, int>` - a halfword at +0
 *  and a word at +4, the two loads the callee copies - and is not spelled here, because this unit
 *  only forwards the caller's address of it. */
struct SCarve801E3684Node {
  struct SCarve801E3684Node* mPrev; /* +0 */
  struct SCarve801E3684Node* mNext; /* +4 */
};

/** `rstl::list<T, rmemory_allocator>` at the offsets the retail bytes read: the allocator is empty
 *  and takes no byte, `mStart` +4, **the `mEnd` this function loads at +8**, then the two self
 *  linked empty-node pointers and `mCount` +0x14. */
struct SCarve801E3684List {
  void* mAllocator;                      /* +0, empty class: no bytes of its own */
  struct SCarve801E3684Node* mStart;     /* +4 */
  struct SCarve801E3684Node* mEnd;       /* +8 */
  struct SCarve801E3684Node* mEmptyPrev; /* +0xC */
  struct SCarve801E3684Node* mEmptyNext; /* +0x10 */
  int mCount;                            /* +0x14 */
};

/** 0x801E36AC, `symbols.txt:7800`, 0xA0 = 160 bytes: retail's own placeholder for this list's
 *  `do_insert_before(node*, const T&)` - the same forty instructions as the `Matching`
 *  `fn_801E3374` in `Carve801E3334.cpp`, which is that name for
 *  `list<pair<TUniqueId, float>>`.  **Declared, never defined here**, and above this claim's end,
 *  so dtk's own `auto_*` object supplies the bytes and the `bl` at 0x801E3698 resolves to retail's
 *  address.  Nothing in this tree claims `fn_801E36AC` is decompiled. */
void fn_801E36AC(struct SCarve801E3684List* self, struct SCarve801E3684Node* node,
                 const void* val);

/** `fn_801E3684` - retail `.text:0x801E3684`, 0x28 = 40 bytes: this list's `push_back`, spelled
 *  the way the matched twin `fn_80007AA0` spells it (`src/MetroidPrime/main.cpp:979`) -
 *  `self->do_insert_before(self->end().get_node(), val)` - with `mEnd` read as a *stored* node
 *  pointer, hence the load rather than an `addi`, and the value forwarded as an address because
 *  `fn_801E36AC` copies 8 bytes out of it.  Ten instructions, no arithmetic, no test, no return
 *  value: the frame, the link register, one `bl`, and the epilogue. */
void fn_801E3684(struct SCarve801E3684List* self, const void* val);

void fn_801E3684(struct SCarve801E3684List* self, const void* val) {
  fn_801E36AC(self, self->mEnd, val);
}

#else

/* Port stand-in - named, not plausible.  See "Port" above: the `#ifndef` half is retail's real
 * body and the host has no `fn_801E36AC` to call, so this does nothing on purpose.  It is empty
 * rather than a plausible-looking forward because guessing the list insertion here would be
 * inventing behaviour no source in the port reaches. */
void fn_801E3684(void* self, const void* val) {
  (void)self;
  (void)val;
}

#endif