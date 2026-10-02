// Carved out of an unclaimed dtk `auto_*` run.  Every number here is measured: the addresses and
// sizes come from `config/G2ME01/symbols.txt:1275-1276`, the instructions are the ones dtk itself
// emitted into `build/G2ME01/asm/auto_03_80045014_text.s:9-50` before this claim existed (dtk
// stops regenerating that file for a claimed range, so it is the surviving record of retail's
// bytes; dtk now emits ours to `build/G2ME01/asm/MetroidPrime/Carve80045014.s`), and the bodies
// below are the C those bytes are the compilation of.
//
// .text 0x80045014..0x8004509C, 0x88 = 136 bytes, 2 functions:
//
//   fn_80045014  0x80045014  0x68   26 instructions
//   fn_8004507C  0x8004507C  0x20    8 instructions
//
// **What the pair is: the tail of the `rstl::reserved_vector< T, N >` copy-construction chain of
// a 0x2C-byte element whose head is the committed carve `src/MetroidPrime/Carve80044F88.c`**
// (`fn_80044FD0` there is the copy constructor and calls `fn_80045014` at 0x80044FF8), so the
// call chain across the two claims is copy constructor -> `uninitialized_copy_n` -> per-element
// copy-construct -> the 0x28-byte copy that is still above this claim.
//
//   fn_80045014  is `rstl::uninitialized_copy_n< const T*, T* >` over that 0x2C-byte element: a
//                bottom-tested loop that copy-constructs each element and returns the end cursor.
//                It is the byte-shape twin of `fn_80248EA4` in `src/WorldFormat/CMetroidAreaCollider.cpp`
//                (0x80248EA4, 0x68 = 104 bytes, 100% in `build/report.json`), the same loop for a
//                0x24-byte element,
//                with exactly two immediates changed: the stride is 0x2C instead of 0x24, and the
//                element copy-construct is `fn_8004507C` instead of `fn_80248F0C`.  Everything
//                else - the "count test at the bottom" entry, the `addi r29,r29,-1` /
//                `addi r31,r31,0x2C` / `addi r30,r30,0x2C` advance after the call, the
//                `mr r3,r30` return of the end cursor, and the three non-volatile cursors that
//                the 0x20 frame is for - is the twin's, instruction for instruction.
//
//   fn_8004507C  is the per-element `construct` step: a frame and one unconditional `bl` to
//                `fn_8004509C` at 0x8004509C, both arguments forwarded in place.  Its twin is
//                `fn_80248F0C` in the same file (0x80248F0C, 0x20 = 32 bytes, 100% in the same
//                report), which
//                the source above spells as `fn_80248F2C(dest, src);` - a call and not an inline
//                wrapper, because the inline wrapper would fold the null test in and emit its
//                callee's body instead.
//
// **Both callee arguments are forwarded in place**, so the C spelling below takes them as
// pointers: `fn_80045014(cur, it)` compiles to `mr r3,r30 / mr r4,r31 / bl`, which is the twin's
// own reference-passing (`fn_80248F0C(cur, *it)`) with the address already in the register.
//
// **The pair's only caller below the claim is `fn_80044FD0`** (the copy constructor in
// `Carve80044F88.c`), which reaches `fn_80045014` with `other + 1`, `self->mCount` and `self + 1`;
// `fn_8004507C` is called by this file's own `fn_80045014` and by `fn_80045160` at 0x80045198,
// which is **above** this claim and so is not ours.  `fn_8004509C` is likewise above the claim
// (`0x8004509C`, 0x28) and is **declared, never defined here**: dtk's auto object for
// 0x8004509C..0x80045CD4 still supplies it to `main.dol`.  For the port's own link the `bl`
// cannot be dropped (it is in retail's bytes), so it is covered by an announced empty-body
// stand-in, `stub_193` in `src/MetroidPrime/PortLinkStubs.cpp` - the same trade `stub_191`
// (`fn_80045014`, now defined by this unit, so that stub is deleted with this claim) and
// `stub_192` (`fn_80044DD4`, for `Carve80044D48.c`) already make.  `PortLinkStubs.cpp` is not in
// `configure.py`, so the stub cannot reach `main.dol`, and nothing here is a claim that
// `fn_8004509C` is decompiled.
//
// **Source order is descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object `.text` verbatim, so an
// ascending file is a permuted `.text` - 100.00% per function and a broken DOL.  Only
// `tools/flip_test.sh` catches that.
//
// Retail names neither function; `symbols.txt` carries the `fn_<addr>` placeholders and this file
// reproduces them verbatim, so the definitions have to stay C - a C++ spelling would mangle to
// `_Z<len>fn_<addr>v` and objdiff would pair nothing.  That is also why the unit is a `.c` rather
// than a `.cpp`.  (MWCC's C mode is C89, so no declaration in a `for`-init: `remaining` is hoisted
// out of the loop below, which costs no instruction - `int n` is a parameter and the hoisted
// initialiser is the same `mr r29,r4` retail has at 0x80045034.)
//
// The directory is retail's own, taken from the nearest claimed ranges: below is
// `MetroidPrime/Carve80044F88.c` and above `MetroidPrime/Carve80045CD4.c`, both in
// `MetroidPrime/`.

/** Retail's element: 0x2C = 44 bytes, the unit `fn_80045014` advances both cursors by.  Only the
 *  size is load-bearing here - every field is reached through the callee that is still above the
 *  claim - so the array is spelled opaque. */
struct SElem44 {
  unsigned int m[11];
};

/** `fn_8004507C` - retail `.text:0x8004507C`, 0x20 = 32 bytes: the element `construct` step, a
 *  frame and one unconditional call.  Declared before it is defined because `fn_80045014` below
 *  calls it. */
void fn_8004507C(void* dest, const struct SElem44* src);

/** `fn_8004509C` - retail `.text:0x8004509C`, 0x28 = 40 bytes, **above this claim**: declared
 *  here because `fn_8004507C`'s `bl` is in retail's bytes and cannot be dropped, never defined. */
extern void fn_8004509C(void* dest, const void* src);

/** `fn_8004507C` - the per-element copy-construct step.  Twin `fn_80248F0C`,
 *  `src/WorldFormat/CMetroidAreaCollider.cpp`.  Both parameters are forwarded untouched: retail's
 *  body is `stwu`/`mflr`/`stw` / `bl fn_8004509C` / `lwz`/`mtlr`/`addi`/`blr`, so it takes them as
 *  pointers and does no register work of its own. */
void fn_8004507C(void* dest, const struct SElem44* src) {
  fn_8004509C(dest, src);
}

/** `fn_80045014` - retail `.text:0x80045014`, 0x68 = 104 bytes, 26 instructions.
 *  `rstl::uninitialized_copy_n< const T*, T* >` for the 0x2C-byte element: copy-construct `n`
 *  elements from `src` into `dest` and return the end cursor.  Twin `fn_80248EA4`,
 *  `src/WorldFormat/CMetroidAreaCollider.cpp:922` - same instructions, element stride 0x2C
 *  instead of 0x24 and this file's `fn_8004507C` where the twin calls `fn_80248F0C`.
 *
 *  The loop is entered at its bottom test (`b .L_80045054` at 0x80045038) and the cursors advance
 *  *after* the construct, so a zero count copies nothing and still returns `dest`.  All three
 *  cursors live in non-volatile registers, which is what the 0x20 frame is for. */
void* fn_80045014(const struct SElem44* src, int n, struct SElem44* dest) {
  const struct SElem44* it = src;
  struct SElem44* cur = dest;
  int remaining = n;
  for (; remaining != 0; --remaining, ++it, ++cur) {
    fn_8004507C(cur, it);
  }
  return cur;
}
