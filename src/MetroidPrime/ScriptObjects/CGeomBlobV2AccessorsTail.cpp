// CGeomBlobV2AccessorsTail.cpp - GeomBlobV2's (module 25) last accessor trio,
// .text 0x256C..0x2584. The ranges come from `config/G2ME01/rels/GeomBlobV2/symbols.txt`:
//
//   0x256C fn_25_256C 0x08  stfs f1, 0x190(r3) / blr
//   0x2574 fn_25_2574 0x04  blr
//   0x2578 fn_25_2578 0x0C  li r0,0 / stb r0, 0x18(r3) / blr
//
// This is a **second** claim, separate from `CGeomBlobV2Accessors.cpp` above, and it is separate
// because of a dead-stripping measurement rather than a choice:
//
//   0x2544..0x255C   CGeomBlobV2Accessors.cpp, 3 functions, all in dtk's FORCEACTIVE list
//   0x255C..0x256C   unclaimed: fn_25_255C and fn_25_2564, the two float getters
//   0x256C..0x2584   this file, 3 functions, all in dtk's FORCEACTIVE list
//
// `build/G2ME01/GeomBlobV2/ldscript.lcf` does not name `fn_25_255C` or `fn_25_2564` - nothing in the
// module's own data or code references them - so a unit claiming all eight built, linked, and had
// `unit_fit.sh` report `claimed 64 / ours 64 / retail 64, fits` with no extra functions, and still
// produced a `GeomBlobV2.rel` **16 bytes short** of retail (33756 against 33772). Those two
// functions are the 16 bytes. One unit cannot claim two discontiguous ranges, hence two files and
// a retail gap in the middle. See `CGeomBlobV2Accessors.cpp` for the full note, and the
// dead-stripping entry in `docs/RUNNING_THE_DECOMP.md`: `scope:global` in `symbols.txt` does not
// fix it, only `force_active:` in `config/G2ME01/config.yml` would.
//
// **`fn_25_2574` and `fn_25_2578` are vtable entries rather than free functions.** `.data:0x10`
// (`lbl_25_data_10`, 0x80 bytes) stores `fn_25_2578` at offset 0x48 and `fn_25_2574` at 0x4C, and
// `.data:0xA8` (`lbl_25_data_A8`, 0x7C) stores `fn_25_49D8` and `fn_25_49D4` at the same two
// offsets - the two vtables' pair of slots between `GetDamageVulnerability`'s two overloads and
// `GetOrbitPosition`. `fn_25_2574` is a bare `blr`, so it is an empty virtual; `fn_25_2578` clears
// the byte at `+0x18`.
//
// The offsets are raw because the class has no header here; see the `CGeomBlobV2` section of
// `docs/research/raw_offsets.md` and `CGeomBlobV2Accessors.cpp`'s header.
//
// Definitions are in descending retail text order: mwcceppc emits definitions in reverse source
// order and mwldeppc keeps the object's `.text` order verbatim, so ascending would permute the
// module's bytes with objdiff still at 100% and only tools/flip_test.sh would catch it.

extern "C" {
// .text 0x2578, 0x0C bytes. Vtable entry 0x48 of the module's first vtable. `li r0,0 ; stb r0,
// 0x18(r3) ; blr` - the displacement 0x18 is the byte, not a zero store at the object head, which
// is the one spelling that got this wrong first: it produced a `.rel` one byte from retail at
// 0x2633, `stb r0, 0x0(r3)` where retail has `stb r0, 0x18(r3)`.
void fn_25_2578(void* self) {
  *reinterpret_cast< unsigned char* >(static_cast< char* >(self) + 0x18) = 0;
}

// .text 0x2574, 0x04 bytes. Vtable entry 0x4C of the module's first vtable: `blr` and nothing
// else, an empty virtual.
void fn_25_2574() {}

// .text 0x256C, 0x08 bytes. stores the float at +0x190.
void fn_25_256C(void* self, float value) {
  *reinterpret_cast< float* >(static_cast< char* >(self) + 0x190) = value;
}
}
