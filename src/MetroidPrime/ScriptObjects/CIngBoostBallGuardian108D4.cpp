// CIngBoostBallGuardian108D4.cpp - IngBoostBallGuardian's (module 30) 0x15C-byte record copy,
// `.text` 0x108D4..0x10B90: one leaf function, 0x2BC bytes.
//
//   0x108D4 fn_30_108D4 0x2BC  twenty-nine triple assignments, 29 float/int/unsigned char
//
// **It is the same idea as `fn_30_10B90` (the unit behind it in the module) at fifteen times the
// size**: 87 loads and 87 stores in retail's schedule and one `blr`, with no relocation in
// `.rela.text` anywhere in 0x108D4..0x10B90 (measured on
// `build/G2ME01/IngBoostBallGuardian/obj/auto_00_0000D2E0_text.o` at object offset 0x35F4), so the
// bytes are the whole of the claim and there is no callee to declare.
//
// **The layout is retail's and it is load-bearing.** The per-member load type repeats
// `lfs, lwz, lbz` exactly 29 times at offsets 0x00/0x04/0x08, 0x0C/0x10/0x14, ... 0x150/0x154/0x158
// - 87 four-byte-or-narrow members, 0x15C bytes, no tail and no gaps. The third member of every
// triple is what fixes the shape: it is `lbz`/`stb`, so it is one byte wide, and the next triple's
// `float` is still 4-aligned, which is why the stride is 12 and not 9. So the record is
// **29 elements of one 12-byte (float, int, unsigned char) triple**, not 87 loose members - and
// saying so is not cosmetic, see the note on the body below.
//
// **Who calls it, and where it sits.** `fn_30_10694` (0x10694, 0x240) is the whole-structure copy;
// it copies +0x00..+0xEF inline, sets `r3 = r30+0xF0` / `r4 = r31+0xF0` and calls `fn_30_10B90`,
// copies +0x138..+0x13F inline, then sets `r3 = r30+0x140` / `r4 = r31+0x140` and calls this
// function (measured on the relocations of `auto_00_0000D2E0_text.o`: `R_PPC_REL24 fn_30_10B90` at
// object offset 0x35B8 and `R_PPC_REL24 fn_30_108D4` at 0x35D4). So this record is a member of a
// 0x3FC-byte structure and it is reached through r3/r4 as a copy assignment on an lvalue, which is
// why the parameters are spelled `self`/`other` rather than through a hidden return pointer.
//
// **The body is one assignment per triple, and the 87-member spelling does not compile to retail's
// bytes.** Spelled member-by-member (`self->t[i].f = other.t[i].f;` and so on, 87 lines) the same
// struct compiles to 700 bytes and 175 instructions in which **five are wrong**: at member 43's
// word the allocator puts the value in `r0` instead of `r5` and then reorders the next two
// instructions around it (`lwz r0,0xac(r4)` / `stw r0,0xac(r3)` where retail has `lwz r5,0xac(r4)`
// / `lbz r0,0xb0(r4)` / `stw r5,0xac(r3)`), and at member 48's byte the two stores swap. That is
// 13 bytes of 700 - objdiff would still call it a near-perfect fuzzy match and `unit_fit.sh` would
// still be happy, and it is not retail's code. Assigning each **triple** instead
// (`self->t[i] = other.t[i];`, 29 lines) gives the same 700 bytes and is byte-identical to retail,
// checked instruction by instruction. The difference is that the three-member implicit assignment
// operator is inlined per element, which is what fixes the register choice; the flat spelling has
// 87 independent scalar copies for the allocator to interleave.
//
// The whole-object spelling `*self = other` is not usable either: at 87 members the compiler emits
// an out-of-line `__as__11RelRecord15CFRC11RelRecord15C` (measured - 732 bytes in the object, two
// functions), which `tools/unit_fit.sh` reports as an extra function and which makes the unit
// un-promotable however good it looks. At 18 members (`fn_30_10B90`, above) the same compiler still
// inlines a whole-object assignment, and there the flat eighteen-member spelling is byte-identical;
// the size is the difference there, and here it is the nesting.
//
// **mw_version is load-bearing**, for the same reason and with the same per-object override as
// `fn_30_10B90`: at the module default GC/1.3.2 this source gives plain load/store pairs - one live
// register, source order - and **512 of its 700 bytes are wrong**. At GC/2.7 it is byte-identical
// (GC/2.0, 2.0p1, 2.5 and 2.6 agree; GC/3.0a5 gives 844 bytes). Setting the version on the
// `Rel(...)` block instead would recompile the other module-30 units.
//
// **No dead-strip hazard, and that is measured.** `fn_30_108D4` is not in
// `build/G2ME01/IngBoostBallGuardian/ldscript.lcf`'s FORCEACTIVE list, but
// `powerpc-eabi-objdump -r` on `auto_00_0000D2E0_text.o` shows an `R_PPC_REL24` naming it (at
// 0x35D4), so dtk's own object holds the reference and this unit's `.text` survives the link. No
// `force_active:` entry, no `config/G2ME01/config.yml` change and no `symbols.txt` rename: the
// split claims `.text` only, and keeping the `fn_30_*` name matters because dtk's objects name it
// that way.
//
// **The claim is 0x108D4..0x10B90 and not the 0x10694..0x10B90 run.** `fn_30_10694` in front of it
// is 0x240 bytes and calls both `fn_30_10B90` and this function, so it is only claimable once this
// one is a unit of its own; `fn_30_10B90` behind it is already its own unit
// (`CIngBoostBallGuardian10B90.cpp`). The carve splits dtk's `auto_00_0000D2E0_text.o` at 0x108D4,
// and `total_functions` stays 28465 because retail's function boundaries are unchanged.
//
// The member names say what each word is *made of*, not what the record is called; a copy
// assignment says what it is made of and nothing else, and these 700 bytes occur nowhere else in
// the DOL to anchor a name.
//
// In `files.cmake` with an empty host branch, as `CIngBoostBallGuardianBits.cpp` explains.
// Definitions are in descending retail text order (one function).

extern "C" {

#ifdef __MWERKS__

// Retail's 0x15C-byte record: 29 copies of this 12-byte triple at offsets 0x00 .. 0x158.
// The member types are read off the loads in fn_30_108D4 - lfs / lwz / lbz, once per triple.
struct RelTriple {
  float f;
  int w;
  unsigned char b;
};

struct RelRecord15C {
  RelTriple t[29];
};

// .text 0x108D4, 0x2BC bytes. `self` is the destination in r3, `other` the source in r4, and the
// body is the twenty-nine element assignments in declaration order - which is what `GC/2.7`
// compiles to retail's schedule: each load issued two instructions ahead of its store, with one
// live float register (`f0`), one live word register (`r5`) and one live byte register (`r0`).
// The header comment above is load-bearing on why it is per triple and not per member.
void fn_30_108D4(RelRecord15C* self, const RelRecord15C& other) {
  self->t[0] = other.t[0];
  self->t[1] = other.t[1];
  self->t[2] = other.t[2];
  self->t[3] = other.t[3];
  self->t[4] = other.t[4];
  self->t[5] = other.t[5];
  self->t[6] = other.t[6];
  self->t[7] = other.t[7];
  self->t[8] = other.t[8];
  self->t[9] = other.t[9];
  self->t[10] = other.t[10];
  self->t[11] = other.t[11];
  self->t[12] = other.t[12];
  self->t[13] = other.t[13];
  self->t[14] = other.t[14];
  self->t[15] = other.t[15];
  self->t[16] = other.t[16];
  self->t[17] = other.t[17];
  self->t[18] = other.t[18];
  self->t[19] = other.t[19];
  self->t[20] = other.t[20];
  self->t[21] = other.t[21];
  self->t[22] = other.t[22];
  self->t[23] = other.t[23];
  self->t[24] = other.t[24];
  self->t[25] = other.t[25];
  self->t[26] = other.t[26];
  self->t[27] = other.t[27];
  self->t[28] = other.t[28];
}

#endif
}
