// CIngBoostBallGuardian388C.cpp - IngBoostBallGuardian's (module 30) 0x20-byte record copy
// constructor, `.text` 0x388C..0x38E0: one leaf function, 0x54 bytes.
//
//   0x388C fn_30_388C 0x54  ten member copies, 3 floats, 3 words, 4 halfwords, 1 byte
//
// **The record is CHealthInfo and the function is its copy constructor.** Retail's 84 bytes occur
// exactly once in `build/G2ME01/main.dol` (which the gate shows is retail byte-identical), at file
// offset 0x6DB60; `build/G2ME01/asm/auto_03_80070DB4_text.s:10` gives `/* 80070DB4 0006DBB4 */`,
// which fixes the mapping at `vaddr = fileoff + 0x80003200` and puts them at 0x80070D60 -
// `config/G2ME01/symbols.txt:2082`, `__ct__11CHealthInfoFRC11CHealthInfo = .text:0x80070D60; //
// type:function size:0x54`. Same length, same bytes, and the shape is a constructor copying into
// the hidden return pointer at r3, which is why the copy is spelled with `self` in r3 and `other`
// in r4 rather than as a free-standing helper.
//
// **The layout is retail's and it is load-bearing.** The four halfword members at +0x10, +0x12,
// +0x18 and +0x1A have to be four separate members: a nested pair of `unsigned short` collapses
// into one `lwz`/`stw` and the function comes out 68 bytes against retail's 84. The byte at +0x1C
// is loaded with `lbz`, so it is `unsigned char` and not `bool`.
//
// **MWCC's two-deep load/store schedule, and `mw_version` is what produces it.** Retail issues
// every load two instructions ahead of its store (`lfs f0`/`lfs f1`, then two stores, then the
// next two loads) and keeps two temporaries live. Under the module default `GC/1.3.2` the same
// whole-object assignment compiles to plain pairs - one live register, source order:
// `lfs f0,0(r4) / stfs f0,0(r3) / lfs f0,4(r4) / ...`. Under `GC/2.7` it is byte-identical to
// retail (measured; `GC/2.0p1`, `GC/2.5` and `GC/2.6` give the same 84 bytes, `GC/3.0a5` does
// not). That is why the `Object(...)` entry in configure.py carries a per-object
// `mw_version="GC/2.7"` - the same override `CLumiteRelTail.cpp` and `CSandBossRelTail.cpp` use -
// rather than the module-wide one, which would recompile the six other module-30 units.
//
// This tree's `include/MetroidPrime/CHealthInfo.hpp` does **not** describe these bytes (three
// floats, two `CWeaponMode`, four `TUniqueId`) and is not edited for this: it is included by CAi,
// CScriptActor and seven other units. The carve therefore carries a local type with retail's
// layout, the arrangement `CIngBoostBallGuardianRel.cpp` uses for `CIngBoostBallGuardianDispatch`.
// A copy constructor says what each member is made of, not what the record is called.
//
// **Leaf**, measured on `build/G2ME01/IngBoostBallGuardian/obj/auto_00_000038E0_text.o` and
// auto_00_00011CD8_text.o: the function is 21 instructions of load/store and one `blr`, and their
// `.rela.text` holds nothing in 0x388C..0x38E0, so the bytes are the whole of the claim and there
// is no callee to declare.
//
// **No dead-strip hazard, and that is measured.** fn_30_388C is not in
// `build/G2ME01/IngBoostBallGuardian/ldscript.lcf`'s FORCEACTIVE list, but both objects above name
// it as an undefined symbol (`powerpc-eabi-nm -u`), so dtk's own objects hold the reference and
// this unit's `.text` survives the link. No `force_active:` entry and no
// `config/G2ME01/config.yml` change.
//
// **The claim is 0x388C..0x38E0 and not the whole 0x3790..0x38E0 run.** The three functions in
// front of it (`fn_30_3790`, `fn_30_37E0`, `fn_30_3838`) are null-guarded constructors that all
// `bl fn_30_12DB4`, which is itself in an unclaimed region, so claiming them means reproducing a
// function this unit cannot see.
//
// In `files.cmake` with an empty host branch, as `CIngBoostBallGuardianBits.cpp` explains.
// Definitions are in descending retail text order (one function).

extern "C" {

#ifdef __MWERKS__

// Retail's 0x20-byte record as `config/G2ME01/symbols.txt:2082` names it; see the note above.
struct RelCHealthInfo {
  float f0;
  float f4;
  float f8;
  int wC;
  unsigned short h10;
  unsigned short h12;
  int w14;
  unsigned short h18;
  unsigned short h1A;
  unsigned char b1C;
};

// .text 0x388C, 0x54 bytes. `self` is the hidden return pointer in r3, `other` in r4; the whole
// body is one assignment, and that is the spelling `GC/2.7` compiles to retail's schedule.
void fn_30_388C(RelCHealthInfo* self, const RelCHealthInfo& other) { *self = other; }

#endif
}
