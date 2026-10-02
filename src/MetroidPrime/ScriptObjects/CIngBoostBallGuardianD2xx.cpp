// CIngBoostBallGuardianD2xx.cpp - IngBoostBallGuardian's (module 30) `GetDamageVulnerability`,
// `AddToRenderer` and `PreRenderAllViewports` overrides, `.text` 0xD230..0xD2E0, 0xB0 bytes, three
// contiguous functions of the module's *first* vtable (sec 5 +0x9C0, the 119-slot one whose nearest
// DOL base is `CScriptDoor`):
//
//   0xD230 fn_30_D230 0x20  `const CDamageVulnerability& GetDamageVulnerability() const`
//                           (slot 14): the call is `ImmuneVulnerability__20CDamageVulnerabilityFv`
//   0xD250 fn_30_D250 0x70  `void AddToRenderer(const CStateManager&) const` (slot 8)
//   0xD2C0 fn_30_D2C0 0x20  `void PreRenderAllViewports(CStateManager&)` (slot 11): a forwarder to
//                           `PreRenderAllViewports__10CPatternedFR13CStateManager`
//
// The names and argument lists come from the item's brief (`tools/rel_class_map.py` reads them off
// the DOL vtable's matching slots) and each signature is what the prologue says: slot 14's is an
// eight-byte address accessor and slot 11's a two-instruction forwarder, both of which only a
// `(const) X&`/void return with `this` in r3 and no hidden argument produces. The two calls are
// `extern "C"` declarations of the DOL's own mangled names, the arrangement
// `CIngBoostBallGuardianRel.cpp` uses for `fn_8022FFC4`; both names are in
// `config/G2ME01/symbols.txt`. `fn_30_D230` passes `this` straight to a *static* member, so its
// body is exactly retail's `bl`/`blr`.
//
// **`fn_30_D250` is a switch on the word at +0xAE4 and its case set is read off the branch tree,
// not assumed**: 2, 4, 6, 7, 8 and 9 fall to `CPatterned::AddToRenderer`, 3 and 5 to a
// `float x11BC < lbl_30_rodata_E0` test that calls it only when the comparison holds, and nothing
// else calls anything. The compiled bytes are byte-identical to retail's (measured on a scratch
// file against `build/G2ME01/IngBoostBallGuardian/obj/auto_00_0000C6CC_text.o`), including MWCC's
// binary-search branch tree.
//
// `lbl_30_rodata_E0` is `.rodata:0xE0` (`size:0x4 data:float`) of this module: the split claims
// `.text` only, so dtk's `.rodata` object defines it and this is an ordinary cross-object
// relocation, exactly as `lbl_30_rodata_64` is in `CIngBoostBallGuardianRel.cpp`.
//
// **No dead-strip hazard, and that is measured**: none of the three is in
// `build/G2ME01/IngBoostBallGuardian/ldscript.lcf`'s FORCEACTIVE list, but
// `auto_04_00000000_data.o` names all three as undefined symbols (the module's own vtable at
// `.data`+0x9C0 stores slot 14 at +0x38, slot 8 at +0x20 and slot 11 at +0x2C), so dtk's object
// holds the references. No `force_active:` entry and no `config/G2ME01/config.yml` change.
//
// This file is in `files.cmake` with an empty host branch, exactly as
// `CIngBoostBallGuardianBits.cpp` explains.
//
// Definitions are in descending retail text order (mwcceppc emits definitions in reverse source
// order and mwldeppc keeps the object's `.text` order verbatim).

extern "C" {

#ifdef __MWERKS__

// The three DOL callees, by the names `config/G2ME01/symbols.txt` gives them.
const void* ImmuneVulnerability__20CDamageVulnerabilityFv();
void AddToRenderer__10CPatternedCFRC13CStateManager(const void* self, const void* mgr);
void PreRenderAllViewports__10CPatternedFR13CStateManager(void* self, void* mgr);
// .rodata:0xE0 of this module: `.float`.
extern const float lbl_30_rodata_E0;

// .text 0xD2C0, 0x20 bytes. Vtable slot 11: `CPatterned::PreRenderAllViewports(mgr)` and nothing
// else.
void fn_30_D2C0(void* self, void* mgr) {
  PreRenderAllViewports__10CPatternedFR13CStateManager(self, mgr);
}

// .text 0xD250, 0x70 bytes. Vtable slot 8. The switch value is the word at +0xAE4; the float the
// two remaining cases test is at +0x11BC.
void fn_30_D250(void* self, const void* mgr) {
  switch (*reinterpret_cast< const int* >(static_cast< const char* >(self) + 0xAE4)) {
  case 2:
  case 4:
  case 6:
  case 7:
  case 8:
  case 9:
    AddToRenderer__10CPatternedCFRC13CStateManager(self, mgr);
    break;
  case 3:
  case 5:
    if (*reinterpret_cast< const float* >(static_cast< const char* >(self) + 0x11BC) <
        lbl_30_rodata_E0) {
      AddToRenderer__10CPatternedCFRC13CStateManager(self, mgr);
    }
    break;
  }
}

// .text 0xD230, 0x20 bytes. Vtable slot 14: the shared immune vulnerability record.
const void* fn_30_D230(void*) { return ImmuneVulnerability__20CDamageVulnerabilityFv(); }

#endif
}
