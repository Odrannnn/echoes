// CIngBoostBallGuardian13C40.cpp - IngBoostBallGuardian's (module 30) health-info pair, `.text`
// 0x13C40..0x13C78, 0x38 bytes, two contiguous functions of the module's second vtable (sec 5
// +0xC44, the 36-slot one whose nearest DOL base is `CScriptDock`):
//
//   0x13C40 fn_30_13C40 0x1C  slot 13, `const CHealthInfo* GetHealthInfo() const`
//   0x13C5C fn_30_13C5C 0x1C  slot 12, `CHealthInfo* HealthInfo()`
//
// The brief reads both names off the DOL vtable's matching slots. The two bodies are byte-identical
// (`lbz r0,0x5C0(r3)` / `cmplwi r0,0` / `beq` / `addi r3,r3,0x5A0` / `blr` / `li r3,0` / `blr`), so
// the class hands out the `CHealthInfo` at +0x5A0 while the byte at +0x5C0 is set and null
// otherwise, exactly as a const/non-const pair returning the same member does. This is the shape
// `CActor::GetHealthInfo()` has in `include/MetroidPrime/CActor.hpp` (which delegates to
// `HealthInfo()`), written out because the module's class is not modelled here.
//
// Both are leaf: `build/G2ME01/IngBoostBallGuardian/obj/auto_00_00011CD8_text.o`'s `.rela.text`
// holds nothing in 0x13C40..0x13C78, so the bytes are the whole of the claim.
//
// **No dead-strip hazard, and that is measured**: neither is in
// `build/G2ME01/IngBoostBallGuardian/ldscript.lcf`'s FORCEACTIVE list, but both are named by
// `auto_04_00000000_data.o` (the module's own vtable at `.data`+0xC44 stores `fn_30_13C5C` at
// +0x38... slot 12 and `fn_30_13C40` at slot 13), so dtk's object holds the references. No
// `force_active:` entry and no `config/G2ME01/config.yml` change.
//
// This file is in `files.cmake` with an empty host branch, exactly as
// `CIngBoostBallGuardianBits.cpp` explains.
//
// Definitions are in descending retail text order (mwcceppc emits definitions in reverse source
// order and mwldeppc keeps the object's `.text` order verbatim).

extern "C" {

#ifdef __MWERKS__

// .text 0x13C5C, 0x1C bytes. Vtable slot 12, the non-const accessor.
const void* fn_30_13C5C(const void* self) {
  if (static_cast< const unsigned char* >(self)[0x5C0] != 0) {
    return static_cast< const char* >(self) + 0x5A0;
  }
  return 0;
}

// .text 0x13C40, 0x1C bytes. Vtable slot 13, the const accessor.
const void* fn_30_13C40(const void* self) {
  if (static_cast< const unsigned char* >(self)[0x5C0] != 0) {
    return static_cast< const char* >(self) + 0x5A0;
  }
  return 0;
}

#endif
}
