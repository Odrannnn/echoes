// CRipperForwarders.cpp - a carve of Ripper's .text 0x00000514..0x0000055C: two functions, each
// of which is nothing but a call. It is a sub-range carve of dtk's `auto_00_00000178_text`, the
// same shape as `CRipperRelMain.cpp`'s carve of 0xD8..0x178 out of `auto_00_000000D8_text`.
//
//   0x514  fn_54_514  0x20  stwu / mflr / stw / bl fn_54_534 / epilogue
//   0x534  fn_54_534  0x28  stwu / mflr / cmplwi r3,0 / stw / beq / bl fn_54_55C / epilogue
//
// **Both pass both arguments through untouched**, so neither reads a member and neither is anything
// but a forwarder here: `fn_54_514` is `fn_54_534`, and `fn_54_534` is `if (p) fn_54_55C(p, q)`.
// `fn_54_534`'s `cmplwi r3,0` sits *between* the `mflr` and the `stw r0,0x14(r1)`, which is the
// schedule the source below produces without being told to (MWCC hoists the compare above the
// store because neither depends on the other), so this is a spelling that matched rather than one
// that was tuned to.
//
// **Neither is a vtable entry**, which is why nothing here is named: CRipper's own vtable is
// `.data:0x48` (`lbl_54_data_48`, 0x150 = 84 words: two leading words of offset-to-top and RTTI
// pointer, then one word per virtual, `build/G2ME01/Ripper/asm/auto_04_00000000_data.s`) and it
// lists `fn_54_C8C`, `fn_54_C20` and `fn_54_C18` but neither of these two. dtk names them for their
// offsets because it has nothing else to go on, and the DOL exports no symbol for them, so the
// dtk names are kept and `config/G2ME01/rels/Ripper/symbols.txt` needs no rename.
//
// `fn_54_55C` (0x55C, 0x13C) is called by name and defined nowhere, so dtk fills it from retail and
// the module's sha1 against `config/G2ME01/config.yml` still holds. Its body is a word-and-float
// copy of a `CAABox`-shaped value followed by a `CToken` copy-construct at +0x1C
// (`bl __ct__6CTokenFRC6CToken`), which is CRipper member data at offsets this tree has no class to
// name - see the note at the end of `CRipperRelMain.cpp` for why the block above this one is left
// alone.
//
// **The definitions are inside `#ifdef __MWERKS__`**, the arrangement `KrocussAccessors.cpp` and
// `RipperAccessors.cpp` both use and for the same measured reason: `fn_54_55C` is a symbol of the
// module, not of the port, so a host build that compiled the calls would add one undefined to the
// port's link to close nothing. The file is in `files.cmake` - unlike the module-head files, which
// are out because they define `RELMain`/`RELExit` - and on the host it is an empty translation unit.
//
// Definitions are in descending retail text order: mwcceppc emits definitions in reverse source
// order and mwldeppc keeps the object's `.text` order verbatim, so ascending would permute the
// module's bytes with objdiff still at 100% and only the module sha1 would catch it.

#ifdef __MWERKS__

extern "C" {
// .text 0x55C, 0x13C, unclaimed: called, never defined here.
void fn_54_55C(void*, void*);

// .text 0x534, 0x28 bytes: `if (p) fn_54_55C(p, q)`.
void fn_54_534(void* p, void* q) {
  if (p) {
    fn_54_55C(p, q);
  }
}

// .text 0x514, 0x20 bytes: the whole body is the call below, both arguments unchanged.
void fn_54_514(void* p, void* q) { fn_54_534(p, q); }
} // extern "C"

#endif // __MWERKS__
