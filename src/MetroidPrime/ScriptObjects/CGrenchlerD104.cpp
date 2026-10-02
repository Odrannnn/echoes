// CGrenchlerD104.cpp - Grenchler's (module 27) .text 0xD104..0xD178: three of CGrenchler's
// non-virtual class methods, the first range this module has claimed above its head.
//
//   0xD104 fn_27_D104  0x08  the word at +0x254
//   0xD10C fn_27_D10C  0x04  empty body (`blr`) - vtable slot 23, `FluidFXThink`'s override,
//                            which `tools/rel_class_map.py Grenchler` names from the module's
//                            own vtable (`.data:0xA54`, slot 0x64)
//   0xD110 fn_27_D110  0x68  `p0F28 != 0 && b09FC == 0 && f5C + lbl_27_rodata_1C4 <
//                            fn_27_B8F0(self)` as a predicate
//
// The three are contiguous - `config/G2ME01/rels/Grenchler/symbols.txt` gives 0xD104 size 0x8,
// 0xD10C size 0x4 and 0xD110 size 0x68, so 0xD104 + 0x8 = 0xD10C, + 0x4 = 0xD110, and
// 0xD110 + 0x68 = 0xD178 is the next function - so the claim covers them and nothing else. Every
// byte around them stays unclaimed and dtk fills it from retail out of the `auto_*` remainders,
// which is what keeps the module's sha1 at `config/G2ME01/config.yml`'s
// `de128c91f467b5b61b4de4a2ed6b217044a975c5`.
//
// Every body is read off `build/G2ME01/Grenchler/asm/auto_00_00000168_text.s`. The two names this
// range reaches are the module's own: `fn_27_B8F0` is unclaimed (it is inside dtk's auto unit, so
// the link takes it from retail) and `lbl_27_rodata_1C4` is the module's `.rodata`, stored by
// dtk's `auto_03_00000000_rodata` object. Both are declared under their retail spellings
// (`extern "C"`), so the call and the float load resolve to exactly the symbols the module's
// retail bytes use.
//
// **The class is modelled with padding, not raw offsets.** `fn_27_D110`'s `lbz r0, 0x9fc(r31);
// cmplwi r0, 0` is a plain byte read through a named member, and the pointer and float it reads
// are named the same way, so `tools/check_raw_offsets.py` counts no raw offset in this file.
// `fn_27_D10C` is an empty virtual override: an empty `extern "C"` body compiles to the one `blr`
// retail has, and the vtable's data relocation is what keeps it (it is already in the module's
// `ldscript.lcf` FORCEACTIVE block).
//
// The bodies are inside `#ifdef __MWERKS__` and the host branch is empty, the arrangement
// `CLumiteRelTail.cpp` and `CPlantScarabSwarmTail.cpp` use: `check_files_cmake.py` requires every
// configure.py `Matching` object to be in `files.cmake`, and only a RELMain/RELExit unit is
// exempt - but a host body would make the port link `fn_27_B8F0` and `lbl_27_rodata_1C4`, which
// the port does not have.
//
// Definitions are in descending retail text order: mwcceppc emits definitions in reverse source
// order and mwldeppc keeps the object's `.text` order verbatim, so ascending would permute the
// module's bytes with objdiff still at 100% and only the module's sha1 would catch it.

#include "types.h"

/** 0xD104's range: only the pointer at +0x254 is read. The class is CGrenchler, whose full
 *  layout this tree does not model, so the gaps are opaque padding of the measured size rather
 *  than invented members. */
struct SGrenchlerPtr254 {
  uchar mUnknown00[0x254];
  void* mPtr254; // +0x254
};

/** 0xD110's range: the pointer at +0xF28, the byte at +0x9FC and the float at +0x5C. */
struct SGrenchlerPredicate {
  uchar mUnknown00[0x5C];
  float mFloat5c; // +0x5C
  uchar mUnknown60[0x9FC - 0x60];
  uchar mByte9fc; // +0x9FC
  uchar mUnknown9fd[0xF28 - 0x9FD];
  void* mPtrF28; // +0xF28
};

/** 0xB8F0, unclaimed: takes the receiver in r3 and returns its result in f1, exactly as the
 *  call at 0xD13C and the `fcmpo` after it read it. */
extern "C" float fn_27_B8F0(const void* self);

/** The module's own `.rodata` constant at 0x1C4, stored by dtk's `auto_03_00000000_rodata`. */
extern "C" const float lbl_27_rodata_1C4;

#ifdef __MWERKS__

// 0xD110, 0x68 bytes. Three independent reads and a call, with the comparison against the
// callee's result: retail keeps the callee's f1 live across the two `lfs`/`fadds` instructions,
// which is what the local `value` is for.
extern "C" bool fn_27_D110(const SGrenchlerPredicate* self) {
  if (self->mPtrF28 != 0 && self->mByte9fc == 0) {
    float value = fn_27_B8F0(self);
    if (lbl_27_rodata_1C4 + self->mFloat5c < value) {
      return true;
    }
  }
  return false;
}

// 0xD10C, 0x04 bytes. The empty override `tools/rel_class_map.py Grenchler` reports at vtable
// slot 23 of `.data:0xA54` - `FluidFXThink`'s. Nothing calls it except the vtable's data
// relocation, which is why the module's FORCEACTIVE block names it.
extern "C" void fn_27_D10C(void*) {}

// 0xD104, 0x08 bytes. A leaf: load the word at +0x254 and return it.
extern "C" void* fn_27_D104(const SGrenchlerPtr254* self) { return self->mPtr254; }

#endif // __MWERKS__
