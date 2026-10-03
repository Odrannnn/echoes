// Carved out of an unclaimed dtk `auto_*` range by lane `carve2` (goal item `carve-801708c4`).
// Every number here is measured: the address and size come from
// `config/G2ME01/symbols.txt:6057`, the instructions are the ones dtk itself emitted into
// `build/G2ME01/asm/auto_03_8016FD94_text.s:816-826` before the claim existed (the same range is
// now `build/G2ME01/asm/MetroidPrime/Carve801708C4.s`, and the bytes were re-read this run from the
// disc with `tools/dol_read.py 0x801708C4 0x20`).
//
// .text 0x801708C4..0x801708E4, 0x20 = 32 bytes, 1 function:
//
//   fn_801708C4    0x801708C4  0x20    8 instructions   vtable slot 8, a pure `CActor::AddToRenderer`
//
//   /* 801708C4 0016D6C4  94 21 FF F0 */  stwu r1, -0x10(r1)
//   /* 801708C8 0016D6C8  7C 08 02 A6 */  mflr r0
//   /* 801708CC 0016D6CC  90 01 00 14 */  stw r0, 0x14(r1)
//   /* 801708D0 0016D6D0  4B ED C2 AD */  bl AddToRenderer__6CActorCFRC13CStateManager
//   /* 801708D4 0016D6D4  80 01 00 14 */  lwz r0, 0x14(r1)
//   /* 801708D8 0016D6D8  7C 08 03 A6 */  mtlr r0
//   /* 801708DC 0016D6DC  38 21 00 10 */  addi r1, r1, 0x10
//   /* 801708E0 0016D6E0  4E 80 00 20 */  blr
//
// **It is vtable slot 8, and the slot number is measured three ways rather than guessed.**  The
// only references to the symbol in retail are data, not code: `grep -rn fn_801708C4
// build/G2ME01/asm/` finds exactly two, both `.4byte` entries in dtk's unclaimed data unit
// `build/G2ME01/asm/auto_07_803B5468_data.s` - `lbl_803B5500` at line 72 (`+0x28`) and
// `auto_07_803B5450_data.s` at line 72, the same word reached from the other side of that unit's
// split - and no `bl` anywhere.  Reading `lbl_803B5500`'s 32 words out with their offsets puts this
// one at `+0x28`, and `+0x08` is slot 0, so the slot is `(0x28 - 0x08) / 4 = 8`.  Slot 8 is
// `AddToRenderer`, from `include/MetroidPrime/CActor.hpp:79-83`, where `CActor`'s overrides are
// declared `ClearFluidList` (6), `PreRender` (7), `AddToRenderer` (8), `Render` (9),
// `CanRenderUnsorted` (10), `PreRenderAllViewports` (11), `HealthInfo` (12),
// `GetHealthInfo` (13), `GetDamageVulnerability` (14) ... - and the vtable confirms that numbering
// word for word: `lbl_803B5500`'s slots 6, 10, 11, 12, 13, 14 are retail's own
// `ClearFluidList__6CActorFR13CStateManager`, `CanRenderUnsorted__6CActorCFRC13CStateManager`,
// `PreRenderAllViewports__6CActorFR13CStateManager`, `HealthInfo__6CActorFv`,
// `GetHealthInfo__6CActorCFv` and `GetDamageVulnerability__6CActorCFv`.  The three neighbours are
// the same identification: slot 7 holds `fn_801708E4`, whose body calls
// `PreRender__6CActorFR13CStateManager` first, and slot 9 holds `fn_80170958`, whose body is
// `lbz r0,0x200(r3)` / `extrwi. r0,r0,1,25` / `beq` / `bl Render__6CActorCFRC13CStateManager`
// (`auto_03_801708E4_text.s:15` and `:49`, which is where the claim above this one moved them).
// So the class overrides `PreRender`, `AddToRenderer` and
// `Render`, and this one is the middle of the three.
//
// **Why the override exists is retail's bytes, not a guess:** a frame and one unconditional
// `bl`, with both argument registers forwarded untouched and no return value, is a pure
// forwarder - this override adds nothing over the base, and writing the call is all that is here.
// It is the twin of `fn_80004438` (0x80004438, 0x20, `src/MetroidPrime/Carve80004438.c`, a
// `Matching` unit, `void fn_80004438(void* self) { fn_80004458(self); }`) - the same eight
// instructions with a different `bl` target and one more argument register passed straight through,
// which changes no instruction at all because a `bl` forwards `r3` and `r4` either way.
// `__sys_free` (0x80008A28, `src/MetroidPrime/main.cpp`) is the same eight instructions a third
// time.
//
// **The callee is retail's base `CActor::AddToRenderer` and it is declared, not claimed.**
// `powerpc-eabi-objdump -d build/G2ME01/main.elf` at 0x801708C4 resolves the `bl` to
// `AddToRenderer__6CActorCFRC13CStateManager` at 0x8004CB7C (`symbols.txt:1485`, 0x184 = 388 bytes,
// ending at 0x8004CD00, where `ShouldDrawShadow__6CActorCFRC13CStateManager` begins).  Those bytes
// are inside `MetroidPrime/CActor.cpp`'s existing claim (0x80049ED8..0x8004E84C,
// `config/G2ME01/splits.txt:231-232`), a `NonMatching` unit, so dtk's own object supplies them in
// the DOL
// link and this carve cannot and should not claim them.  The signature is retail's mangled
// `(CActor*, CStateManager const&)` - `const void* self, const void* mgr` here - which is what a
// plain C definition site emits the `bl` against.  The host link has no such object, so
// `src/Kyoto/Alloc/PortMwccNew.cpp` defines the name and forwards it to the host's own
// `CActor::AddToRenderer`; see the note there.  Nothing here claims that function is decompiled.
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object `.text` order verbatim, so an
// ascending file is a permuted `.text` - 100.00% per function and a broken DOL.  Only
// `tools/flip_test.sh` catches that.  A one-function file cannot get it wrong.
//
// Retail names none of this.  `symbols.txt` carries the `fn_<addr>` placeholder and this file
// reproduces that symbol verbatim, so the definition has to stay C: a C++ one would mangle to
// `_Z<len>fn_<addr>v` and objdiff would pair nothing.  That is also why the unit is a `.c` rather
// than a `.cpp`.  The file is compiled as C for the port's host build and, like every other source
// in `files.cmake`, is syntax-checked as C++ by `tools/probe_sources.sh`; both accept it as
// written.
//
// Its own unit because a claim may not span an unclaimed gap.  Below this range
// 0x8016FD4C..0x8016FD94 is `MetroidPrime/Carve8016FD4C.c`'s own claim, and above it
// 0x801708E4..0x80171DC4 is still dtk's (the function immediately above, `fn_801708E4`, is 0x74
// bytes of `rstl::string` plus `CTransform4f` work and is not this one).  The claim is exactly the
// one 0x20-byte run and nothing else.
//
// The directory is retail's own, taken from the nearest claimed ranges:
// `MetroidPrime/Carve8016FD4C.c` (0x8016FD4C..0x8016FD94) is the claim below this one and
// `MetroidPrime/CWorldLayerState.cpp` (0x80171DC4..0x801724CC) the one above, so this address is in
// the `MetroidPrime/` neighbourhood.

/** 0x8004CB7C, `symbols.txt:1485`, 0x184 = 388 bytes: `CActor::AddToRenderer(const CStateManager&)
 *  const`, the base implementation this override forwards to.  Its bytes sit inside
 *  `MetroidPrime/CActor.cpp`'s existing `NonMatching` claim, so no unit of this carve defines it;
 *  it is declared here with retail's own mangled name, which is what a plain C definition site
 *  emits the `bl` against.  Declared, never defined here - the host link's definition is
 *  `src/Kyoto/Alloc/PortMwccNew.cpp`, which forwards to the host's own `CActor::AddToRenderer`.
 *  `self` is const because the method is, which is also why the override can sit in slot 8
 *  unchanged. */
extern void AddToRenderer__6CActorCFRC13CStateManager(const void* self, const void* mgr);

/** `fn_801708C4` - retail `.text:0x801708C4`, 0x20 = 32 bytes, 8 instructions: vtable slot 8 of
 *  `lbl_803B5500`, i.e. `AddToRenderer(const CStateManager&) const`, and a pure forwarder to
 *  `CActor::AddToRenderer` above - a frame, one `bl`, both argument registers untouched, no return
 *  value.  Its measured twin is `fn_80004438` (0x80004438, 0x20,
 *  `src/MetroidPrime/Carve80004438.c`, `Matching`), the same eight instructions with a different
 *  `bl` target. */
void fn_801708C4(const void* self, const void* mgr);

void fn_801708C4(const void* self, const void* mgr) {
  AddToRenderer__6CActorCFRC13CStateManager(self, mgr);
}