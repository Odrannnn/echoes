// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the address and
// the size come from `config/G2ME01/symbols.txt:9627`, and the 25 instructions below are the ones
// dtk emitted into `build/G2ME01/asm/auto_03_80220394_text.s:9-36` (the `auto` unit this range
// lives in before the claim), read off one by one rather than guessed.
//
// .text 0x80220394..0x802203F8, 0x64 = 100 bytes, 1 function:
//
//   fn_80220394    0x80220394  0x64   25 instructions
//
// **What it is: the destructor of the 0x50-byte member at +0x130 of the ScriptLoader
// editor-properties class.**  Its caller is `fn_802201F8` (0x802201F8, 0x9C, the class's own
// deleting destructor, `build/G2ME01/asm/auto_03_8021FABC_text.s:521-563`), which calls it at
// 0x80220234 as `addi r3,r30,0x130 / li r4,-1` before the three siblings at +0xE4, +0x94 and
// +0x48.  So the receiver is that +0x130 member and the two callees tear down the two subobjects
// inside it:
//
//   `__dt__11CMayaSplineFv(self + 8, -1)`   `addi r3,r30,0x8` at 0x802203B4
//   `fn_80241C90(self + 4, -1)`             `addi r3,r30,0x4` at 0x802203C0
//
// Those offsets are the constructor's too, which is what makes this the same member: the class
// constructor `fn_802203F8` (0x802203F8, 0xF0) stores the byte at +0 (`stb r0,0x130(r31)` at
// 0x8022048C), builds a `fn_80241CCC` object at +4 (0x80220478), constructs a `CMayaSpline` at +8
// (0x80220480) and writes the float at +0x4C (0x80220494); the next member starts at +0x180
// (0x80220490), which is what fixes the member's size at 0x50.  The body itself is the MWCC
// deleting-destructor shape `src/MetroidPrime/Carve800045A0.c` documents and the sibling
// `src/MetroidPrime/ScriptLoader/Carve80220294.c` reproduces three times: receiver guard, `li r4,-1`
// to each member destructor, `Free__7CMemoryFPCv` only for a positive flag, receiver returned in r3.
//
// Retail names none of this: `symbols.txt` carries the `fn_<addr>` placeholder and this file
// reproduces that symbol verbatim, so the definition has to stay C - a C++ one would mangle to
// `_Z<len>fn_<addr>v` and objdiff would pair nothing.  That is also why the unit is a `.c`.
//
// Its own unit because neither neighbour belongs to this chain and a unit may not claim two
// discontiguous ranges in one section (dtk `dol split` fails with "Cyclic dependency ... link
// order"): the claim immediately below is `MetroidPrime/ScriptLoader/Carve80220294.c`
// (0x80220294..0x80220394, whose own header names this function as the body that stops its range)
// and the one above is `MetroidPrime/CQuitGameScreen.cpp` (0x802219EC..0x802221C0), so this range
// is exactly the gap between a claimed unit and the next claimed one.
//
// One definition, so "descending by address" is trivially satisfied here; the rule matters for any
// function added above it later, because mwcceppc emits definitions in *reverse* source order.

/** 0x800327FC, `symbols.txt:963`, size 0x58: `CMayaSpline::~CMayaSpline()`, the member at +8 of
 *  this receiver.  Not claimed by any unit of ours - dtk's own `auto_03_80032674_text.o` defines
 *  it and the matching build takes it from there, exactly as the sibling unit's `fn_80220294`
 *  does.  Declared, never defined here. */
extern void __dt__11CMayaSplineFv(void* self, short flag);

/** 0x80241C90, `symbols.txt:10220`, size 0x3C: the deleting destructor of the object at +4 of this
 *  receiver - its constructor is `fn_80241CCC` (0x80241CCC, 0xC: `li r0,0 / stw r0,0(r3) / blr`).
 *  Defined by dtk's `auto_03_802399F4_text.o` (`build/G2ME01/asm/auto_03_802399F4_text.s`, its
 *  `.fn` is `fn_80241C90`: receiver guard, `extsh.` on the flag, `Free__7CMemoryFPCv`, receiver
 *  returned), so the matching build takes it from there too.  Declared, never defined here for the
 *  matching build; the port-only stand-in at the end of this file is what the host link needs. */
extern void* fn_80241C90(void* self, short flag);

/** 0x802CE388, `symbols.txt:12992`, size 0x64: `CMemory::Free(void const*)`.  Claimed by
 *  `Kyoto/Alloc/CMemory.cpp`, so our own tree defines it in both builds.  Declared, never defined
 *  here. */
extern void Free__7CMemoryFPCv(const void* ptr);

void* fn_80220394(void* self, short flag) {
  if (self) {
    __dt__11CMayaSplineFv((char*)self + 8, -1);
    fn_80241C90((char*)self + 4, -1);
    if (flag > 0) {
      Free__7CMemoryFPCv(self);
    }
  }
  return self;
}

#ifdef TARGET_PC
// Port-only stand-in for the one symbol in this file that nothing else under `src/` defines.  The
// matching build does not compile this block (`PORT_NOTES.md`, "TARGET_PC, and the rule for port
// edits"), so `main.dol` still takes the real 0x80241C90 from dtk's auto object above and
// `fn_80220394` keeps its retail bytes.
//
// The host's flat link carries our sources only, not dtk's objects, so without a definition the
// link loses `fn_80241C90`: measured with `./tools/probe_sources.sh` (which compiles every file in
// `files.cmake` with `-DTARGET_PC`), without this block the strict check names
// `NEW fn_80241C90` and with it the undefined count does not grow.  The other callee needs
// nothing here: `src/MetroidPrime/ScriptLoader/Carve80220294.c` already defines
// `__dt__11CMayaSplineFv` under `TARGET_PC` for its own three wrappers, and a second definition
// would be a duplicate (`link_check: duplicate definitions`).
//
// Empty body on purpose: a stand-in that returns something plausible is worse than one that
// announces itself.  Nothing in the port calls `fn_80220394` (the only caller in retail is
// `fn_802201F8`, which no unit of ours claims and no table names), so this cannot change what the
// game does.
void* fn_80241C90(void* self, short flag) {
  (void)self;
  (void)flag;
  return self;
}
#endif
