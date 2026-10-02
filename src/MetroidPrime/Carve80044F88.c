// Carved out of an unclaimed dtk `auto_*` run.  Every number here is measured: the addresses and
// sizes come from `config/G2ME01/symbols.txt:1272-1274`, the instructions are the ones dtk itself
// emitted into `build/G2ME01/asm/auto_03_80044CA4_text.s:244-289` before this claim existed (dtk
// stops regenerating that file for a claimed range, so it is the surviving record of retail's
// bytes; dtk now emits ours to `build/G2ME01/asm/MetroidPrime/Carve80044F88.s`), and the bodies
// below are the C those bytes are the compilation of.
//
// .text 0x80044F88..0x80045014, 0x8C = 140 bytes, 3 functions:
//
//   fn_80044F88  0x80044F88  0x20    8 instructions
//   fn_80044FA8  0x80044FA8  0x28   10 instructions
//   fn_80044FD0  0x80044FD0  0x44   17 instructions
//
// **What the trio is: the `rstl::reserved_vector< T, N >` copy-construction chain of a 0x2C-byte
// element**, and all three bodies are byte-shape twins - same instructions, same registers, call
// targets apart - of functions already `Matching` in this tree.  A twin fixes the spelling of a
// body without a single guess, which is why this carve needed no retry:
//
//   fn_80044F88  twin of `__sys_free`   `src/MetroidPrime/main.cpp:396`  (0x80008A28, 0x20, 100%)
//   fn_80044FA8  twin of `fn_80004D5C`  `src/MetroidPrime/Player/CGameStateBlockConstruct.cpp:12`
//                                       (0x80004D5C, 0x28, 100%)
//   fn_80044FD0  twin of `fn_80248E60`  `src/WorldFormat/CMetroidAreaCollider.cpp:950`
//                                       (0x80248E60, 0x44, 100%)
//
// Comparing the instruction *encodings* - dtk's asm for **our** object against the twin's asm for
// the retail bytes - reports **0** differing words for `fn_80044FD0` (even its `bl` encodes
// identically: the displacement is the same 0x1C there and here) and **1** for each of the other
// two, the `bl` (`48 00 00 15` here against the twin's own call).
//
// **`fn_80044FD0` is the `reserved_vector` copy constructor itself**: store the count, then read it
// back and copy that many inline elements, then return the receiver.
//
//   80044fdc  lwz  r0,0(r4)        ; other->mCount
//   80044fe4  mr   r31,r3          ; self
//   80044fe8  addi r3,r4,0x4       ; other + 1, the inline element array
//   80044fec  stw  r0,0(r31)       ; self->mCount
//   80044ff0  addi r5,r31,0x4      ; self + 1
//   80044ff4  lwz  r4,0(r31)       ; the *reload*: the bound is self->mCount, not other->mCount
//   80044ff8  bl   fn_80045014     ; uninitialized_copy_n(other + 1, self->mCount, self + 1)
//   80045000  mr   r3,r31          ; returns self
//
// The reload at 0x80044FF4 is why the copy bound below is `self->mCount` and not a saved copy of
// `other->mCount`: with the bound taken from `other` there is nothing to reload, one register is
// freed and the whole allocation shifts.  `other + 1` / `self + 1` on the 4-byte head below is
// retail's own `addi r3,r4,4` / `addi r5,r31,4`; the element stride (0x2C) lives inside
// `fn_80045014`, whose `bl` is still above this claim.
//
// **`fn_80044FA8` is the `construct_impl` step**: a null test on the destination and one call, the
// same two instructions `fn_80004D5C` spells.  **`fn_80044F88` is the `construct` step above it**:
// a frame and one unconditional `bl`, spelled as a call as in `__sys_free` because the inline
// wrapper would fold the null test away and emit `fn_80044FA8`'s *previous* body instead.  Its
// two parameters are fixed by its only caller, `fn_80044F1C` (0x80044F1C, 0x6C, just below the
// claim, `asm/auto_03_80044CA4_text.s:227`): r3 is a cursor advancing 0x1E8 per iteration and r4 a
// second pointer, and neither is reloaded, so both are forwarded in place.
//
// **`fn_80045014` is declared, never defined here**, and it is above the claim so this unit must
// not define it: dtk's auto object for 0x80045014..0x80045CD4 still supplies it to `main.dol`.  For
// the port's own link the `bl` cannot be dropped (it is in retail's bytes), so it is covered by an
// announced empty-body stand-in, `stub_191` in `src/MetroidPrime/PortLinkStubs.cpp` - the same
// trade `stub_186` (`fn_8000408C`, for `Carve80004010.c`) and `stub_182` (`fn_80008D68`, for
// `Carve800045A0.c`) already make.  `PortLinkStubs.cpp` is not in `configure.py`, so the stub
// cannot reach `main.dol`, and nothing here is a claim that `fn_80045014` is decompiled.
//
// **Source order is descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object `.text` verbatim, so an
// ascending file is a permuted `.text` - 100.00% per function and a broken DOL.  Only
// `tools/flip_test.sh` catches that.
//
// Retail names none of the three; `symbols.txt` carries the `fn_<addr>` placeholders and this file
// reproduces them verbatim, so the definitions have to stay C - a C++ spelling would mangle to
// `_Z<len>fn_<addr>v` and objdiff would pair nothing.  That is also why the unit is a `.c` rather
// than a `.cpp`.  (MWCC's C mode is C89, so no declaration in a `for`-init; the loops here do not
// need one.)  The file is compiled as C for the port's host build and, like every other source in
// `files.cmake`, is syntax-checked as C++ by `tools/probe_sources.sh` - hence the two explicit
// casts on the `fn_80044FD0` call in `fn_80044FA8`, which are compile-time only and leave the
// object byte-identical (measured: the same three relocations, 140 bytes, before and after).
//
// The directory is retail's own, taken from the nearest claimed ranges: below is
// `MetroidPrime/Carve80004744.c`/`CStateManager.cpp` and above `MetroidPrime/Carve80045CD4.c`,
// both in `MetroidPrime/`.

/** 0x80045014, `symbols.txt:1275`, 0x68 = 104 bytes: the elementwise copy
 *  `uninitialized_copy_n< T*, T* >` for the 0x2C-byte element, the one call the copy constructor
 *  below makes.  Its range is not part of this claim, so it is declared and never defined here.
 *  Defined for `main.dol` by dtk's auto object for 0x80045014..0x80045CD4; for the port, see
 *  `stub_191` in `src/MetroidPrime/PortLinkStubs.cpp`. */
extern void fn_80045014(const void* src, int n, void* dest);

/** The head both functions below touch: the element count at +0x00 with the inline element array
 *  at +0x04.  Only the one word is read here, so the array is reached with `+ 1` on this 4-byte
 *  head rather than with a member of a size the claim does not need. */
struct SVecHead {
  int mCount;
};

/** `fn_80044FD0` - retail `.text:0x80044FD0`, 0x44 = 68 bytes.  Declared before it is defined
 *  because `fn_80044FA8` below calls it. */
struct SVecHead* fn_80044FD0(struct SVecHead* self, const struct SVecHead* other);
void fn_80044FA8(void* self, const void* src);
void fn_80044F88(void* dest, const void* src);

struct SVecHead* fn_80044FD0(struct SVecHead* self, const struct SVecHead* other) {
  self->mCount = other->mCount;
  fn_80045014(other + 1, self->mCount, self + 1);
  return self;
}

/** `fn_80044FA8` - retail `.text:0x80044FA8`, 0x28 = 40 bytes: the `construct_impl` step, a null
 *  test on the destination and the copy constructor.  Its twin `fn_80004D5C` is the same ten
 *  instructions. */
void fn_80044FA8(void* self, const void* src) {
  if (self != 0) {
    fn_80044FD0((struct SVecHead*)self, (const struct SVecHead*)src);
  }
}

/** `fn_80044F88` - retail `.text:0x80044F88`, 0x20 = 32 bytes: the `construct` step, a frame and
 *  one unconditional call.  Its only caller is `fn_80044F1C`, which builds one 0x1E8-byte object
 *  per iteration; both arguments are forwarded in place. */
void fn_80044F88(void* dest, const void* src) {
  fn_80044FA8(dest, src);
}
