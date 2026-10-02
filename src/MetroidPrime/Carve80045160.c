// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the address and
// size come from `config/G2ME01/symbols.txt:1280`, the instructions are the ones dtk itself
// emitted into `build/G2ME01/asm/auto_03_8004509C_text.s:70-101` before this claim existed (dtk
// stops regenerating that file for a claimed range, so it is the surviving record of retail's
// bytes; dtk now emits ours to `build/G2ME01/asm/MetroidPrime/Carve80045160.s`), and the body
// below is the C those bytes are the compilation of.
//
// .text 0x80045160..0x800451CC, 0x6C = 108 bytes, 1 function:
//
//   fn_80045160  0x80045160  0x6C   27 instructions
//
// **What it is: `rstl::uninitialized_fill_n` for the 0x2C-byte element whose copy-construct is
// `fn_8004507C`** - n elements built from *one* source, which is why the source is never advanced:
//
//   80045170  mr   r31,r3          ; cur = dest
//   80045178  li   r30,0x0         ; i = 0
//   80045180  mr   r29,r5          ; value, constant across the loop
//   80045188  mr   r28,r4          ; count
//   8004518c  b    800451a4        ; the count test comes first
//   80045190  mr   r3,r31 / mr r4,r29
//   80045198  bl   fn_8004507C     ; construct(cur, value)
//   8004519c  addi r30,r30,0x1     ; ++i
//   800451a0  addi r31,r31,0x2c    ; cur += 0x2C, the element size
//   800451a4  cmpw r30,r28 / blt 80045190
//
// The loop is entered at its bottom test and the cursor advances *after* the construct, so a zero
// count builds nothing.  All four live values sit in non-volatile registers, which is what the
// 0x20 frame is for, and there is no `mr r3,...` before the epilogue: the function returns void
// and `r3` is simply whatever the last `mr r3,r31` left there.
//
// **The spelling is fixed by an already-`Matching` twin, and the twin is byte-identical
// instruction for instruction.**  `fn_80281310` in `src/MetroidPrime/Carve80281310.c`
// (0x80281310, 0x6C = 108 bytes, 27 instructions, 100% in `build/report.json`) is the same loop
// for a 0x10-byte element: same frame, same `mr r3,r31 / mr r4,r29 / bl` call, same
// `cmpw r30,r28 / blt`, same four non-volatile cursors, and the same two `addi`s in the same
// order.  Only two words differ - the stride (0x2C here against 0x10 there) and the `bl` - so
// `char* cur`, `int i` and the `for (i = 0; i < count; ++i, cur += 0x2C)` header are the twin's,
// copied rather than guessed.  Retail's own address twin of this function is `fn_80044F1C`
// (0x80044F1C, 0x6C, `build/G2ME01/asm/auto_03_80044DD4_text.s:111-142`), the same 27
// instructions with a 0x1E8 stride and a `bl fn_80044F88`; a whole family of this shape is
// already matched across the tree, so the loop form is not in question here.
//
// **The stride is what fixes the element's size, and nothing else here names it**: 0x2C = 44
// bytes, the same element `Carve80045014.c`'s `fn_80045014` copies.  It is not named in retail and
// it is not named here.
//
// **`fn_8004507C` is declared, never defined here**: it is 0x8004507C, 0x68 bytes *below* this
// claim, and `Carve80045014.c` (Matching, 0x80045014..0x8004509C) already defines it, so the
// `bl` resolves to a real definition in the DOL link and **this unit needs no
// `PortLinkStubs.cpp` entry**.  Nothing else is called, so nothing else is declared.  This is the
// caller `Carve80045014.c`'s header comment names at 0x80045198: `fn_8004507C` is called by that
// file's own `fn_80045014` and by this function.
//
// **The parameter order is fixed by this function's only caller**, `fn_80045128` at 0x80045128
// (0x38 bytes, `auto_03_8004509C_text.s:52-68`), whose tail is
// `stw r4,0x0(r3)` / `addi r3,r31,0x4` / `bl fn_80045160` / `mr r3,r31`: it stores the second
// argument into the destination's first word, then passes `self + 4` as the first argument and
// forwards `r4` and `r5` untouched.  So `r3` is the destination array, `r4` the count and `r5`
// the value - the order the body below declares, and the same order the twin's.
//
// Source order is descending by address and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object `.text` verbatim, so an
// ascending file is a permuted `.text` - 100.00% per function and a broken DOL.  Only
// `tools/flip_test.sh` catches that.
//
// Retail names this function nothing; `symbols.txt` carries the `fn_<addr>` placeholder and this
// file reproduces it verbatim, so the definition has to stay C - a C++ spelling would mangle to
// `_Z<len>fn_<addr>v` and objdiff would pair nothing.  That is also why the unit is a `.c` rather
// than a `.cpp`.  (MWCC's C mode is C89, so no declaration in a `for`-init: `i` is hoisted out
// of the loop below, which costs no instruction - the `li r30,0x0` retail has at 0x80045178 is
// the hoisted initialiser.)
//
// The directory is retail's own, taken from the nearest claimed ranges: below is
// `MetroidPrime/Carve80045014.c` and above `MetroidPrime/Carve80045CD4.c`, both in
// `MetroidPrime/`.

/** 0x8004507C, `symbols.txt:1276`, 0x20 = 32 bytes: the per-element copy-construct this loop
 *  calls, defined by `src/MetroidPrime/Carve80045014.c` (Matching, 0x80045014..0x8004509C).  That
 *  range is below this claim and is not part of it, so it is declared and never defined here.
 *  Both arguments are forwarded in place: retail never writes `r3` or `r4` between the prologue
 *  and the `bl`, so the callee takes them as pointers. */
void fn_8004507C(void* dest, const void* src);

/** `fn_80045160` - retail `.text:0x80045160`, 0x6C = 108 bytes, 27 instructions.
 *  `rstl::uninitialized_fill_n` for the 0x2C-byte element: copy-construct `count` elements from
 *  the single `value` into `dest` and return nothing.  Twin `fn_80281310`,
 *  `src/MetroidPrime/Carve80281310.c` - the same 27 instructions with a 0x10 stride instead of
 *  0x2C and this file's `fn_8004507C` where the twin calls `fn_8028137C`. */
void fn_80045160(void* dest, int count, const void* value) {
  char* cur = (char*)dest;
  int i;
  for (i = 0; i < count; ++i, cur += 0x2C) {
    fn_8004507C(cur, value);
  }
}