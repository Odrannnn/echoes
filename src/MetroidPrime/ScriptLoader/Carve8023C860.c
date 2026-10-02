// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the
// addresses and sizes come from `config/G2ME01/symbols.txt:10136-10137`, the instructions
// are the ones dtk itself emitted into
// `build/G2ME01/asm/auto_03_802399F4_text.s:3362-3387` while this range was still unclaimed,
// and the bodies below are the C those bytes are the compilation of.
//
// .text 0x8023C860..0x8023C8A8, 0x48 = 72 bytes, 2 functions:
//
//   fn_8023C860    0x8023C860  0x3C = 60 bytes  15 instructions  deleting-destructor shape
//   fn_8023C89C    0x8023C89C  0x0C = 12 bytes   3 instructions  set the word at +0 to 1
//
// **What the two are.**  Retail names neither, so the twins identify them, and this range is
// the *second* copy of the pair the neighbouring carve `Carve8023C950` (commit 10c3f89f,
// 0x8023C950..0x8023C998, same auto unit) landed - the instructions are the same ones, so the
// identifying is already done and this file only has to reproduce them:
//
//   fn_8023C860  is `fn_8023C950` **word for word**, its `bl Free__7CMemoryFPCv` included: the
//                receiver is tested (`mr. r31,r3` / `beq`), the flag is re-tested as a
//                **short** (`extsh. r0,r4` / `ble` - a `bool` would give `extsb.`) and
//                `if (flag > 0)` sits **inside** `if (self)`, so the receiver's `beq` lands on
//                the epilogue.  That epilogue returns the receiver in r3 (`mr r3,r31`), and
//                that is what says the return is `void*` and not `void`: as `void` mwcceppc
//                drops the move and the body compiles to 14 instructions / 56 bytes, so the
//                function is 12 bytes short and does not fit the claim.  `fn_8023C950` is in
//                turn `__dt__5CMainFv` (0x800087DC, 0x3C = 60 bytes, `src/MetroidPrime/main.cpp`
//                `CMain::~CMain() {}`) word for word.
//   fn_8023C89C  is `fn_8023C98C` with the other constant: `li r0,1` / `stw r0,0(r3)` / `blr`
//                against its sibling's `li r0,0`.  `fn_8023C98C` is
//                `DisableFog__Q29CGameArea8CAreaFogFv` (0x80056DE8, 0xC = 12 bytes, spelled
//                `mFogMode = kRFM_None;` at `src/MetroidPrime/CGameArea.cpp`), so the shape is
//                a single word store and the type of that word is not observable: a constant
//                store needs no type (`int` here).
//
// **Who calls them, and with what.**  Both call sites are in dtk's unclaimed
// `auto_03_800B743C_text`/`auto_03_800B7928_text` (the same code listed twice, in two unclaimed
// ranges), so nothing of ours needs them and no count depends on them:
//
//   0x800B7E98  `mr r3,<member> ; li r4,-1 ; bl fn_8023C860`, in the same destructor walk as
//                `fn_8023C950`'s two sites (0x800B7E8C / 0x800B7EA4) - one object's members are
//                destroyed three at a time with MWCC's "destroy, do not free me afterwards"
//                flag `-1`, so the free-through-to-self is dead at that site and is not dead in
//                the source.
//   0x800B7FD4  `addi r3,r31,0xb8 ; bl fn_8023C89C`, between the two `fn_8023C98C` stores on
//                `this + 0xb4` (0x800B7FCC) and `this + 0xbc` (0x800B7FDC); the three words are
//                one clear/enable triple - 0 at +0xb4, **1** at +0xb8, 0 at +0xbc.
//
// **Its own unit because a claim may not span an unclaimed gap.**  The neighbours are not
// writable: below is `fn_8023C7B8` (0x8023C7B8, 0xA8 = 168 bytes) and above is `fn_8023C8A8`
// (0x8023C8A8, 0xA8 = 168 bytes, a byte reader with `lhz` / `addi r0,r4,2` stream advances) -
// neither is twin-shaped, so 0x8023C860..0x8023C8A8 is the whole writable run.  The whole gap
// this range sits in is dtk's `auto_03_802399F4_text`, 0x802399F4..0x80242894; no claim spans
// it, and this one does not either.  The nearest claimed boundaries are `RubiksPuzzle.cpp`
// ending at 0x802399F4 and `ScriptLoader.cpp` starting at 0x80242894, so nothing here is
// adjacent to a unit boundary (which is what creates a dtk link-order cycle); the neighbouring
// carve `Carve8023C950.c` starts 0xA8 above this claim's end and links.
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object `.text` verbatim, so an
// ascending file is a permuted `.text` - 100.00% per function, `unit_fit.sh` still saying
// "fits" and a broken DOL.  Only `tools/flip_test.sh` catches that.  `fn_8023C89C` (0x8023C89C)
// is the *higher* address and therefore comes first here; the object then leads with
// `fn_8023C860` at offset 0, which is where retail has it.
//
// Retail names none of these, so `symbols.txt:10136-10137` carries the `fn_<addr>` placeholder
// and this file reproduces that symbol verbatim, which is why the definitions have to stay C:
// a C++ one would mangle to `_Z<len>fn_<addr>v` and objdiff would pair nothing.  That is also
// why the unit is a `.c` rather than a `.cpp`.
//
// The directory is retail's own, taken from the nearest claimed range: the `ScriptLoader/`
// units own the range below this one and `ScriptLoader.cpp` the range above it.  For an
// anonymous function that is the only evidence there is, and it beats a lane picking the
// directory it happened to own.
//
// The one callee, `Free__7CMemoryFPCv` (0x802CE388, `symbols.txt:12992`), is claimed by
// `Kyoto/Alloc/CMemory.cpp` in the DOL and defined under this name for the host link by
// `src/Kyoto/Alloc/PortMwccNew.cpp`, so this unit needs no `#ifndef __MWERKS__` stand-in and
// adds no undefined symbol: it declares the symbol and never defines it.

/** 0x802CE388, `symbols.txt:12992`, 0x64 = 100 bytes: retail's `CMemory::Free(void const*)`.
 *  Claimed by `Kyoto/Alloc/CMemory.cpp` in the DOL and defined for the host link by
 *  `src/Kyoto/Alloc/PortMwccNew.cpp`.  Declared, never defined here. */
extern void Free__7CMemoryFPCv(const void* ptr);

void* fn_8023C860(void* self, short flag);
void fn_8023C89C(int* self);

/** `fn_8023C89C` - retail `.text:0x8023C89C`, 0xC = 12 bytes: store 1 in the word at +0.  The
 *  shape is its sibling `fn_8023C98C`'s (three instructions, a constant store and a return)
 *  with `li r0,1` instead of `li r0,0`, and the twin of *that* is
 *  `DisableFog__Q29CGameArea8CAreaFogFv`.  Called on `this + 0xb8` at 0x800B7FD4, between the
 *  two `fn_8023C98C` clears on `this + 0xb4` and `this + 0xbc`. */
void fn_8023C89C(int* self) { *self = 1; }

/** `fn_8023C860` - retail `.text:0x8023C860`, 0x3C = 60 bytes: a deleting-destructor-shaped
 *  step, byte for byte the sibling carve `fn_8023C950` (commit 10c3f89f).  `self` is tested and
 *  kept for the return, the flag is a **short**, and only a positive flag reaches the free; the
 *  receiver is returned in r3 whatever the flag was.  Called at 0x800B7E98 with `li r4,-1`, in
 *  the same teardown walk as `fn_8023C950`'s two sites. */
void* fn_8023C860(void* self, short flag) {
  if (self) {
    if (flag > 0) {
      Free__7CMemoryFPCv(self);
    }
  }
  return self;
}
