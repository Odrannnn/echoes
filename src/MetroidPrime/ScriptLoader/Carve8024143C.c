// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the
// addresses and sizes come from `config/G2ME01/symbols.txt:10202-10203`, the instructions
// are the ones dtk itself emitted into `build/G2ME01/asm/auto_03_8023E5F4_text.s:3452-3478`
// while this range was still unclaimed, and the bodies below are the C those bytes are the
// compilation of.
//
// .text 0x8024143C..0x80241488, 0x4C = 76 bytes, 2 functions:
//
//   fn_8024143C    0x8024143C  0x3C = 60 bytes  15 instructions  deleting-destructor shape
//   fn_80241478    0x80241478  0x10 = 16 bytes   4 instructions  zero two floats at +0/+4
//
// **What the two are.**  Retail names neither, so the twins identify them, and both twins are
// exact - same instructions, same operand schedule, only the `bl`/data displacement differs:
//
//   fn_8024143C  is `__dt__5CMainFv` (0x800087DC, 0x3C = 60 bytes, `src/MetroidPrime/main.cpp:517`
//                `CMain::~CMain() {}`) word for word, `bl Free__7CMemoryFPCv` included: the
//                receiver is tested (`mr. r31,r3` / `beq`), the flag is re-tested as a **short**
//                (`extsh. r0,r4` / `ble` - a `bool` would give `extsb.`) and `if (flag > 0)` sits
//                **inside** `if (self)`, so the receiver's `beq` lands on the epilogue.  That
//                epilogue returns the receiver in r3 (`mr r3,r31`), and that is what says the
//                return is `void*` and not `void`: as `void` mwcceppc drops the move and the body
//                compiles to 14 instructions / 56 bytes, so the function is 12 bytes short of its
//                claim.  `Carve8023C860.c`, `Carve8023C950.c`, `Carve8023E5A8.c`, `Carve80241C90.c`
//                and `Carve802420F8.c` are the same shape written out the same way in this
//                directory, so the identifying is already done and needs no fresh evidence.
//   fn_80241478  is `fn_8023E5E4` (`MetroidPrime/ScriptLoader/Carve8023E5A8.c:130`, 0x10 = 16
//                bytes, the four-instruction two-float constructor) **byte for byte**, the
//                `lfs` included: `lfs f0, lbl_8041DCB0@sda21(r0)` ; `stfs f0, 0x0(r3)` ;
//                `stfs f0, 0x4(r3)` ; `blr`.  Its `lfs` is the identical encoding
//                `c0 02 b8 f0`, i.e. the same `.sdata2` object at the same address
//                `lbl_8041DCB0` (0x8041DCB0, `symbols.txt:24608`, `.sdata2`, `size:0x4`,
//                `data:float`), and the same displacement: `_SDA2_BASE_` = 0x804223C0
//                (`tools/sda.py`) minus 0x8041DCB0 is 0x4710 = 18192, and the instruction's D
//                field is `0xB8F0` = -18192.  So the `sda21` relocation has the same shape here
//                as in the twin and the same object supplies it.
//
// **Who calls them, and with what.**  All four call sites are in `fn_800FF648`
// (`symbols.txt:4530`, 0x800FF648, 0x2C0 = 704 bytes), in dtk's unclaimed `auto_03_800FF298_text`
// (and the duplicate listing `auto_03_800FF484_text` is the same code in the other unclaimed
// range), so nothing of ours needs them and no count depends on them:
//
//   0x800FF680 and 0x800FF688  `addi r3,r26,0x44` / `addi r3,r26,0x50` ; `bl fn_80241478` in
//                `fn_800FF648`'s prologue, immediately after `bl __ct__20SLdrEditorPropertiesFv`
//                on `r26 = r1+0x34` and `bl Green__6CColorFv`.  Two calls on `this+0x44` and
//                `this+0x50` in a constructor's own body is a two-float member being zeroed in
//                place, not a load from data - the same reading `Carve8023E5A8.c` records for the
//                twin's two sites.
//   0x800FF8D4 and 0x800FF8E0  `mr r3,r29` / `mr r3,r28` ; `li r4,-1` ; `bl fn_8024143C`, the
//                destructor walk at the end of the same function, immediately before
//                `addi r3,r1,0x34 ; li r4,-1 ; bl __dt__20SLdrEditorPropertiesFv` on the same
//                object the constructor opened with.  `-1` is MWCC's "destroy, do not free me
//                afterwards", so the free-through-to-self is dead at those two sites and is not
//                dead in the source - the same reading `Carve8023E5A8.c` records for its two.
//
// **Its own unit because a claim may not span an unclaimed gap, and because the neighbours are
// not writable.**  The function below is `LoadTypedefVector2f__FR12SLdrVector2fR12CInputStream`
// (`symbols.txt:10201`, 0x80241378, 0xC4 = 196 bytes, ending exactly at 0x8024143C) and the
// function above is `LoadTypedefScannableParameters__FR23SLdrScannableParametersR12CInputStream`
// (`symbols.txt:10204`, 0x80241488, 0xA8 = 168 bytes).  **Retail names both**, so each needs its
// own mangling and a `.cpp` and neither can be claimed by a `.c` anyway; neither is twin-shaped
// either.  So 0x8024143C..0x80241488 is the whole writable run and this unit claims exactly it.
// The range is inside dtk's `auto_03_8023E5F4_text`; the nearest claimed boundaries are
// `MetroidPrime/ScriptLoader/Carve8023E5A8.c` ending at 0x8023E5F4 and
// `MetroidPrime/ScriptLoader/Carve80241C90.c` starting at 0x80241C90, so nothing here is
// adjacent to a unit boundary - which is the case that creates a dtk link-order cycle
// ("Cyclic dependency ... link order", see `docs/RUNNING_THE_DECOMP.md`).
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object `.text` verbatim, so an
// ascending file is a permuted `.text` - 100.00% per function, `unit_fit.sh` still saying "fits"
// and a broken DOL.  Only `tools/flip_test.sh` catches that.  `fn_80241478` (0x80241478) is the
// *higher* address and therefore comes first here; the object then leads with `fn_8024143C` at
// offset 0, which is where retail has it.  `tools/check_decl_order.py --unit
// MetroidPrime/ScriptLoader/Carve8024143C.c` is the cheap check for the order.
//
// Retail names none of these, so `symbols.txt:10202-10203` carries the `fn_<addr>` placeholders
// and this file reproduces those symbols verbatim, which is why the definitions have to stay C:
// a C++ one would mangle to `_Z<len>fn_<addr>v` and objdiff would pair nothing.  That is also
// why the unit is a `.c` rather than a `.cpp`.
//
// The directory is retail's own, taken from the nearest claimed range: the `ScriptLoader/` carves
// own the range below this one (`Carve8023E5A8.c` ends at 0x8023E5F4, same auto unit's lower part)
// and `ScriptLoader.cpp` the range above it.  For an anonymous function that is the only evidence
// there is, and it beats a lane picking the directory it happened to own.
//
// The one callee, `Free__7CMemoryFPCv` (0x802CE388, `symbols.txt:12992`, 0x64 = 100 bytes), is
// claimed by `Kyoto/Alloc/CMemory.cpp` in the DOL and defined under this name for the host link by
// `src/Kyoto/Alloc/PortMwccNew.cpp`, so this unit needs no host stand-in for it and it adds no
// undefined symbol: this file declares the symbol and never defines it.
//
// The one **data** reference, `lbl_8041DCB0`, is retail's own `.sdata2` pool entry and needs **no
// host definition here**: `MetroidPrime/ScriptLoader/Carve8023E5A8.c` already defines it for the
// host under `#ifndef __MWERKS__`, and this file is in the same build of the same target, so the
// name resolves.  Repeating that definition would be a second definition of one symbol in the
// port's flat link, which `link_gap.py` cannot see (it counts what is *missing*) and which the
// boot probe cannot see either (it links with the reach stubs); `tools/gate.sh`'s `port link dups`
// step is what catches it.  Measured: with the declaration only, the port is at its baseline of
// 286 undefined and 0 duplicates.

/** 0x802CE388, `symbols.txt:12992`, 0x64 = 100 bytes: retail's `CMemory::Free(void const*)`.
 *  Claimed by `Kyoto/Alloc/CMemory.cpp` in the DOL and defined for the host link by
 *  `src/Kyoto/Alloc/PortMwccNew.cpp`.  Declared, never defined here. */
extern void Free__7CMemoryFPCv(const void* ptr);

/** 0x8041DCB0, `.sdata2`, 4 bytes: dtk recorded `.float 0`
 *  (`build/G2ME01/asm/auto_11_8041DAC8_sdata2.s:567-570`); `data:float` in `symbols.txt:24608`.
 *  It is the pool entry the twin `fn_8023E5E4` reads, at the same address and the same
 *  `sda21` displacement.  The matching build takes the real bytes from dtk's own
 *  `auto_11_8041DAC8_sdata2.o`; the host link takes them from
 *  `MetroidPrime/ScriptLoader/Carve8023E5A8.c`'s `#ifndef __MWERKS__` block, which already
 *  carries retail's own value.  Declared, never defined here. */
extern const float lbl_8041DCB0;

void* fn_8024143C(void* self, short flag);
void fn_80241478(float* self);

/** `fn_80241478` - retail `.text:0x80241478`, 0x10 = 16 bytes: store retail's shared zero
 *  float at +0 and at +4.  Four instructions, no frame, the register `f0` loaded once and
 *  stored twice.  Byte-for-byte the twin `fn_8023E5E4` in `Carve8023E5A8.c`, down to the
 *  `lbl_8041DCB0` address.  Called on `this + 0x44` at 0x800FF680 and on `this + 0x50` at
 *  0x800FF688, in `fn_800FF648`'s prologue, right after the object at `this + 0` is
 *  `__ct__20SLdrEditorPropertiesFv`-constructed. */
void fn_80241478(float* self) {
  self[0] = lbl_8041DCB0;
  self[1] = lbl_8041DCB0;
}

/** `fn_8024143C` - retail `.text:0x8024143C`, 0x3C = 60 bytes: a deleting-destructor-shaped
 *  step.  `self` is tested and kept for the return, the flag is a **short**, and only a positive
 *  flag reaches the free; the receiver is returned in r3 whatever the flag was.  The twin
 *  `__dt__5CMainFv` is these 15 instructions word for word, and its two call sites at
 *  0x800FF8D4 / 0x800FF8E0 pass `li r4,-1`, dead as a free and live as a destroy. */
void* fn_8024143C(void* self, short flag) {
  if (self) {
    if (flag > 0) {
      Free__7CMemoryFPCv(self);
    }
  }
  return self;
}