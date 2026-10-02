// CIngBoostBallGuardian13B7C.cpp - IngBoostBallGuardian's (module 30) touch-bounds neighbourhood,
// `.text` 0x13B7C..0x13BA4, 0x28 bytes, four contiguous functions of the module's second vtable
// (sec 5 +0xC44, the 36-slot one whose nearest DOL base is `CScriptDock`):
//
//   0x13B7C fn_30_13B7C 0xC   `lhz r0,0x4F6(r4)` / `sth r0,0(r3)`: a two-byte copy
//   0x13B88 fn_30_13B88 0x8   `addi r3,r3,0x570` / `blr`, vtable slot 15
//   0x13B90 fn_30_13B90 0x8   the same eight bytes, vtable slot 14
//   0x13B98 fn_30_13B98 0xC   one bit of the byte at +0x5D8, not a vtable slot
//
// The brief names slots 15 and 14 `GetDamageVulnerability(const CVector3f&, const CVector3f&,
// const CDamageInfo&)` and `GetDamageVulnerability()`; both return a `const
// CDamageVulnerability&`, and both bodies are the *same* address accessor, so the module's class
// answers both overloads with the record at +0x570. `fn_30_13B7C`'s argument order is measured, not
// guessed: the destination is in r3 and the object in r4, so the source takes the destination first
// - the shape a member function cannot have, which is why this one is declared as a free function
// over the object pointer. `fn_30_13B98`'s `& 4` is the module's documented one-bit-mask
// encoding: `CIngBoostBallGuardianBits.cpp` measured that `(b & M) != 0` compiles to a rotate of
// `32 - log2(M)`, so a source constant of 0x04 is what produces retail's `rlwinm r3,r0,30,31,31`
// (the bit it really tests is bit 1, not bit 2 - that is MWCC's behaviour on a zero-extended byte
// and it is what retail's bytes contain).
//
// Every one of the four is leaf: `build/G2ME01/IngBoostBallGuardian/obj/auto_00_00011CD8_text.o`'s
// `.rela.text` holds nothing in 0x13B7C..0x13BA4, so the bytes are the whole of the claim and
// there is no callee to declare.
//
// **No dead-strip hazard, and that is measured**: none of the four is in
// `build/G2ME01/IngBoostBallGuardian/ldscript.lcf`'s FORCEACTIVE list, but `fn_30_13B88`,
// `fn_30_13B90` are named by `auto_04_00000000_data.o` (the module's own vtable at `.data`+0xC44,
// slots 15 and 14 = +0x3C and +0x38) and `fn_30_13B7C` and `fn_30_13B98` are both named as
// undefined by `auto_00_000038E0_text.o`, which calls into them. No `force_active:` entry and no
// `config/G2ME01/config.yml` change.
//
// This file is in `files.cmake` with an empty host branch, exactly as
// `CIngBoostBallGuardianBits.cpp` explains.
//
// Definitions are in descending retail text order (mwcceppc emits definitions in reverse source
// order and mwldeppc keeps the object's `.text` order verbatim).

extern "C" {

#ifdef __MWERKS__

// .text 0x13B98, 0xC bytes. One bit of the byte at +0x5D8. See the note above on the mask encoding.
bool fn_30_13B98(const void* self) {
  return (static_cast< const unsigned char* >(self)[0x5D8] & 4) != 0;
}

// .text 0x13B90, 0x8 bytes. Vtable slot 14. The record at +0x570.
const void* fn_30_13B90(const void* self) { return static_cast< const char* >(self) + 0x570; }

// .text 0x13B88, 0x8 bytes. Vtable slot 15. The same record, and the same bytes.
const void* fn_30_13B88(const void* self) { return static_cast< const char* >(self) + 0x570; }

// .text 0x13B7C, 0xC bytes. The halfword at +0x4F6, copied to the destination in r3.
void fn_30_13B7C(unsigned short* out, const void* self) {
  *out = *reinterpret_cast< const unsigned short* >(static_cast< const char* >(self) + 0x4F6);
}

#endif
}
