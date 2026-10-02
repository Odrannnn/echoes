// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the addresses
// and sizes come from `config/G2ME01/symbols.txt:962-968`, the instructions are the ones dtk
// itself emitted into `build/G2ME01/asm/auto_03_80032774_text.s:8-252`, and the bodies below are
// the C++ those bytes are the compilation of.
//
// .text 0x80032774..0x80032A98, 0x324 = 804 bytes, 7 functions:
//
//   fn_80032774              0x80032774  0x88  34 instructions  `CTweakPlayerRes`'s deleting dtor
//   __dt__11CMayaSplineFv    0x800327FC  0x58  22 instructions  `CMayaSpline::~CMayaSpline()`
//   fn_80032854              0x80032854  0x54  21 instructions  frees the pointer at +0xC
//   fn_800328A8              0x800328A8  0xB0  44 instructions  `single_ptr<CTweakPlayerGun>::~`
//   fn_80032958              0x80032958  0x54  21 instructions  `single_ptr<T>::~`, trivial T
//   fn_800329AC              0x800329AC  0x54  21 instructions  `single_ptr<T>::~`, trivial T
//   fn_80032A00              0x80032A00  0x98  38 instructions  `single_ptr<CTweakParticle>::~`
//
// **All seven are registered global destructors, and the `.ctors` entry names each one.**
// `fn_800324A4` (`build/G2ME01/asm/auto_fn_800324A4_text.s:9-126`, the tweak-global constructor,
// itself unclaimed) hands every one of them to `__register_global_object` with the address of a
// tweak global in `.sbss`, and `docs/research/tweak_globals.md` ("The fifteen slots, one row per
// store") already maps those slots.  In address order of the destructors:
//
//   fn_80032A00   .ctors 0x8003254C-0x80032560   gpTweakParticle            (asm 101-106)
//   fn_800329AC   .ctors 0x80032568-0x80032598   gpTweakPlayerB, gpTweakPlayerA
//   fn_80032958   .ctors 0x800325A0-0x800325D0   gpTweakPlayerControlsB, gpTweakPlayerControlExpert
//   fn_800328A8   .ctors 0x800325D8-0x80032608   gpTweakPlayerGunMulti, gpTweakPlayerGunSingle
//   fn_8003271C   .ctors 0x80032610-0x80032624   gpTweakPlayerRes   <- in Carve80032674.c, below
//   fn_800326C8   .ctors 0x80032628-0x80032640   gpTweakSlideShow   <- in Carve80032674.c
//   fn_80032674   .ctors 0x80032648-0x8003265C   gpTweakTargeting   <- in Carve80032674.c
//   (fn_80032BE8/94/40/EC/98/A98 at 0x800324A4-0x80032544 are `Carve80032A98.c`/`BE8.c`, above.)
//
// Each global's declared type (`include/MetroidPrime/Tweaks/`) fixes which destructor it is, and
// the bodies below follow from that type rather than from the shape of the bytes:
//
//   `gpTweakParticle` is `rstl::single_ptr< CTweakParticle >` (`CTweakParticle.hpp:21`), and
//   `CHECK_SIZEOF(CTweakParticle, 0x34)` with `mData` at +0 and three `rstl::string` members at
//   +4, +0x14 and +0x24 (`CTweakParticle.hpp:13-16`).  `fn_80032A00` is therefore
//   `~single_ptr<CTweakParticle>` with `delete mPtr` **inlined**: `~CTweakParticle` is implicit
//   (the class declares no destructor), so the compiler emits the three string releases itself,
//   in reverse declaration order - +0x24, +0x14, +0x04, each as
//   `addic. r0,r31,off / beq / addi r3,r31,off / bl internal_dereference__...Fv` - and then
//   `Free(mPtr)`.  The same shape is `fn_82_480` in the REL (`docs/research/tweak_globals.md`,
//   "The 52-byte class").
//
//   `gpTweakPlayerGunMulti`/`Single` are `rstl::single_ptr< CTweakPlayerGun >`
//   (`CTweakPlayerGun.hpp:63-64`), and `CHECK_SIZEOF(CTweakPlayerGun, 0xf8)` with `mData` at +0
//   and `rstl::reserved_vector< SWeaponInfo, 4 > mBeamInfo` at +4
//   (`CTweakPlayerGun.hpp:57-58`).  `fn_800328A8` is `~single_ptr<CTweakPlayerGun>` with the
//   implicit `~CTweakPlayerGun` inlined, i.e. `~reserved_vector` -> `destroy_elements()`
//   (`rstl/reserved_vector.hpp:126-136`) over the count at +4.  `SWeaponInfo` is **not** declared
//   trivially destructible, so the loop is entered, and `destroy()`'s `in->~T()` inlines to
//   nothing (all three of its members are POD) - which is why the emitted loop has an empty body
//   and only walks its index, in the byte-unrolled form mwcceppc gives such a loop.  Read off the
//   bytes, not guessed: `lwz r6,4(r3)` is `mCount`, `li r4,0` the index, and the two `bdnz` loops
//   are the unrolled bulk and the remainder.
//
//   `gpTweakPlayerB`/`A` (`CTweakPlayer.hpp`) and `gpTweakPlayerControlsB`/`ControlExpert`
//   (`CTweakPlayerControls.hpp`) hold classes whose destructors are trivial, so
//   `~single_ptr<T>` is `Free(mPtr)` with **no** destructor call and no null test -
//   `fn_800329AC`/`fn_80032958` are the same 21 instructions as `fn_80032B94`
//   (`Carve80032A98.c`) and eleven other copies in the DOL.
//
//   `fn_80032774` is the deleting destructor of `CTweakPlayerRes` itself (604 bytes, five
//   `SLdrSpline` members at +0x104, +0x148, +0x18C, +0x1D0, +0x214, `docs/research/
//   tweak_globals.md` row 13): the five `__dt__11CMayaSplineFv` calls run in reverse declaration
//   order and the stride 0x44 is `CHECK_SIZEOF(CMayaSpline, 0x44)`.  `fn_8003271C`, the
//   `single_ptr<CTweakPlayerRes>` destructor in `Carve80032674.c` below, is its only caller and
//   passes the deleting flag `1`.
//
//   `__dt__11CMayaSplineFv` (0x800327FC) is `CMayaSpline::~CMayaSpline()`: one call to
//   `fn_80032854` on `self + 8`, the member at +8 (`Kyoto/Math/CMayaSpline.hpp`).  Its bytes are
//   the DOL's only other copy of this 22-instruction shape apart from
//   `__dt__Q314CMemoryCardSys13CCardFileInfo9SSaveSlotFv` (0x8030ABC4), which is the same 22
//   instructions with another callee and another offset - measured with the seeder's mask
//   (`bl` targets and `lis` immediates), not by eye.
//
//   `fn_80032854` frees the pointer at +0xC of its receiver.  Its 21 instructions are the same
//   shape as the matched `fn_80004744` (`src/MetroidPrime/Carve80004744.c`) and 61 other copies
//   in the DOL (measured, same mask: 62 byte-shapes including this one) - the family that file
//   documents.
//
// **This unit is a `.cpp`, and that is load-bearing, not a style choice.**  A hand-written C body
// cannot produce `fn_800328A8`'s or `fn_80032A00`'s inlined member teardowns: the string releases
// are the compiler's own member-destruction code (`addic.`/`beq` around a computed `this + off`)
// and the count-and-walk loop is `reserved_vector::destroy_elements()` with the element destructor
// inlined away.  Both are what `delete mPtr` compiles to when the pointee's destructor is
// implicit, so the bodies below are written as that `delete` and the two class types come from
// their real headers.  `extern "C"` is what keeps the seven symbols unmangled - `fn_<addr>` names
// and `__dt__11CMayaSplineFv` alike - so objdiff pairs every one of them.
//
// **The spelling of each body is measured, not free.**  `delete self->mPtr` with the parameter
// declared as the class pointer is what reproduces retail's register assignment (self in the lower
// callee-saved register, the `short` flag in the higher one: `mr r31,r4` then `mr. r30,r3`); a
// local `T* p = (T*)selfv;` before the null test, or an explicit `self->~single_ptr<T>()` call,
// flips the two registers or adds a second null-test branch - 8 and 38 differing instructions
// respectively, measured this run.  The five plain bodies use the `void*` parameter directly
// (`*(const void**)self`, `(char*)self + off`) for the same reason.
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object `.text` verbatim, so an
// ascending file is a permuted `.text` - 100.00% per function and a broken DOL.  Only
// `tools/flip_test.sh` catches that.  `tools/check_decl_order.py --unit
// main/MetroidPrime/Factories/Carve80032774` is the cheap check.
//
// Its own unit because a claim may not span an unclaimed gap, and because this is exactly the
// unclaimed run between two claims: below, `MetroidPrime/Factories/Carve80032674.c`
// (0x80032674..0x80032774) ends where this claim starts, and above,
// `MetroidPrime/Factories/Carve80032A98.c` (0x80032A98..0x80032BE8) starts where it ends.  Both
// are `Matching`, so the claim starts and ends on unit boundaries and touches no other range.
// The directory is retail's own, taken from the nearest claimed ranges (`CAssetFactory.cpp` below
// and `Carve80032A98.c` above are both `MetroidPrime/Factories/`).
//
// Port: `Free__7CMemoryFPCv` and `internal_dereference__Q24rstl66basic_string<c,...>Fv` are both
// defined by units that are in `files.cmake` (`src/Kyoto/Alloc/PortMwccNew.cpp:34` and
// `src/rstl/rstl_strings.cpp:150`), so this unit adds no undefined symbol to the host link.  It
// *does* define `__dt__11CMayaSplineFv`, which until now only
// `src/MetroidPrime/ScriptLoader/Carve80220294.c`'s `#ifdef TARGET_PC` stand-in provided; that
// stand-in is deleted in the same change, because two definitions of one symbol is a port-link
// duplicate.  No `PortLinkStubs.cpp` entry names any of these seven.

#include "MetroidPrime/Tweaks/CTweakParticle.hpp"
#include "MetroidPrime/Tweaks/CTweakPlayerGun.hpp"

/** 0x802CE388, `symbols.txt:12992`, size 0x64: retail's `CMemory::Free(void const*)`.  It sits
 *  inside `Kyoto/Alloc/CMemory.cpp` (`.text` 0x802CE224..0x802CE72C), which is `Matching`, so our
 *  own object for that unit supplies these bytes in the DOL link.  Declared, never defined here.
 *  `src/Kyoto/Alloc/PortMwccNew.cpp:34` defines it under that name for the host link. */
extern "C" void Free__7CMemoryFPCv(const void* ptr);

extern "C" void* fn_80032A00(rstl::single_ptr< CTweakParticle >* self, short flag);
extern "C" void* fn_800329AC(void* self, short flag);
extern "C" void* fn_80032958(void* self, short flag);
extern "C" void* fn_800328A8(rstl::single_ptr< CTweakPlayerGun >* self, short flag);
extern "C" void* fn_80032854(void* self, short flag);
extern "C" void* __dt__11CMayaSplineFv(void* self, short flag);
extern "C" void* fn_80032774(void* self, short flag);

/** 0x80032A00, `symbols.txt:968`, 0x98 = 152 bytes, 38 instructions:
 *  `rstl::single_ptr< CTweakParticle >::~single_ptr()` - the destructor `.ctors` 0x8003254C
 *  registers for `gpTweakParticle`.  `delete mPtr` is retail's own `~single_ptr` body
 *  (`include/rstl/single_ptr.hpp:42`), inlined with the pointee's implicit destructor: the three
 *  `rstl::string` members at +0x24, +0x14 and +0x04 released in that order, then `Free(mPtr)`,
 *  and only then the receiver's own `Free` behind the flag. */
extern "C" void* fn_80032A00(rstl::single_ptr< CTweakParticle >* self, short flag) {
  if (self) {
    delete self->mPtr;
    if (flag > 0) {
      Free__7CMemoryFPCv(self);
    }
  }
  return self;
}

/** 0x800329AC, `symbols.txt:967`, 0x54 = 84 bytes, 21 instructions: the same
 *  `~single_ptr<T>` for `gpTweakPlayerA`/`gpTweakPlayerB` (`.ctors` 0x80032568-0x80032598).
 *  `T`'s destructor is trivial, so retail's body is `Free(*(void**)self)` with no destructor call
 *  and no null test on the member - the shape `fn_80032B94` (`Carve80032A98.c`) matched. */
extern "C" void* fn_800329AC(void* self, short flag) {
  if (self) {
    Free__7CMemoryFPCv(*(const void**)self);
    if (flag > 0) {
      Free__7CMemoryFPCv(self);
    }
  }
  return self;
}

/** 0x80032958, `symbols.txt:966`, 0x54 = 84 bytes, 21 instructions: the same body again, for
 *  `gpTweakPlayerControlsB`/`gpTweakPlayerControlExpert` (`.ctors` 0x800325A0-0x800325D0). */
extern "C" void* fn_80032958(void* self, short flag) {
  if (self) {
    Free__7CMemoryFPCv(*(const void**)self);
    if (flag > 0) {
      Free__7CMemoryFPCv(self);
    }
  }
  return self;
}

/** 0x800328A8, `symbols.txt:965`, 0xB0 = 176 bytes, 44 instructions:
 *  `rstl::single_ptr< CTweakPlayerGun >::~single_ptr()` for
 *  `gpTweakPlayerGunMulti`/`gpTweakPlayerGunSingle` (`.ctors` 0x800325D8-0x80032608), with the
 *  pointee's implicit destructor inlined - the `reserved_vector< SWeaponInfo, 4 >` at +4 walked by
 *  the count at its +0 and then freed. */
extern "C" void* fn_800328A8(rstl::single_ptr< CTweakPlayerGun >* self, short flag) {
  if (self) {
    delete self->mPtr;
    if (flag > 0) {
      Free__7CMemoryFPCv(self);
    }
  }
  return self;
}

/** 0x80032854, `symbols.txt:964`, 0x54 = 84 bytes, 21 instructions: frees the pointer at +0xC of
 *  its receiver, then the receiver itself behind the flag.  Retail names it nothing and its caller
 *  is `__dt__11CMayaSplineFv` below (`addi r3,r30,0x8 / li r4,-1`), so the receiver is the member
 *  at +8 of a `CMayaSpline`. */
extern "C" void* fn_80032854(void* self, short flag) {
  if (self) {
    Free__7CMemoryFPCv(*(const void**)((char*)self + 0xC));
    if (flag > 0) {
      Free__7CMemoryFPCv(self);
    }
  }
  return self;
}

/** 0x800327FC, `symbols.txt:963`, 0x58 = 88 bytes, 22 instructions:
 *  `CMayaSpline::~CMayaSpline()` - the name is retail's own, and `extern "C"` is what keeps it
 *  spelled that way instead of mangled twice.  One member teardown, at +8, through `fn_80032854`
 *  above with the `-1` flag that means "destroy, do not free me". */
extern "C" void* __dt__11CMayaSplineFv(void* self, short flag) {
  if (self) {
    fn_80032854((char*)self + 8, -1);
    if (flag > 0) {
      Free__7CMemoryFPCv(self);
    }
  }
  return self;
}

/** 0x80032774, `symbols.txt:962`, 0x88 = 136 bytes, 34 instructions: `CTweakPlayerRes`'s deleting
 *  destructor, reached from `fn_8003271C` (`Carve80032674.c`, 0x8003271C..0x80032774) with the
 *  deleting flag `1`.  Five `SLdrSpline` members - `typedef CMayaSpline` - at +0x214, +0x1D0,
 *  +0x18C, +0x148 and +0x104, destroyed in reverse declaration order with the stride
 *  `CHECK_SIZEOF(CMayaSpline, 0x44)`. */
extern "C" void* fn_80032774(void* self, short flag) {
  if (self) {
    __dt__11CMayaSplineFv((char*)self + 0x214, -1);
    __dt__11CMayaSplineFv((char*)self + 0x1D0, -1);
    __dt__11CMayaSplineFv((char*)self + 0x18C, -1);
    __dt__11CMayaSplineFv((char*)self + 0x148, -1);
    __dt__11CMayaSplineFv((char*)self + 0x104, -1);
    if (flag > 0) {
      Free__7CMemoryFPCv(self);
    }
  }
  return self;
}
