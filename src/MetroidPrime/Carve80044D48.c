// Carved out of an unclaimed dtk `auto_*` run.  Every number here is measured: the addresses and
// sizes come from `config/G2ME01/symbols.txt:1264-1266`, the instructions are the ones dtk itself
// emitted into `build/G2ME01/asm/auto_03_80044CA4_text.s:60-105` before this claim existed (dtk
// stops regenerating that file for a claimed range, so it is the surviving record of retail's
// bytes; dtk now emits ours to `build/G2ME01/asm/MetroidPrime/Carve80044D48.s`), and the bodies
// below are the C those bytes are the compilation of.
//
// .text 0x80044D48..0x80044DD4, 0x8C = 140 bytes, 3 functions:
//
//   fn_80044D48  0x80044D48  0x20    8 instructions
//   fn_80044D68  0x80044D68  0x28   10 instructions
//   fn_80044D90  0x80044D90  0x44   17 instructions
//
// **What the trio is: the `rstl::reserved_vector< T, N >` copy-construction chain of a 0x164-byte
// element**, and all three bodies are byte-shape twins - same instructions, same registers, call
// targets apart - of functions already `Matching` in this tree.  A twin fixes the spelling of a
// body without a single guess, which is why this carve needed no retry:
//
//   fn_80044D48  twin of `fn_80248F0C` `src/WorldFormat/CMetroidAreaCollider.cpp:903`
//                                       (0x80248F0C, 0x20, 100%)
//   fn_80044D68  twin of `fn_80248F2C` `src/WorldFormat/CMetroidAreaCollider.cpp:893`, also
//                `fn_80004D5C` `src/MetroidPrime/Player/CGameStateBlockConstruct.cpp:12`
//                                       (0x80004D5C, 0x28, 100%)
//   fn_80044D90  twin of `fn_80248E60` `src/WorldFormat/CMetroidAreaCollider.cpp:950`
//                                       (0x80248E60, 0x44, 100%)
//
// `fn_80044D48` is the `construct` step, `fn_80044D68` the `construct_impl` step under it, and
// `fn_80044D90` the copy constructor those two exist to reach - the same three steps, in the same
// order and at the same offsets, as the sibling claim `src/MetroidPrime/Carve80044F88.c`.
//
// **`fn_80044D90` is the `reserved_vector` copy constructor itself**: store the count, then read it
// back and copy that many inline elements, then return the receiver.
//
//   80044d9c  lwz  r0,0(r4)        ; other->mCount
//   80044da4  mr   r31,r3          ; self
//   80044da8  addi r3,r4,0x4       ; other + 1, the inline element array
//   80044dac  stw  r0,0(r31)       ; self->mCount
//   80044db0  addi r5,r31,0x4      ; self + 1
//   80044db4  lwz  r4,0(r31)       ; the *reload*: the bound is self->mCount, not other->mCount
//   80044db8  bl   fn_80044DD4     ; uninitialized_copy_n(other + 1, self->mCount, self + 1)
//   80044dc0  mr   r3,r31          ; returns self
//
// The reload at 0x80044DB4 is why the copy bound below is `self->mCount` and not a saved copy of
// `other->mCount`: with the bound taken from `other` there is nothing to reload, one register is
// freed and the whole allocation shifts.  `other + 1` / `self + 1` on the 4-byte head below is
// retail's own `addi r3,r4,4` / `addi r5,r31,4`; the element stride (0x20) lives inside
// `fn_80044DD4`, whose `bl` is still above this claim.
//
// **`fn_80044D68` is the `construct_impl` step**: a null test on the destination and one call, the
// same two instructions `fn_80004D5C` spells.  **`fn_80044D48` is the `construct` step above it**:
// a frame and one unconditional `bl`, spelled as a call as in `fn_80248F0C` because the inline
// wrapper would fold the null test away and emit `fn_80044D68`'s *previous* body instead.  Its
// two parameters are fixed by its only caller, `fn_80044CDC` (0x80044CDC, 0x6C, just below the
// claim, `asm/auto_03_80044CA4_text.s`): r3 is a cursor advancing 0x164 per iteration and r29 (its
// r5) a source pointer that is the same every iteration, and neither is reloaded, so both are
// forwarded in place.
//
// **`fn_80044DD4` is declared, never defined here**, and it is above the claim so this unit must
// not define it: dtk's auto object for 0x80044DD4..0x80044F88 still supplies it to `main.dol`.
// For the port's own link the `bl` cannot be dropped (it is in retail's bytes), so it is covered by
// an announced empty-body stand-in, `stub_192` in `src/MetroidPrime/PortLinkStubs.cpp` - the same
// trade `stub_191` (`fn_80045014`, for `Carve80044F88.c`) and `stub_186` (`fn_8000408C`, for
// `Carve80004010.c`) already make.  `PortLinkStubs.cpp` is not in `configure.py`, so the stub
// cannot reach `main.dol`, and nothing here is a claim that `fn_80044DD4` is decompiled.
//
// **Source order is descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object `.text` verbatim, so an
// ascending file is a permuted `.text` - 100.00% per function and a broken DOL.  Only
// `tools/flip_test.sh` catches that.  Note that `tools/carve_diff.sh` does **not**: it compares
// instruction-by-instruction and labels our unresolved `bl` placeholders by the enclosing
// function, so a permutation still lines up and it reports `NOT byte-exact` either way.
//
// Retail names none of the three; `symbols.txt` carries the `fn_<addr>` placeholders and this file
// reproduces them verbatim, so the definitions have to stay C - a C++ spelling would mangle to
// `_Z<len>fn_<addr>v` and objdiff would pair nothing.  That is also why the unit is a `.c` rather
// than a `.cpp`.  (MWCC's C mode is C89, so no declaration in a `for`-init; the loops here do not
// need one.)  The file is compiled as C for the port's host build and, like every other source in
// `files.cmake`, is syntax-checked as C++ by `tools/probe_sources.sh` - hence the two explicit
// casts on the `fn_80044D90` call in `fn_80044D68`, which are compile-time only and leave the
// object byte-identical.
//
// The directory is retail's own, taken from the nearest claimed ranges: below is
// `MetroidPrime/CStateManager.cpp` and above `MetroidPrime/Carve80044F88.c`, both in
// `MetroidPrime/`.

/** 0x80044DD4, `symbols.txt:1267`, 0x70 = 112 bytes: the elementwise copy
 *  `uninitialized_copy_n< T*, T* >` for the 0x20-byte element, the one call the copy constructor
 *  below makes.  Its range is not part of this claim, so it is declared and never defined here.
 *  Defined for `main.dol` by dtk's auto object for 0x80044DD4..0x80044F88; for the port, see
 *  `stub_192` in `src/MetroidPrime/PortLinkStubs.cpp`. */
extern void fn_80044DD4(const void* src, int n, void* dest);

/** The head both functions below touch: the element count at +0x00 with the inline element array
 *  at +0x04.  Only the one word is read here, so the array is reached with `+ 1` on this 4-byte
 *  head rather than with a member of a size the claim does not need. */
struct SVecHead {
  int mCount;
};

/** `fn_80044D90` - retail `.text:0x80044D90`, 0x44 = 68 bytes.  Declared before it is defined
 *  because `fn_80044D68` below calls it. */
struct SVecHead* fn_80044D90(struct SVecHead* self, const struct SVecHead* other);
void fn_80044D68(void* self, const void* src);
void fn_80044D48(void* dest, const void* src);

struct SVecHead* fn_80044D90(struct SVecHead* self, const struct SVecHead* other) {
  self->mCount = other->mCount;
  fn_80044DD4(other + 1, self->mCount, self + 1);
  return self;
}

/** `fn_80044D68` - retail `.text:0x80044D68`, 0x28 = 40 bytes: the `construct_impl` step, a null
 *  test on the destination and the copy constructor.  Its twin `fn_80004D5C` is the same ten
 *  instructions. */
void fn_80044D68(void* self, const void* src) {
  if (self != 0) {
    fn_80044D90((struct SVecHead*)self, (const struct SVecHead*)src);
  }
}

/** `fn_80044D48` - retail `.text:0x80044D48`, 0x20 = 32 bytes: the `construct` step, a frame and
 *  one unconditional call.  Its only caller is `fn_80044CDC`, which copy-constructs one
 *  0x164-byte object per iteration; both arguments are forwarded in place. */
void fn_80044D48(void* dest, const void* src) {
  fn_80044D68(dest, src);
}