// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the addresses
// and sizes come from `config/G2ME01/symbols.txt:10118-10119`, the instructions are the ones dtk
// itself emitted into `build/G2ME01/asm/auto_03_802399F4_text.s:1995-2022` while this range was
// still unclaimed, and the bodies below are the C those bytes are the compilation of.
//
// .text 0x8023B634..0x8023B67C, 0x48 = 72 bytes, 2 functions:
//
//   __dt__11SLdrCommandFv   0x8023B634  0x3C = 60 bytes  15 instructions  deleting destructor
//   __ct__11SLdrCommandFv   0x8023B670  0x0C = 12 bytes   3 instructions  store the zero word
//
// **These two are named in retail, not anonymous.**  `symbols.txt:10118-10119` carries
// `__dt__11SLdrCommandFv = .text:0x8023B634; // size:0x3C` and
// `__ct__11SLdrCommandFv = .text:0x8023B670; // size:0xC`, and dtk's own comments name the class
// (`auto_03_802399F4_text.s:1996` `SLdrCommand::~SLdrCommand()`, `:2017`
// `SLdrCommand::SLdrCommand()`), both `.fn ... global`.  objdiff pairs functions by symbol name,
// so this file has to reproduce both names verbatim - and that does *not* force a `.cpp`: a
// MWCC-mangled name that is already a legal C identifier needs nothing produced, and a C++
// spelling would mangle `__dt__11SLdrCommandFv` to `_Z19__dt__11SLdrCommandFvPs` and pair nothing.
// (`RUNNING_THE_DECOMP.md:2027-2028` says a run stops at a real-named function because it needs
// its own mangling and a `.cpp`; that is too strong, and this file is the counterexample - both
// halves reached 100.00% from a `.c`.)  The file is compiled as C for the port's host build and,
// like every other source in `files.cmake`, is syntax-checked as C++ by `tools/probe_sources.sh`,
// so everything here is spelled as plain C with no constructs that change between the two.
//
// **What the pair is.**  `include/MetroidPrime/ScriptLoader/Structs/SLdrCommand.hpp:7-12` declares
// both of them and defines neither, and this file is what satisfies those two declarations.  The
// class as declared is one word - `int unknown_0x94ba5737` - and that is exactly the word the
// constructor zeroes and the only word either function touches.
//
// **Who calls them, and with what.**  Two caller families, both measured:
//
//   `asm/MetroidPrime/ScriptObjects/CScriptControllerAction.s:52`  `addi r3,r28,0x3c ; bl
//   __ct__11SLdrCommandFv` - the `SLdrCommand cmd` member of `SLdrControllerAction`
//   (`include/MetroidPrime/ScriptLoader/SLdrControllerAction.hpp:14`), whose inline constructor
//   initialises it with `cmd()`.  `:151` and `:160` are `mr r3,r29 ; li r4,-1 ; bl
//   __dt__11SLdrCommandFv` in that same destructor, `-1` being MWCC's "destroy, do not free me
//   afterwards", so the free-through-to-self is dead at those two sites and is not dead in the
//   source.
//   `asm/auto_03_8022C4C4_text.s:418-460` tears eight of them down at `r30+0x90, 0x88, ... 0x58`
//   **descending**, each `addic. r0,reg,off / beq / addi r3,r30,off / li r4,-1 / bl
//   __dt__11SLdrCommandFv`, and `:491-526` builds the same eight back up **ascending** at 0x58
//   steps.  That walk is what fixes the element stride at 8 bytes: the neighbour word at +0x04 of
//   each element is cleared by the caller (`li r0,0 / stw r0,0x4(reg)` after every constructor
//   call), so only the low word is `SLdrCommand`.
//
// **Why the two bodies are what they are.**  Both are twins of matched code in this tree, and
// both are exact:
//
//   __dt__11SLdrCommandFv  is `__dt__80004B9C` (`src/MetroidPrime/Player/Carve80004B9C.c`, 0x50 =
//   80 bytes, 100.00% in `build/report.json`) with its one `bl fn_80004BEC` member teardown
//   deleted - 20 instructions there, 15 here, identical in the four that decide the signature:
//   `mr. r31,r3 / beq` tests the receiver once, the epilogue's `mr r3,r31` *is* the return (so
//   the return type is `void*`, not `void` - as `void` mwcceppc drops the move and the body is
//   12 bytes short and does not fit the claim), `extsh. r0,r4 / ble` is a **16-bit** flag test
//   (a `bool` would give `extsb.`), and the flag never leaves r4 while the only call takes its
//   argument in r3, which is why the frame saves r31 alone.
//   __ct__11SLdrCommandFv  is `fn_8023C98C` (`src/MetroidPrime/ScriptLoader/Carve8023C950.c`, 0xC =
//   12 bytes, 100.00%), which is in turn `DisableFog__Q29CGameArea8CAreaFogFv`
//   (`mFogMode = kRFM_None`) and `fn_80004010` (`src/MetroidPrime/Carve80004010.c`): `li r0,0 /
//   stw r0,0x0(r3)` / `blr`, a store of a zero word at +0 and a return.  The type of the word is
//   not observable - a constant store needs no type - and `unknown_0x94ba5737` is the only member
//   the header declares.
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object `.text` verbatim, so an
// ascending file is a permuted `.text` - 100.00% per function, `unit_fit.sh` still saying "fits",
// and a broken DOL.  Only `tools/flip_test.sh` catches that.  `__ct__11SLdrCommandFv`
// (0x8023B670) is the *higher* address and therefore comes first here; the object then leads with
// `__dt__11SLdrCommandFv` at offset 0, which is where retail has it.  With a run of two,
// "descending" is a coin-flip decided by which address has more digits, so
// `tools/carve_diff.sh <start> <whole-range-size> <trial.o>` over the **range** - not one function -
// is the check to run straight after the trial compile; `tools/check_decl_order.py` cannot help
// there, because it needs the object already at the unit's real path.
//
// Its own unit because the claim may not span an unclaimed gap: the range is still inside dtk's
// `auto_03_802399F4_text`, whose nearest claimed boundary below is `RubiksPuzzle.cpp` ending at
// 0x802399F4 and above is `Carve8023C860.c` starting at 0x8023C860.  0x8023B634..0x8023B67C is the
// whole writable run - the function above it, `LoadTypedefCommand__FR11SLdrCommandR12CInputStream`
// at 0x8023B5C0, ends exactly at this range's start, and the one below at 0x8023B67C is
// `LoadTypedefAudioPlaybackParms`, 0x180 = 384 bytes of stream reads - so this unit claims exactly
// it, and nothing here is adjacent to a unit boundary.
//
// The one callee, `Free__7CMemoryFPCv` (0x802CE388, `symbols.txt:12992`, 0x64 = 100 bytes), is
// claimed by `Kyoto/Alloc/CMemory.cpp` in the DOL (`.text` 0x802CE224..0x802CE72C,
// `config/G2ME01/splits.txt:2538`) and defined under this name for the host link by
// `src/Kyoto/Alloc/PortMwccNew.cpp:38`, so this unit needs no `#ifndef __MWERKS__` stand-in and
// adds no undefined symbol: it declares the symbol and never defines it.

/** The one word either function touches: `SLdrCommand` as declared at
 *  `include/MetroidPrime/ScriptLoader/Structs/SLdrCommand.hpp:7-12`, of which only the single
 *  `int` at +0 is named there.  Named locally rather than included because the header drags in
 *  `Kyoto/Streams/CInputStream.hpp`, which is C++, and this unit has to be C. */
struct SLdrCommandWord {
  int unknown_0x94ba5737;
};

/** 0x802CE388, `symbols.txt:12992`, 0x64 = 100 bytes: retail's `CMemory::Free(void const*)`.
 *  Claimed by `Kyoto/Alloc/CMemory.cpp` in the DOL and defined for the host link by
 *  `src/Kyoto/Alloc/PortMwccNew.cpp:38`.  Declared, never defined here. */
extern void Free__7CMemoryFPCv(const void* ptr);

void __ct__11SLdrCommandFv(struct SLdrCommandWord* self);
void* __dt__11SLdrCommandFv(struct SLdrCommandWord* self, short flag);

/** `__ct__11SLdrCommandFv` - retail `.text:0x8023B670`, 0xC = 12 bytes: store the zero word at
 *  +0 and return.  Retail calls it on `this + 0x3c` (`CScriptControllerAction.s:52`) and on each
 *  of eight 8-byte elements at 0x58..0x90 (`auto_03_8022C4C4_text.s:491-526`), clearing the
 *  neighbour word at +0x04 itself.  The three instructions are `fn_8023C98C`'s exactly. */
void __ct__11SLdrCommandFv(struct SLdrCommandWord* self) { self->unknown_0x94ba5737 = 0; }

/** `__dt__11SLdrCommandFv` - retail `.text:0x8023B634`, 0x3C = 60 bytes: a deleting destructor
 *  with no member teardown left, because `SLdrCommand` has nothing to tear down.  `self` is
 *  tested once and kept for the return, the flag is a **short**, only a positive flag reaches the
 *  free, and the receiver is returned in r3 whatever the flag was.  Every call site passes
 *  `li r4,-1`, dead as a free and live as a destroy. */
void* __dt__11SLdrCommandFv(struct SLdrCommandWord* self, short flag) {
  if (self) {
    if (flag > 0) {
      Free__7CMemoryFPCv(self);
    }
  }
  return self;
}
