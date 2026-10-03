// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the
// addresses and sizes come from `config/G2ME01/symbols.txt:10232-10233`, the instructions are
// the ones dtk itself emitted into
// `build/G2ME01/asm/auto_03_8023C998_text.s:6566-6591`, and the bodies below are the C those
// bytes are the compilation of.
//
// .text 0x802420F8..0x80242140, 0x48 = 72 bytes, 2 functions:
//
//   fn_802420F8    0x802420F8  0x3C = 60 bytes  15 instructions  deleting-destructor shape
//   fn_80242134    0x80242134  0x0C = 12 bytes   3 instructions  clear the word at +0
//
// **What the two are.**  Retail names neither, so the twins identify them, and both twins are
// exact - same instructions, same operand schedule, only the `bl` displacement differs:
//
//   fn_802420F8  is `__dt__5CMainFv` (0x800087DC, 0x3C = 60 bytes,
//                `src/MetroidPrime/main.cpp:517` `CMain::~CMain() {}`) word for word,
//                `bl Free__7CMemoryFPCv` included: the receiver is tested (`mr. r31,r3` /
//                `beq`), the flag is re-tested as a **short** (`extsh. r0,r4` / `ble` - a `bool`
//                would give `extsb.`) and `if (flag > 0)` sits **inside** `if (self)`, so the
//                receiver's `beq` lands on the epilogue.  That epilogue returns the receiver in
//                r3 (`mr r3,r31`), and that is what says the return is `void*` and not `void`:
//                as `void` mwcceppc drops the move and the body compiles to 14 instructions /
//                56 bytes, so the function would be 12 bytes short of its claim.
//   fn_80242134  is `DisableFog__Q29CGameArea8CAreaFogFv` (0x80056DE8, 0xC = 12 bytes, spelled
//                `mFogMode = kRFM_None;` at `src/MetroidPrime/CGameArea.cpp:1592`, the member
//                `ERglFogMode mFogMode` first at
//                `include/MetroidPrime/CGameArea.hpp:120`) exactly: `li r0,0` /
//                `stw r0,0(r3)` / `blr` - store a zero word at +0 and return.  The type of the
//                word is not observable: a constant store needs no type, and the twin is an
//                enum.
//
// **Who calls them, and with what.**  Both call sites are in dtk's unclaimed
// `auto_03_800B743C_text` (and the duplicate listing `auto_03_800B7928_text` is the same code in
// the other unclaimed range), so nothing of ours needs them and no count depends on them:
//
//   0x800B7F14 and 0x800B7F1C  `mr r3,r17 ; li r4,-1 ; bl fn_802420F8`, in the destructor walk
//                `fn_800B7928` runs over its stack temporaries; `r17` is `r1+0xc4`, set at
//                0x800B7954 alongside the other temporaries the walk tears down (`r18` = `r1+0x134`
//                ... `r29` = `r1+0xcc`).  `-1` is MWCC's "destroy, do not free me afterwards",
//                so the free-through-to-self is dead at those two sites and is not dead in the
//                source.  This is the same walk that reaches the twin of the carve below it,
//                `fn_8023C950`, at 0x800B7E8C and 0x800B7EA4.
//   0x800B7F5C  `addi r3,r31,0x44 ; bl fn_80242134` in `fn_800B7F44` - itself entered from
//                `fn_800B7928` as `addi r3,r1,0x80 ; bl fn_800B7F44` at 0x800B7944, so `this+0x44`
//                is the first member of that temporary.  The three words after it
//                (`this+0x4c`, `+0x54`, `+0x5c`) go to `fn_8024203C` instead, so this is the odd
//                one out of a four-word run of initialisers.
//
// **Its own unit because a claim may not span an unclaimed gap, and because the neighbours are
// not writable.**  The range below is still dtk's `auto_03_8023C998_text`: `fn_80242050`
// (0x80242050, 0xA8 = 168 bytes) ends exactly where this one starts and is not twin-shaped.  The
// range above starts at `fn_80242140` (0x80242140, 0xD8 = 216 bytes) and is also not twin-shaped.
// 0x802420F8..0x80242140 is the whole writable run and this unit claims exactly it.  No claim
// spans the gap, and the nearest claimed boundaries are
// `MetroidPrime/ScriptLoader/Carve8023C950.c` ending at 0x8023C998 and
// `MetroidPrime/ScriptLoader.cpp` starting at 0x80242894, so nothing here is adjacent to a unit
// boundary - which is what creates a dtk link-order cycle.
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object `.text` verbatim, so an
// ascending file is a permuted `.text` - 100.00% per function, `unit_fit.sh` still saying
// "fits" and a broken DOL.  Only `tools/flip_test.sh` catches that.  `fn_80242134`
// (0x80242134) is the *higher* address and therefore comes first here; the object then leads with
// `fn_802420F8` at offset 0, which is where retail has it.
//
// Retail names none of these, so `symbols.txt:10232-10233` carries the `fn_<addr>` placeholder
// and this file reproduces that symbol verbatim, which is why the definitions have to stay C: a
// C++ one would mangle to `_Z<len>fn_<addr>v` and objdiff would pair nothing.  That is also why
// the unit is a `.c` rather than a `.cpp`.
//
// The directory is retail's own, taken from the nearest claimed range: the `ScriptLoader/` units
// own the range below this one (`Carve8023C950.c`, 0x8023C950..0x8023C998) and `ScriptLoader.cpp`
// the range above it.  For an anonymous function that is the only evidence there is, and it beats
// a lane picking the directory it happened to own.
//
// The one callee, `Free__7CMemoryFPCv` (0x802CE388, `symbols.txt:12992`), is claimed by
// `Kyoto/Alloc/CMemory.cpp` (`.text` 0x802CE224..0x802CE72C) in the DOL and defined under this
// name for the host link by `src/Kyoto/Alloc/PortMwccNew.cpp:38`, so this unit needs no
// `#ifndef __MWERKS__` stand-in and adds no undefined symbol: it declares the symbol and never
// defines it.

/** 0x802CE388, `symbols.txt:12992`, 0x64 = 100 bytes: retail's `CMemory::Free(void const*)`.
 *  Claimed by `Kyoto/Alloc/CMemory.cpp` in the DOL and defined for the host link by
 *  `src/Kyoto/Alloc/PortMwccNew.cpp:38`.  Declared, never defined here. */
extern void Free__7CMemoryFPCv(const void* ptr);

void* fn_802420F8(void* self, short flag);
void fn_80242134(int* self);

/** `fn_80242134` - retail `.text:0x80242134`, 0xC = 12 bytes: store the zero word at +0.  The
 *  twin is `DisableFog__Q29CGameArea8CAreaFogFv`, three instructions, and this is the same three.
 *  Called on `this + 0x44` at 0x800B7F5C, the first of a four-word run whose other three
 *  (`+0x4c`, `+0x54`, `+0x5c`) go to `fn_8024203C`. */
void fn_80242134(int* self) { *self = 0; }

/** `fn_802420F8` - retail `.text:0x802420F8`, 0x3C = 60 bytes: a deleting-destructor-shaped
 *  step.  `self` is tested and kept for the return, the flag is a **short**, and only a positive
 *  flag reaches the free; the receiver is returned in r3 whatever the flag was.  The twin
 *  `__dt__5CMainFv` is these 15 instructions word for word, and its two call sites at
 *  0x800B7F14 / 0x800B7F1C pass `li r4,-1`, dead as a free and live as a destroy. */
void* fn_802420F8(void* self, short flag) {
  if (self) {
    if (flag > 0) {
      Free__7CMemoryFPCv(self);
    }
  }
  return self;
}