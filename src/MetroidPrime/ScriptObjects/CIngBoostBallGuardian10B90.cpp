// CIngBoostBallGuardian10B90.cpp - IngBoostBallGuardian's (module 30) 0x48-byte record copy,
// `.text` 0x10B90..0x10C24: one leaf function, 0x94 bytes.
//
//   0x10B90 fn_30_10B90 0x94  eighteen member copies, 9 floats, 9 words, no padding
//
// **It is the same idea as `fn_30_388C`, one range up the module's history, at twice the size:**
// eighteen loads and eighteen stores in retail's two-deep schedule and one `blr`, with no
// relocation in `.rela.text` anywhere in 0x10B90..0x10C24 (measured on
// `build/G2ME01/IngBoostBallGuardian/obj/auto_00_0000D2E0_text.o`), so the bytes are the whole of
// the claim and there is no callee to declare. `CIngBoostBallGuardian388C.cpp` spells that unit;
// this one only has more members, and the layout below is read off the same disassembly.
//
// **The layout is retail's and it is load-bearing.** Six words at +0x00..+0x14, nine floats at
// +0x18..+0x38, three words at +0x3C..+0x44 - eighteen four-byte members, 0x48 with no padding
// and no tail. Every load is `lwz`/`lfs` and every store `stw`/`stfs`, so nothing is packed: a
// narrower member would change the instruction the compiler picks and the byte count. The member
// names say what each word is *made of*, not what the record is called.
//
// **Who calls it, and where it sits.** `fn_30_10694` (0x10694, 0x240) is the whole-structure
// copy; it copies +0x00..+0xEF inline, then sets `r3 = r30+0xF0`, `r4 = r31+0xF0` and calls this
// function, then copies +0x138..+0x13F inline and calls `fn_30_108D4` at +0x140. So this record
// is a member of a 0x3FC-byte structure, not a structure the module loads, and it is reached
// through r3/r4 as a copy assignment on an lvalue rather than through a hidden return pointer -
// which is why the two parameters are spelled `self` and `other` and the body is one assignment.
//
// **mw_version is what produces retail's schedule.** Retail keeps two temporaries live and issues
// every load two instructions ahead of its store (`r5`/`r0` for the words, `f0`/`f1` for the
// floats). Under the module default `GC/1.3.2` the same whole-object assignment compiles to
// plain pairs - one live register, source order - and the bytes are not retail's; under `GC/2.7`
// they are (measured on this tree, the same experiment `CIngBoostBallGuardian388C.cpp` records).
// That is why the `Object(...)` entry in configure.py carries a per-object `mw_version="GC/2.7"`
// rather than the module-wide default, which would recompile the other module-30 units.
//
// **No dead-strip hazard, and that is measured.** `fn_30_10B90` is not in
// `build/G2ME01/IngBoostBallGuardian/ldscript.lcf`'s FORCEACTIVE list, but
// `powerpc-eabi-objdump -r` on both `auto_00_00000000_text.o` and `auto_00_0000D2E0_text.o` shows
// an `R_PPC_REL24` naming it (at 0x10898), so dtk's own object holds the reference and this
// unit's `.text` survives the link. No `force_active:` entry, no `config/G2ME01/config.yml`
// change and no `symbols.txt` rename: the split claims `.text` only.
//
// **The claim is 0x10B90..0x10C24 and not the 0x108D4..0x10C24 run.** `fn_30_108D4` in front of
// it is 0x2BC bytes of the same kind of copy, and `fn_30_10C24` behind it is a different
// function, so neither is claimed here.
//
// In `files.cmake` with an empty host branch, as `CIngBoostBallGuardianBits.cpp` explains.
// Definitions are in descending retail text order (one function).

extern "C" {

#ifdef __MWERKS__

// Retail's 0x48-byte record, member types read off the loads in fn_30_10B90.
struct RelRecord48 {
  int w00;
  int w04;
  int w08;
  int w0C;
  int w10;
  int w14;
  float f18;
  float f1C;
  float f20;
  float f24;
  float f28;
  float f2C;
  float f30;
  float f34;
  float f38;
  int w3C;
  int w40;
  int w44;
};

// .text 0x10B90, 0x94 bytes. `self` is the destination in r3, `other` the source in r4, and the
// body is the eighteen member copies in declaration order - which is what `GC/2.7` compiles to
// retail's two-deep schedule. The whole-object spelling `*self = other` is NOT usable here: at
// eighteen members `GC/2.7` stops inlining and emits a `bl` to an out-of-line
// `__as__11RelRecord48FRC11RelRecord48`, so the object would define two functions where retail
// defines one (measured; `tools/unit_fit.sh` would report the extra). At ten members, in
// `CIngBoostBallGuardian388C.cpp`, the same compiler still inlines it - the size is the
// difference, not the spelling.
void fn_30_10B90(RelRecord48* self, const RelRecord48& other) {
  self->w00 = other.w00;
  self->w04 = other.w04;
  self->w08 = other.w08;
  self->w0C = other.w0C;
  self->w10 = other.w10;
  self->w14 = other.w14;
  self->f18 = other.f18;
  self->f1C = other.f1C;
  self->f20 = other.f20;
  self->f24 = other.f24;
  self->f28 = other.f28;
  self->f2C = other.f2C;
  self->f30 = other.f30;
  self->f34 = other.f34;
  self->f38 = other.f38;
  self->w3C = other.w3C;
  self->w40 = other.w40;
  self->w44 = other.w44;
}

#endif
}