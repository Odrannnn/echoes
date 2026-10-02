// CGeomBlobV2Accessors.cpp - GeomBlobV2's (module 25) first accessor trio, .text 0x2544..0x255C.
// The ranges come from `config/G2ME01/rels/GeomBlobV2/symbols.txt`:
//
//   0x2544 fn_25_2544 0x08  lwz r3, 0x15c(r3) / blr
//   0x254C fn_25_254C 0x08  lwz r3, 0x15c(r3) / blr
//   0x2554 fn_25_2554 0x08  stfs f1, 0x198(r3) / blr
//
// **This block is not the shape the other landed heads have, and that is measured rather than
// assumed.** Krocuss, MysteryFlyer, Tryclops, AtomicAlpha, IngPuddle and EmperorIngStage3 all open
// with the thirteen-accessor family: the `kInvalidUniqueId` store, the `li r3,0` predicate run,
// the `+0x44f` byte, the `+0x34c` flag, the `skDamageHitTime__10CPatterned` / `lbl_8041B758` float pair and the
// `+0x754` address. None of that is here. These are two pointer getters at `+0x15c` and a float
// setter at `+0x198`, and they name **no DOL global at all**, so the object relocates against
// nothing outside itself. That is why this file can be listed in `files.cmake`, unlike the module
// head sources, which call `fn_25_2490` and `fn_80229EAC` that the port cannot link.
//
// **The claim is three functions, not eight, and dtk's FORCEACTIVE list is why.**
// `build/G2ME01/GeomBlobV2/ldscript.lcf` names `fn_25_2544`, `fn_25_254C`, `fn_25_2554`,
// `fn_25_256C`, `fn_25_2574` and `fn_25_2578`; it does **not** name `fn_25_255C` or `fn_25_2564`,
// the two float *getters*, because nothing in the module's own data or code references them.
// Measured: claiming the whole `0x2544..0x2584` range in one unit built, linked and left
// `unit_fit.sh` reporting `claimed 64 / ours 64 / retail 64, fits` with no extra functions - and
// still broke the module's hash, `GeomBlobV2.rel` coming out **16 bytes short** of retail
// (33756 against 33772). Those are exactly the two unreferenced getters, dead-stripped by
// mwldeppc. So the range is split in two around them and the two middle functions stay in the
// unclaimed remainder, where dtk's retail bytes keep them:
//
//   0x2544..0x255C   this file, 3 functions, all FORCEACTIVE
//   0x255C..0x256C   unclaimed: fn_25_255C and fn_25_2564, the two float getters
//   0x256C..0x2584   CGeomBlobV2AccessorsTail.cpp, 3 functions, all FORCEACTIVE
//
// One unit cannot claim two discontiguous ranges, hence two files. See the dead-stripping entry in
// `docs/RUNNING_THE_DECOMP.md`: adding `scope:global` to `symbols.txt` does not fix this; only
// `force_active:` in `config/G2ME01/config.yml` would, and that is a config change this item does
// not make. **A `unit_fit.sh` "fits" on a REL unit is not the module's verdict** - only the module
// sha1 is, and it is what caught this.
//
// The offsets are raw because the class has no header here: `CGeomBlobV2`'s entity loader
// (`fn_25_2490`, 0x2490, 0xB4) allocates 0x1E0 bytes and needs the CActor/CPatterned hierarchy this
// tree does not model, and nothing names the byte at `+0x15c`. See the `CGeomBlobV2` section of
// `docs/research/raw_offsets.md`.
//
// Definitions are in descending retail text order: mwcceppc emits definitions in reverse source
// order and mwldeppc keeps the object's `.text` order verbatim, so ascending would permute the
// module's bytes with objdiff still at 100% and only tools/flip_test.sh would catch it.

extern "C" {
// .text 0x2554, 0x08 bytes. stores the float at +0x198.
void fn_25_2554(void* self, float value) {
  *reinterpret_cast< float* >(static_cast< char* >(self) + 0x198) = value;
}

// .text 0x254C, 0x08 bytes. the pointer at +0x15c.
void* fn_25_254C(const void* self) {
  return *reinterpret_cast< void* const* >(static_cast< const char* >(self) + 0x15C);
}

// .text 0x2544, 0x08 bytes. the pointer at +0x15c.
void* fn_25_2544(const void* self) {
  return *reinterpret_cast< void* const* >(static_cast< const char* >(self) + 0x15C);
}
}
