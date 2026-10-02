// CGrenchler1F18.cpp - Grenchler's (module 27) .text 0x1E74..0x1FAC: six of CGrenchler's methods,
// four of them virtual predicates and two the deleting destructors of the module's two smallest
// classes.
//
//   0x1E74 fn_27_1E74  0x5C  deleting destructor: own vtable `.data:0xEA0`, then the base's
//                            `.data:0xEAC` through the second `if (self)`, then `Free`
//   0x1ED0 fn_27_1ED0  0x1C  the base's own deleting destructor: `.data:0xEAC`, then `Free`
//   0x1F18 fn_27_1F18  0x2C  `state word +0xA78 == 8 && second argument's +0x14FC owner's
//                            +0x38C word == 1`
//   0x1F44 fn_27_1F44  0x20  the module's `CPatterned::Stuck` override, one call, nothing else
//   0x1F64 fn_27_1F64  0x1C  `+0xD68 > lbl_27_rodata_168`
//   0x1F80 fn_27_1F80  0x2C  `bit 2 of +0xD50 != 0 && +0xE5C > +0xE64`
//
// The six are contiguous - `config/G2ME01/rels/Grenchler/symbols.txt` gives 0x1E74 size 0x5C,
// 0x1ED0 size 0x1C, 0x1F18 size 0x2C, 0x1F44 size 0x20, 0x1F64 size 0x1C and 0x1F80 size 0x2C, so
// each one starts where the one below it ends and 0x1F80 + 0x2C = 0x1FAC is `fn_27_1FAC` - so the
// claim covers them and nothing else. Everything around them, including the rest of dtk's
// `auto_00_00000168_text`, stays unclaimed and dtk fills it from retail, which is what keeps the
// module's sha1 at `config/G2ME01/config.yml`'s `de128c91f467b5b61b4de4a2ed6b217044a975c5`.
//
// **Three of the six are `bool` returns whose bytes are comparison idioms, and that is what fixes
// their spellings.** `fn_27_1F64` and `fn_27_1F80` end in `fcmpo` + `mfcr` + `rlwinm r3,r0,2,31,31`
// (the GT bit) and `fn_27_1F18` ends in `subfic`/`cntlzw`/`srwi` - MWCC's `== 1` - so each is a
// `return` of a comparison rather than an `if` with two `li`s. The GT bit is why both float
// comparisons are spelled `>`: `<` emits `srwi r3,r0,31` instead, measured in a probe compiled
// with this unit's own flags.
//
// **`fn_27_1F80`'s bit test is spelled `== 0` and not `!`, and that is a block-layout
// measurement.** The two have the same value but MWCC lays the blocks out differently: `!mBit2`
// puts the `return false` at the end behind a `beq`, `mBit2 == 0` puts it directly after the test
// behind a `bne` that jumps over it - retail's layout. Probed under both GC/1.3.2 and GC/2.7; the
// two compilers agree on it.
//
// **`fn_27_1F44` is a forwarding override and its frame is retail's.** MWCC does not turn
// `return CPatterned::Stuck(...)` into a tail call here: the bytes are `stwu r1,-0x10` /
// `mflr r0` / `stw r0,0x14(r1)` / `bl Stuck__10CPatternedCFR13CStateManagerRC12CTriggerData` /
// epilogue, with the three incoming registers untouched - so the callee is declared under its own
// mangled name (`symbols.txt:5625`, 0x80151978) and called with (self, arg, arg) in r3/r4/r5.
//
// **The two destructors store the module's own vtables and are the only functions here that are
// not vtable slots.** `.data:0xEA0` and `.data:0xEAC` are twelve bytes each - two zero words and a
// function pointer - and each points at the destructor that stores it, so both are kept by the
// link without a `force_active:` entry. The second `if (self)` in `fn_27_1E74` is retail's `beq`
// on the CR0 left by `mr. r31,r3`, the address test MWCC emits for an inlined base destructor.
//
// **The class is modelled with padding, not raw offsets.** Every member is named, so
// `tools/check_raw_offsets.py` counts no raw offset in this file. `lbl_27_rodata_168` is the
// module's own `.rodata` constant and is *referenced*, not spelled as a literal: a literal would
// make mwcceppc emit a private `.rodata` constant into this object and grow the module's
// `.rodata`, which is the trap `CGrenchlerAC5C.cpp` measured on the same run.
//
// The four predicates are in the module's `ldscript.lcf` FORCEACTIVE block (they are vtable slots
// with no call site in the module's bytes), so nothing here needs a `force_active:` entry -
// measured: the module links to retail's exact 121468 bytes.
//
// The bodies are inside `#ifdef __MWERKS__` and the host branch is empty, the arrangement
// `CLumiteRelTail.cpp` and `CPlantScarabSwarmTail.cpp` use: `check_files_cmake.py` requires every
// configure.py `Matching` object to be in `files.cmake`, and only a RELMain/RELExit unit is
// exempt - but a host body would make the port link the DOL's `Stuck` spelling through a
// module-local call.
//
// Definitions are in descending retail text order: mwcceppc emits definitions in reverse source
// order and mwldeppc keeps the object's `.text` order verbatim, so ascending would permute the
// module's bytes with objdiff still at 100% and only the module's sha1 would catch it. This file
// was written ascending first and the module's `.text` came out 0x1F18-first with all six at
// 100% - the sha1 is the only thing that saw it.

#include "types.h"

/** The one-bit fields MWCC packs into a byte, most significant first: `mBit0` is 0x80 of the
 *  byte. `fn_27_1F80` reads the third of these. */
struct SGrenchlerBits {
  u8 mBit0 : 1;
  u8 mBit1 : 1;
  u8 mBit2 : 1;
  u8 mBit3 : 1;
  u8 mBit4 : 1;
  u8 mBit5 : 1;
  u8 mBit6 : 1;
  u8 mBit7 : 1;
};

/** 0x1F80's range: the byte at +0xD50 and the two floats it compares. The class is CGrenchler,
 *  whose full layout this tree does not model, so the gaps are opaque padding of the measured
 *  size rather than invented members. */
struct SGrenchlerF80 {
  uchar mUnknown00[0xD50];
  SGrenchlerBits mFlagsD50; // +0xD50
  uchar mUnknownD51[0xE5C - 0xD51];
  float mFloatE5C; // +0xE5C
  uchar mUnknownE60[0xE64 - 0xE60];
  float mFloatE64; // +0xE64
};

/** 0x1F64's range: the float at +0xD68. */
struct SGrenchlerF64 {
  uchar mUnknown00[0xD68];
  float mFloatD68; // +0xD68
};

/** 0x1F18's range: the state word at +0xA78. */
struct SGrenchlerF18 {
  uchar mUnknown00[0xA78];
  int mState0A78; // +0xA78
};

/** 0x1F18's second argument: a pointer at +0x14FC whose owner has the word at +0x38C. */
struct SOwnerF18 {
  uchar mUnknown00[0x38C];
  int mWord38C; // +0x38C
};

struct SArgF18 {
  uchar mUnknown00[0x14FC];
  const SOwnerF18* mOwner14FC; // +0x14FC
};

/** The module's own `.rodata:0x168`, a float, stored by dtk's `auto_03_00000000_rodata`. */
extern "C" const float lbl_27_rodata_168;

/** The module's own `.data:0xEA0` and `.data:0xEAC`, twelve bytes each and the same shape: two
 *  zero words then a function pointer - the two single-slot vtables `fn_27_1E74` and `fn_27_1ED0`
 *  store into the objects they destroy (`build/G2ME01/Grenchler/asm/auto_04_00000000_data.s`).
 *  Referenced, never spelled: a literal would need the same address anyway. */
extern "C" void* lbl_27_data_EA0;
extern "C" void* lbl_27_data_EAC;

/** 0x802CE388, `symbols.txt:12992`, size 0x64: `CMemory::Free(void const*)`, claimed by
 *  `Kyoto/Alloc/CMemory.cpp` in both builds - declared under retail's own emitted spelling so the
 *  two deleting destructors below need no header. */
extern "C" void Free__7CMemoryFPCv(const void* ptr);

/** 0x80151978, `symbols.txt:5625`, size 0x18: `CPatterned::Stuck`'s own implementation, reached
 *  through the mangled name the module's own `bl` uses. */
extern "C" bool Stuck__10CPatternedCFR13CStateManagerRC12CTriggerData(
    const void* self, const void* mgr, const void* trigger);

#ifdef __MWERKS__

// 0x1F80, 0x2C bytes. **The bit test is spelled `== 0`, not `!`, and that is measured**: the two
// compile to the same *values* but MWCC lays the blocks out differently - `!mBit2` puts the
// `return false` at the end behind a `beq`, while `== 0` puts it directly after the test behind a
// `bne` over it, which is retail's layout (probed under both GC/1.3.2 and GC/2.7; the two agree).
// The comparison is `>` for the same reason: `<` emits `srwi r3,r0,31` where retail's word is
// `rlwinm r3,r0,2,31,31`, the GT bit rather than the LT bit.
extern "C" bool fn_27_1F80(const SGrenchlerF80* self) {
  if (self->mFlagsD50.mBit2 == 0) {
    return false;
  }
  return self->mFloatE5C > self->mFloatE64;
}

// 0x1F64, 0x1C bytes. Retail loads the member into f1 first and the module's constant into f0
// second, and its last word is `rlwinm r3,r0,2,31,31` - the GT bit - so the comparison is `>` and
// not `<` (which is `srwi r3,r0,31`, measured in a probe compiled with this unit's own flags).
extern "C" bool fn_27_1F64(const SGrenchlerF64* self) {
  return self->mFloatD68 > lbl_27_rodata_168;
}

// 0x1F44, 0x20 bytes. One call and a frame; see the header for why the frame is retail's.
extern "C" bool fn_27_1F44(const void* self, const void* mgr, const void* trigger) {
  return Stuck__10CPatternedCFR13CStateManagerRC12CTriggerData(self, mgr, trigger);
}

// 0x1F18, 0x2C bytes. The word at +0x38C is compared with 1 - `subfic`/`cntlzw`/`srwi` is MWCC's
// `== 1` on a word - and the state test comes first.
extern "C" bool fn_27_1F18(const SGrenchlerF18* self, const SArgF18* arg) {
  if (self->mState0A78 != 8) {
    return false;
  }
  return arg->mOwner14FC->mWord38C == 1;
}

// 0x1ED0, 0x1C bytes. The base's own deleting destructor, one vtable store and the same free.
// Retail schedules the `extsh.` of the flag between the `lis` and the `addi` of the vtable's
// address, which is what the source order below produces.
extern "C" void* fn_27_1ED0(void** self, short flag) {
  if (self != 0) {
    *self = &lbl_27_data_EAC;
    if (flag > 0) {
      Free__7CMemoryFPCv(self);
    }
  }
  return self;
}

// 0x1E74, 0x5C bytes. The deleting destructor of the class whose vtable is `.data:0xEA0`: it
// stores its own vtable, then the base's (`.data:0xEAC`) through the **second** `if (self)` -
// retail's `beq` there re-uses the `mr. r31,r3` CR0, which is the address test MWCC emits for an
// inlined base destructor, and it survives the outer test that already established self is
// non-null.
extern "C" void* fn_27_1E74(void** self, short flag) {
  if (self != 0) {
    *self = &lbl_27_data_EA0;
    if (self != 0) {
      *self = &lbl_27_data_EAC;
    }
    if (flag > 0) {
      Free__7CMemoryFPCv(self);
    }
  }
  return self;
}

#endif // __MWERKS__
