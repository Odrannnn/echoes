// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the addresses
// and sizes come from `config/G2ME01/symbols.txt:9624-9626`, the instructions are the ones dtk
// itself emitted into `build/G2ME01/asm/auto_03_8021FABC_text.s:566-642`, and the bodies below
// are the C those bytes are the compilation of.
//
// .text 0x80220294..0x80220394, 0x100 = 256 bytes, 3 functions:
//
//   fn_80220294    0x80220294  0x54    21 instructions
//   fn_802202E8    0x802202E8  0x58    22 instructions
//   fn_80220340    0x80220340  0x54    21 instructions
//
// All three are MWCC deleting-destructor wrappers of one `CMayaSpline` member: `mr. r30,r3 / beq`
// guards the receiver, `li r4,-1` is the "do not free me afterwards" flag handed to the member's
// own destructor, and only a positive `flag` reaches `Free__7CMemoryFPCv`.  They differ only in
// which subobject they tear down:
//
//   fn_80220294  destroys a `CMayaSpline` at the receiver itself (+0).
//   fn_802202E8  destroys one at +4 (`addi r3,r30,0x4` at 0x80220308).
//   fn_80220340  is byte-identical to `fn_80220294` (also +0).
//
// Each is a byte-shape twin of an already-matched function of the same shape, which is why the
// bodies below are read rather than guessed: `fn_80220294` and `fn_80220340` are the 21
// instructions of `fn_800045A0` and `fn_802202E8` is the 22 of `fn_80004678`, both in the
// `Matching` unit `src/MetroidPrime/Carve800045A0.c` (0x800045A0 and 0x80004678).  Disassembled
// beside retail's range each pair differs only in the `bl` displacement - which is
// address-relative - and in the callee, which there is `fn_800045F4` / `fn_800046D0` and here is
// `__dt__11CMayaSplineFv`.  That file's `(char*)self + 4` call is this file's `fn_802202E8`; its
// +0 call is `fn_80220294` and `fn_80220340`.
//
// The caller in the same unclaimed range names the three members and fixes this range: in
// `fn_802201F8` (0x802201F8, 0x9C, asm lines 521-563) the parent destructor calls `fn_80220340`
// at 0x80220240 after `addi r3,r30,0xE4`, `fn_802202E8` at 0x8022024C after `addi r3,r30,0x94`
// and `fn_80220294` at 0x80220258 after `addi r3,r30,0x48`, each after `li r4,-1`.  The range
// stops at 0x80220394 because the function above it, `fn_80220394` (0x80220394, 0x64), is a
// different body - it tears down a `CMayaSpline` at +8 (0x802203B4) and then calls `fn_80241C90`
// on +4 (0x802203C0) - and the one below it, `fn_802201F8`, is the parent destructor itself.
//
// Retail names none of these.  `symbols.txt` carries the `fn_<addr>` placeholder and this file
// reproduces it verbatim, so the definitions have to stay C: a C++ one would mangle to
// `_Z<len>fn_<addr>v` and objdiff would pair nothing.  That is also why the unit is a `.c`.
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object `.text` verbatim, so an
// ascending file is a permuted `.text` - 100.00% per function and a broken DOL.  Only
// `tools/flip_test.sh` catches that.
//
// Its own unit because neither neighbour belongs to this chain and a unit may not claim two
// discontiguous ranges in one section (dtk `dol split` fails with "Cyclic dependency ... link
// order").
//
// The directory is the nearest claimed range's: the claim immediately below is
// `MetroidPrime/ScriptLoader/CannonBallLoaderSet.cpp` (0x8021FAB4..0x8021FABC) and the one above
// is `MetroidPrime/CQuitGameScreen.cpp` (0x802219EC..0x802221C0), so this address is in the
// ScriptLoader neighbourhood.

/** 0x802CE388, `symbols.txt:12992`, size 0x64: `CMemory::Free(void const*)`.  Claimed by
 *  `Kyoto/Alloc/CMemory.cpp` (`.text` 0x802CE224..0x802CE72C), so our own tree supplies it in both
 *  builds.  Declared, never defined here. */
extern void Free__7CMemoryFPCv(const void* ptr);

/** 0x800327FC, `symbols.txt:963`, size 0x58: `CMayaSpline::~CMayaSpline()`, the member each
 *  wrapper below tears down.  Not claimed by any unit of ours - it is inside dtk's
 *  `auto_03_80032674_text.o` (asm line 128, which itself calls `fn_80032854` and
 *  `Free__7CMemoryFPCv`), so the matching build takes it from that object, exactly as it does for
 *  the twin unit's `fn_800045F4`/`fn_800046D0`.  Declared, never defined here for the matching
 *  build; the port-only stand-in at the end of this file is what the host link needs.
 *  `grep -rn __dt__11CMayaSplineFv src/ include/` finds no other declaration or definition. */
extern void __dt__11CMayaSplineFv(void* self, short flag);

void* fn_80220340(void* self, short flag) {
  if (self) {
    __dt__11CMayaSplineFv(self, -1);
    if (flag > 0) {
      Free__7CMemoryFPCv(self);
    }
  }
  return self;
}

void* fn_802202E8(void* self, short flag) {
  if (self) {
    __dt__11CMayaSplineFv((char*)self + 4, -1);
    if (flag > 0) {
      Free__7CMemoryFPCv(self);
    }
  }
  return self;
}

void* fn_80220294(void* self, short flag) {
  if (self) {
    __dt__11CMayaSplineFv(self, -1);
    if (flag > 0) {
      Free__7CMemoryFPCv(self);
    }
  }
  return self;
}

#ifdef TARGET_PC
// Port-only stand-in, and it is **not** a claim that `__dt__11CMayaSplineFv` is decompiled - it is
// not.  The matching build does not compile this block (PORT_NOTES.md, "TARGET_PC, and the rule
// for port edits"), so main.dol still takes the real 0x800327FC from dtk's auto object above and
// the three functions keep their retail bytes.
//
// The host's flat link carries our sources only, not dtk's objects, so without this the port's
// link loses one symbol: measured with `./tools/probe_sources.sh`, which compiles every file in
// `files.cmake` with `-DTARGET_PC` (its COMMON, line 49) - without this block it prints
// `link: NOT LINKED` and the strict check names `NEW __dt__11CMayaSplineFv`; with it, `LINKED`
// and no growth.  Nothing in the port reaches the three wrappers above (the only caller in retail
// is `fn_802201F8`, which no unit of ours claims and no table names), so this stand-in cannot
// change what the game does.
//
// Empty body on purpose: a stand-in that returns something plausible is worse than one that
// announces itself.  `include/Kyoto/Math/CMayaSpline.hpp` is not including it in the port link -
// `src/Kyoto/Math/CMayaSpline.cpp` defines that class's other members and not its destructor.
void __dt__11CMayaSplineFv(void* self, short flag) {}
#endif
