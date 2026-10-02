// CIngBoostBallGuardianBits.cpp - IngBoostBallGuardian's (module 30) flag-byte accessor run,
// `.text` 0xB788..0xB7E0: five functions, 0x58 bytes, all of which read one bit out of the two
// flag bytes at +0x11ED and +0x11EE and nothing else.
//
// **The unit is leaf: `build/G2ME01/IngBoostBallGuardian/obj/auto_00_00000130_text.o` carries no
// relocation at all in 0xB788..0xB7E0**, which is measured, not assumed - every one of these five
// bodies is pure load/rotate/branch, so the bytes are the whole of the claim and there is no callee
// to name. That is also why they can be five functions in one unit: they are contiguous in retail
// (`symbols.txt`: 0xB788 0xC, 0xB794 0xC, 0xB7A0 0xC, 0xB7AC 0x28, 0xB7D4 0xC) and a claim may
// not span an unclaimed gap.
//
//   0xB788 fn_30_B788 0xC  `clrlwi r3, r0, 31` on the byte at +0x11EE
//   0xB794 fn_30_B794 0xC  `rlwinm r3, r0, 28, 31, 31` on the byte at +0x11EE
//   0xB7A0 fn_30_B7A0 0xC  `rlwinm r3, r0, 25, 31, 31` on the byte at +0x11EF
//   0xB7AC fn_30_B7AC 0x28 two one-bit reads, one per byte, ANDed
//   0xB7D4 fn_30_B7D4 0xC  `rlwinm r3, r0, 27, 31, 31` on the byte at +0x11EE
//
// **The mask encoding is measured, not read off the disassembly, and it is not the obvious one.**
// Compiling `(static_cast<const unsigned char*>(p)[k] & M) != 0` with this tree's MWCC gives
// (measured on a scratch file; GC/1.3.2 and GC/2.7 byte-identical here):
//
//   M = 0x01 -> clrlwi rD, r0, 31          M = 0x10 -> rlwinm rD, r0, 28, 31, 31
//   M = 0x02 -> rlwinm rD, r0, 31, 31, 31  M = 0x20 -> rlwinm rD, r0, 27, 31, 31
//   M = 0x04 -> rlwinm rD, r0, 30, 31, 31  M = 0x40 -> rlwinm rD, r0, 26, 31, 31
//   M = 0x08 -> rlwinm rD, r0, 29, 31, 31  M = 0x80 -> rlwinm rD, r0, 25, 31, 31
//
// so the rotate is always `32 - log2(M)`, and **the bit such a predicate really tests is bit k-1,
// not bit k**; at M = 0x01 the rotate is 0 and nothing of the byte survives, so the function is
// constant. That is MWCC's own behaviour on a zero-extended byte, it is what retail's bytes
// contain, and it is the only reason the source constants below are 0x01/0x10/0x80/0x20 rather
// than the bit each function appears to read. `CIngBoostBallGuardianRel.cpp`'s `fn_30_4C`
// (`lbz r3,0x44f(r3)` + `extrwi r3,r0,1,28`) is the same family at M = 0x08.
//
// **`fn_30_B7AC` needs the shift spelling, not the mask spelling, and both were measured.**
// `(b[0x11EE] & 8) == 0 && (b[0x11ED] & 2) == 0` - the obvious reading of `beq` then `bne` - gives
// `rlwinm. r0, r0, 0, 28, 28` and `rlwinm. r0, r0, 0, 30, 30`, four bytes off retail. Retail holds
// `rlwinm. r0, r0, 29, 31, 31` and `rlwinm. r0, r0, 31, 31, 31`, which is the **`(b >> k) & 1`
// spelling** - what MWCC emits for a one-bit *bitfield* read - with the two `&&` operands negated
// the way the branches say. `(b1 >> 3) & 1` and `!((b2 >> 1) & 1)` reproduce all 40 bytes. The
// control flow is what it is either way: `li r4,0` hoisted above the first test, `beq` leaving
// with r4 still 0, `bne` on the second, `li r4,1` at the join.
//
// **No dead-strip hazard, and that is measured.** None of the five is in
// `build/G2ME01/IngBoostBallGuardian/ldscript.lcf`'s FORCEACTIVE list, but all five are named by
// `auto_04_00000000_data.o` - the module's own vtable stores them at `.data`+0x210, +0x21C, +0x228,
// +0x234 and +0x240 - so dtk's object holds the reference and this unit's `.text` survives the link.
// No `force_active:` entry and no `config/G2ME01/config.yml` change is needed.
//
// This file is listed in `files.cmake`, and **its host branch defines nothing at all** - the same
// arrangement `CLumiteRelTail.cpp` uses, and the reason `tools/check_files_cmake.py` requires it: a
// `Matching` object must be either in `files.cmake` or in that tool's judge-owned EXCLUDED set, and
// the only automatic exemption is a unit that defines RELMain/RELExit. An empty host branch keeps
// the port's undefined count where it is while the MWCC branch is the retail source.
//
// Definitions are in descending retail text order (mwcceppc emits definitions in reverse source
// order and mwldeppc keeps the object's `.text` order verbatim, so ascending would permute the
// module's bytes with objdiff still at 100%). `python3 tools/check_decl_order.py --unit
// MetroidPrime/ScriptObjects/CIngBoostBallGuardianBits.cpp`.

extern "C" {

#ifdef __MWERKS__

// .text 0xB7D4, 0xC bytes. One bit out of the flag byte at +0x11EE.
bool fn_30_B7D4(const void* self) {
  return (static_cast< const unsigned char* >(self)[0x11EE] & 0x20) != 0;
}

// .text 0xB7AC, 0x28 bytes. The compound predicate: bit 3 of the byte at +0x11EE must be set and
// bit 1 of the byte at +0x11ED must be clear. See the note above on the shift spelling.
bool fn_30_B7AC(const void* self) {
  const unsigned char* flags = static_cast< const unsigned char* >(self);
  return ((flags[0x11EE] >> 3) & 1) && !((flags[0x11ED] >> 1) & 1);
}

// .text 0xB7A0, 0xC bytes. One bit out of the flag byte at +0x11EF.
bool fn_30_B7A0(const void* self) {
  return (static_cast< const unsigned char* >(self)[0x11EF] & 0x80) != 0;
}

// .text 0xB794, 0xC bytes. One bit out of the flag byte at +0x11EE.
bool fn_30_B794(const void* self) {
  return (static_cast< const unsigned char* >(self)[0x11EE] & 0x10) != 0;
}

// .text 0xB788, 0xC bytes. One bit out of the flag byte at +0x11EE.
bool fn_30_B788(const void* self) {
  return (static_cast< const unsigned char* >(self)[0x11EE] & 1) != 0;
}

#endif
}