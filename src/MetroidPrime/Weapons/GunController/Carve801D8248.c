// Carved out of an unclaimed dtk `auto_*` range by lane `carve` (goal item `carve-801d8248`).
// Every number here is measured: the address and size come from `config/G2ME01/symbols.txt:7616`
// (`fn_801D8248 = .text:0x801D8248; // type:function size:0xC align:4`), the instructions are the
// ones dtk itself emitted into `build/G2ME01/asm/auto_03_801D72D0_text.s:1106-1111` while this
// range was unclaimed, and the body below is the C those bytes are the compilation of.
//
// .text 0x801D8248..0x801D8254, 0xC = 12 bytes, 1 function:
//
//   fn_801D8248    0x801D8248  0xC = 12 bytes
//       0x801D8248  38 00 00 00   li      r0, 0x0
//       0x801D824C  98 03 00 08   stb     r0, 0x8(r3)
//       0x801D8250  4E 80 00 20   blr
//
// **What it is: the array element constructor `fn_801D81C8` hands to `__construct_array`.** That
// function (`build/G2ME01/asm/auto_03_801D72D0_text.s:1070-1098`, 0x80 bytes) sets the array up
// with
//
//     r3 = this + 0x274      (the array)
//     r4 = fn_801D8248       (second argument)
//     r5 = fn_801D8160       (third argument)
//     r6 = 0xc               (element size, 12)
//     r7 = 3                 (element count)
//     r0 = lbl_803B7310      (sixth argument, stored at this + 0x0 just before the call)
//     bl __construct_array
//
// and the same unit's destructor path calls `__destroy_arr` on the same array with the *same*
// third argument as its destructor (`auto_03_801D72D0_text.s:1014-1020`: `addi r3, r30, 0x274` /
// `addi r4, r4, fn_801D8160@l` / `li r5, 0xc` / `li r6, 0x3` / `bl __destroy_arr`).  So
// `fn_801D8160` (0x801D8160, 0x68 bytes, `symbols.txt:7614`) is the element destructor and
// `fn_801D8248` is the default constructor that goes with it - which is why all three bytes of
// its body are the flag clear that destructor tests for.  `fn_801D8160` reads exactly that byte
// (`lbz r0, 0x8(r30)`, then `cmplwi r0, 0x0` / `beq`) before deciding whether to call
// `__dt__6CTokenFv` and `Free__7CMemoryFPCv`, so the store below is the one that decides whether
// the destructor has any base-class work to do.  Retail names the element type nowhere, so the
// struct below is named for the parameter only and models the three offsets that are measured:
// the eight bytes of `CToken` (`include/Kyoto/CToken.hpp`: `CObjectReference* mObjRef`, `bool
// mLockHeld`, padded to 8) and the one flag byte at +8.
//
// **The name is retail's, so the unit is `.c`.**  `symbols.txt:7616` declares
// `fn_801D8248 = .text:0x801D8248; // type:function size:0xC align:4` - retail names no such
// function and the `fn_<addr>` placeholder is all there is.  A `.cpp` unit would mangle it to
// `_Z<len>fn_801D8248Pv` and objdiff would pair nothing; `.c` is compiled with `-lang=c`, so the
// definition below *is* the symbol.  No other unit in `src/` defines it -
// `grep -rn fn_801D8248 src/ include/` finds only `config/G2ME01/symbols.txt` - so there is
// exactly one definition and no `PortLinkStubs.cpp` duplicate to delete.
//
// **Byte-shape twin: `VSimplified__28CAnimTreeAnimReaderContainerFv`** at 0x802A5EE8, 0xC bytes,
// in the `Matching` unit `src/Kyoto/Animation/CAnimTreeAnimReaderContainer.cpp:133-136` (whose
// body is `return rstl::optional_object_null();`).  Its retail disassembly
// (`build/G2ME01/asm/Kyoto/Animation/CAnimTreeAnimReaderContainer.s:132-136`) is the same three
// instructions with the same encodings - `38 00 00 00 / 98 03 00 08 / 4E 80 00 20` - which is why
// the constant zero has to be materialised into `r0` rather than stored from anywhere else.  The
// two functions compute unrelated things; only the emitted bytes are shared.
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object's `.text` order verbatim,
// so an ascending file is a permuted `.text` - 100.00% per function and a broken DOL.  Only
// `tools/flip_test.sh` catches that.  A one-function file cannot get it wrong.
//
// Its own unit because a claim may not span an unclaimed gap: `MetroidPrime/Weapons/
// GunController/CGunMotion.cpp` claims `.text` up to 0x801D72D0 and `MetroidPrime/Weapons/
// CGunWeapon.cpp` claims from 0x801D8254, and this 12-byte function sits in the dtk
// `auto_03_801D72D0_text` gap between them.  What is left of that gap, 0x801D72D0..0x801D8248, is
// deliberately still unclaimed: it is `fn_801D8160`, `fn_801D8090` and the rest of the
// `CGunWeapon`-adjacent block, none of which is a copy of anything already matched.
//
// The port compiles this file too (`files.cmake`) and it adds no undefined symbol: it calls
// nothing and reads no global.

/** The element `fn_801D81C8` constructs three of at `this + 0x274`, named for the parameter only.
 *  `+0..+8` is the `CToken` base (`include/Kyoto/CToken.hpp`), `+8` is the one flag byte the
 *  destructor `fn_801D8160` tests.  Nothing here reads the base members, so only the offsets that
 *  the instructions actually name are modelled. */
struct SElem801D8248 {
  void* mObjRef;       /* +0 */
  int mLockHeld;       /* +4 */
  unsigned char mFlag; /* +8 */
};

/** 0x801D8248, `symbols.txt:7616`, size 0xC: the default constructor `__construct_array` runs on
 *  each of the three 12-byte elements.  Clears the flag at +8 and nothing else - which is what
 *  makes `fn_801D8160`'s `lbz r0, 0x8(r30)` test fail, so a default-constructed element has no
 *  `CToken` to destroy and no block to free. */
void fn_801D8248(struct SElem801D8248* self) { self->mFlag = 0; }
