// Carved out of the unclaimed dtk `auto_03_80335A44_text` range.  Every number here is
// measured: the address and the size come from `config/G2ME01/symbols.txt:15077`, the
// instructions are the ones dtk itself emitted into
// `build/G2ME01/asm/auto_03_80335A44_text.s`, and the body below is the C those bytes are
// the compilation of.
//
// .text 0x80335A44..0x80335A5C, 0x18 = 24 bytes, 1 function:
//
//   fn_80335A44    0x80335A44  0x18   lwz      r0, 0x44(r3)
//                                       li       r3, 0x0
//                                       rlwinm.  r0, r0, 0, 23, 23
//                                       beqlr
//                                       li       r3, 0x2
//                                       blr
//
// So: read the word at +0x44 and return 2 if a flag bit is set, 0 if it is not.
//
// **The bit is 8, not 23.**  `rlwinm`'s MB/ME fields number the mask from the *most*
// significant bit (MB = 0 is the sign bit), so `MB = ME = 23` keeps bit 31-23 = 8, i.e.
// the mask is 0x100.  This is not a subtlety to read out of the asm: every spelling tried
// with `0x800000` compiles to some *other* rotate mask - measured, `w & 0x800000u` gives
// `rlwinm. r0,r0,0,8,8` and `w & 0x100u` gives retail's `rlwinm. r0,r0,0,23,23` - and the
// repo's own Matching units agree: `CActor.cpp`'s `sfxId & 0x20000` (bit 17) is
// `rlwinm. r0, r4, 0, 14, 14` (`build/G2ME01/asm/MetroidPrime/CActor.s:1081`), and 31-17 = 14.
//
// **The `int r` is load-bearing too.**  `? 2 : 0`, `if (…) return 2; return 0;` and
// `if (…) return 2; else return 0;` all compile to MWCC's branchless select -
// `li r0,2 / rlwinm r3,r3,9,31,31 / neg r3,r3 / and r3,r0,r3 / blr`, 5 instructions and
// the wrong bytes.  Assigning through a `0`-initialised local is what makes it keep the
// `beqlr`.  Measured, all four spellings at `-O4,p`.
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits
// function definitions in *reverse* source order and mwldeppc keeps the object `.text`
// verbatim, so an ascending file is a permuted `.text` - 100.00% per function and a broken
// DOL.  Only `tools/flip_test.sh` catches that.
//
// Retail names none of this.  `symbols.txt` carries the `fn_<addr>` placeholder and this
// file reproduces that symbol verbatim, so the definition has to stay C: a C++ one would
// mangle to `_Z<len>fn_80335A44Pv` and objdiff would pair nothing.  That is also why the
// unit is a `.c` rather than a `.cpp`, as with the neighbouring carves of this address run
// (`Kyoto/Math/Carve80335A3C.c` at 0x80335A3C, `Kyoto/Math/Carve80335A5C.c` at 0x80335A5C).
//
// Its own unit because a unit may not claim two discontiguous ranges in one section
// (dtk `dol split` fails with "Cyclic dependency ... link order"), and because the
// functions on either side of this gap are not trivial.
//
// Nothing branches to it - no `bl`, `b` or `ba` in `.text` reaches 0x80335A44, measured -
// and it is in no FORCEACTIVE entry, but four raw function-pointer tables in `.data` do
// name it at the same slot (each holds ... 0x80335A34, 0x80335A3C, 0x80335A44,
// 0x803385D0 ... at one offset; file offsets 0x3B85BC, 0x3B8974, 0x3B8C8C, 0x3B8EBC).
// All of those are dtk auto units - raw bytes with no relocations - so the reference does
// not keep the symbol alive, which is why this carve also adds the top-level
// `force_active:` entry to `config/G2ME01/config.yml`: see the comment there, and
// `docs/goal-notes/carve-80218c64.md` for the same trap measured on `fn_80218C6C`.
//
// The directory is `MetroidPrime/` and not beside its neighbours in `Kyoto/Math/` because
// that is the unit name this carve is queued and judged under; the code belongs to the
// same anonymous address run either way and nothing here depends on the directory.
int fn_80335A44(void* self) {
  unsigned int w = *(unsigned int*)((unsigned char*)self + 0x44);
  int r = 0;
  if (w & 0x100u) {
    r = 2;
  }
  return r;
}
