// CLumiteRelTail.cpp - Lumite's (module 39) teardown group, .text 0x738..0x7C0: the flag-guarded
// forwarder, the pair it calls into, and the registrar. A second unit in the same module, the
// same arrangement as `CFogOverlayRelStubs.cpp` - the head, `CLumiteRel.cpp`, claims
// `.text 0x0..0x190`, everything between the two ranges stays unclaimed, so dtk fills it from
// retail and the module's sha1 against `config/G2ME01/config.yml` still holds.
//
// Ranges from `config/G2ME01/rels/Lumite/symbols.txt`:
//
//   0x738 fn_39_738  0x40  copies the flag at +0x4C from `other` to `self`, and if it is set,
//                          calls fn_39_778; returns `self` (the epilogue's `mr r3,r31`)
//   0x778 fn_39_778  0x20  the call into the next one; r3 (self) passes straight through
//   0x798 fn_39_798  0x28  `if (self == 0) return; fn_39_7C0(self);`
//
// **This object is compiled with GC/2.7, not the REL default GC/1.3.2, and that is the whole of
// what fn_39_738 needed.** Retail's fn_39_738 reads the flag out of r4 *before* saving r31:
//
//   0x738 stwu r1,-0x10 / mflr r0 / stw r0,0x14(r1)
//   0x744 **lbz r0,0x4c(r4)**
//   0x748 **stw r31,0xc(r1) / mr r31,r3**
//   0x750 cmplwi r0,0 / stb r0,0x4c(r3) / beq / bl fn_39_778 / epilogue with `mr r3,r31`
//
// A previous run measured this function under GC/1.3.2 and recorded the save order as
// unreachable from the source - every spelling that carries the r31 spill puts it above the `lbz`,
// and the spelling that puts the `lbz` in retail's slot (a `void` function with no return of
// `self`) has no spill at all. **GC/2.7 is the difference, not the spelling**: compiled there, the
// save lands after the load, and fn_39_738 is instruction for instruction retail's. The other two
// functions in this object are unaffected - the whole 0x88-byte range is byte-identical to
// `build/G2ME01/Lumite/asm/auto_00_00000000_text.s` under 2.7, and the module's sha1 holds with
// this object in the link. (This is the same class of difference `configure.py` records for
// `CGameOptions.cpp`: a scheduling difference between two builds of the compiler family that no
// source spelling reaches.) The version is set per object in the `Rel("Lumite", ...)` block.
//
// **fn_39_798 is the early-return spelling, not `if (self) { call(); }`.** The two are the same
// program and MWCC schedules them differently, measured at GC/1.3.2:
//
//   if (self) {...}        stwu / mflr / stw r0,0x14 / cmplwi r3,0 / beq / bl / ...    wrong
//   if (self == 0) return; stwu / mflr / cmplwi r3,0 / stw r0,0x14 / beq / bl / ...    retail
//
// retail's `cmplwi r3,0` sits *above* the saved-LR store, so the guard has to be an early return.
//
// **fn_39_7C0 (0x7C0, 0x13C) is left retail.** It is the module's copy/steal constructor - three
// floats, a flag-and-word pair, and three `optional_object<CToken>` members each copying through
// `__ct__6CTokenFRC6CToken` and then calling `Lock__6CTokenFv` - so it needs the member types
// modelled, and it is not this item's to guess. It is declared here only so the forwarder's call
// resolves by the dtk name, exactly as `CMysteryFlyerRel.cpp` declares its unclaimed
// `fn_45_2BBC`. Its declaration carries one parameter and no return value: retail's `fn_39_798`
// sets r3 to the pointer and jumps, and the callee's r4 is whatever the module's own caller left
// there, so the calling shape below is the whole of what this unit has to reproduce.
//
// This file is listed in `files.cmake`, and **its host branch defines nothing at all** - the same
// arrangement `CLumiteRel.cpp` uses for RELMain/RELExit, and the reason is the one
// `tools/check_files_cmake.py` enforces: a `Matching` object has to be either in `files.cmake` or
// in the checker's judge-owned EXCLUDED set, and the only automatic exemption is a unit that
// defines RELMain/RELExit, which this one deliberately does not. An empty host branch keeps the
// port's undefined count where it is while the MWCC branch is the retail source and reproduces
// retail's bytes.
//
// Definitions are in descending retail text order (mwcceppc emits definitions in reverse source
// order); `python3 tools/check_decl_order.py --unit MetroidPrime/ScriptObjects/CLumiteRelTail.cpp`.

extern "C" {

// .text 0x7C0, 0x13C: the module's own copy constructor for the structure fn_39_738 guards.
// Left retail - see the note above.
void fn_39_7C0(void* self);

#ifdef __MWERKS__

void fn_39_778(void* self);

// .text 0x798, 0x28 bytes. `self` in r3.
void fn_39_798(void* self) {
  if (self == 0) {
    return;
  }
  fn_39_7C0(self);
}

// .text 0x778, 0x20 bytes. `self` in r3, which the call passes straight through.
void fn_39_778(void* self) { fn_39_798(self); }

// .text 0x738, 0x40 bytes. `self` in r3, `other` in r4. The flag is read once into r0 and stored
// from that same register, and `self` comes back in r3 through r31 - see the note above for why
// this one function is compiled with 2.7.
void* fn_39_738(void* self, const void* other) {
  const unsigned char flag = static_cast< const unsigned char* >(other)[0x4C];
  static_cast< unsigned char* >(self)[0x4C] = flag;
  if (flag != 0) {
    fn_39_778(self);
  }
  return self;
}

#endif
}
