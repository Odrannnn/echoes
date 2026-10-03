// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the addresses
// and sizes come from `config/G2ME01/symbols.txt:7300`, the instructions are the ones dtk itself
// emitted into `build/G2ME01/asm/auto_03_801BEDD0_text.s:2217-2227` before the claim existed (the
// same range is now `build/G2ME01/asm/MetroidPrime/Enemies/Carve801C0D14.s`), and the body below
// is the C those bytes are the compilation of.
//
// .text 0x801C0D14..0x801C0D34, 0x20 = 32 bytes, 1 function:
//
//   fn_801C0D14    0x801C0D14  0x20    8 instructions   a frame and one unconditional forward
//
// **The byte shape is read out of its matched twin's own source, not guessed.**  `fn_80004438`
// (0x80004438, 0x20, `src/MetroidPrime/Carve80004438.c`, `Matching`) is these eight instructions
// word for word: `stwu r1,-0x10(r1) / mflr r0 / stw r0,0x14(r1) / bl <callee> / lwz r0,0x14(r1) /
// mtlr r0 / addi r1,r1,0x10 / blr`.  That file calls its whole body "a frame and one
// unconditional `bl` ... so it is the `destroy` of the pair, whose whole body is
// `destroy_impl(in)`", and it reaches that from the argument rather than from a load, a test or a
// return value - none of the eight instructions here does either.  Compared with that twin's two
// `.fn` blocks the difference is **one word, the `bl`**; nothing else differs, and the 0x20 length
// is the same.  The twin writes it as `fn_80004438(void* self) { fn_80004458(self); }`, so this
// copy is written as the same forward - **which is what its own bytes say, not a guess about what
// template retail instantiated.**  Whether retail compiled it from an `rstl::` template or from a
// hand-written pair is not decidable from eight instructions and is not claimed here.
//
// **Both of this function's parameters reach `fn_801C0CB0` untouched.**  The `bl` is preceded by no
// `mr` and followed by no `li`, so r3 and r4 are the caller's r3 and r4 on entry to the callee.
// The two call sites in retail (measured with `grep -rn 'bl fn_801C0D14' build/G2ME01/asm/`) are
// both in `fn_80199604` (`build/G2ME01/asm/auto_03_80197978_text.s:2118-2121`) and both do
// `mr r4,r31 / addi r3,r31,0x6c4 / bl fn_801C0D14`, so two arguments are set before every call and
// the second one is live.  That is why this body forwards **two** parameters where
// `src/MetroidPrime/Carve80004438.c:97` forwards one: the frame is the same either way, because
// mwcceppc does not have to move r3 or r4 to call, and the extra parameter costs no instruction.
//
// `fn_801C0CB0` (0x801C0CB0, 0x64 = 100 bytes, `symbols.txt:7299`) is **below** this claim and is
// therefore declared, never defined here.  Its own bytes are a 0x10 frame, a `lwz r0,0x0(r3)` /
// `cmpwi r0,-1` / `beq` early return, then - on the other side of that test - a `lwz r3,0x24(r4)`
// that reads a word off the second parameter and three dependent loads off it, one `bl
// DelAdditiveAnimation__9CAnimDataFUi`, and a run of stores writing -1 into its receiver at +0x00
// and +0x04 and clearing eight bits at +0x08 and +0x0D.  That is 100 bytes of a spelling job of
// its own, which is why the claim stops where it does, and its `bl` at 0x801C0CE0 is retail's, so
// the carve cannot drop the call.  For the DOL, dtk's own `auto_03_801BEDD0_text.o` defines it; for
// the port link **this file's host branch is empty by design** (the body is inside
// `#ifdef __MWERKS__`), the trade `src/MetroidPrime/Carve801C2D74.cpp` makes: adding a host body
// here would add one undefined symbol to the port's link for a function no host source calls -
// both retail callers of `fn_801C0D14` are themselves inside the unclaimed dtk range above.
// **Nothing here claims `fn_801C0CB0` is decompiled.**
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object `.text` verbatim, so an
// ascending file is a permuted `.text` - 100.00% per function and a broken DOL.  Only
// `tools/flip_test.sh` catches that.  With one function in the claim the order is trivially right,
// but the rule is stated so the next carve into this range keeps it.
//
// Retail names none of these.  `symbols.txt` carries the `fn_801C0D14` placeholder and this file
// reproduces that symbol verbatim, so the definition has to stay C: a C++ one would mangle to
// `_Z<len>fn_801C0D14v` and objdiff would pair nothing.  That is also why the unit is a `.c`
// rather than a `.cpp`.
//
// Its own unit because a claim may not span an unclaimed gap: below it, 0x801C0CB0..0x801C0D14
// (`fn_801C0CB0`) is unclaimed, and above it 0x801C0D34..0x801C0E80 (`fn_801C0D34`, 0x14C) is too.
// The directory is retail's own, taken from the nearest claimed ranges: below is
// `MetroidPrime/Enemies/CKnockBackMgr.cpp` (0x801BD714..0x801BEDD0), above is
// `MetroidPrime/Carve801C128C.c` (0x801C128C..0x801C1290).

#ifdef __MWERKS__

/** `fn_801C0CB0` - retail `.text:0x801C0CB0`, 0x64 = 100 bytes, `symbols.txt:7299`: the function
 *  this file's `fn_801C0D14` forwards to, taking the receiver in r3 and a second pointer in r4 -
 *  it reads `r4 + 0x24` and dereferences the result twice before its one call.  Declared, never
 *  defined here; the DOL gets it from dtk's own `auto_03_801BEDD0_text.o` and the port's host
 *  branch of this file is empty, so neither link has to be given one.  Both parameters are
 *  pointers: the function writes words into `((char*)self)[0x00]` and `[0x04]` and bytes at `+0x08`
 *  and `+0x0D`. */
extern void fn_801C0CB0(void* self, void* arg2);

/** `fn_801C0D14` - retail `.text:0x801C0D14`, 0x20 = 32 bytes: a 0x10 frame and one unconditional
 *  `bl fn_801C0CB0`, and nothing else - no load, no test, no return value.  Both parameters are
 *  forwarded in the registers they arrived in, which is what the absence of any `mr` or `li` around
 *  the `bl` says. */
void fn_801C0D14(void* self, void* arg2);

void fn_801C0D14(void* self, void* arg2) { fn_801C0CB0(self, arg2); }

#endif // __MWERKS__
