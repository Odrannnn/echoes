// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the addresses
// and sizes come from `config/G2ME01/symbols.txt:6041` and `:1473`, the instructions are the ones
// dtk itself emitted into `build/G2ME01/asm/auto_03_8016DF3C_text.s:1622-1632` before the claim
// existed (the same range is now this unit's own
// `build/G2ME01/asm/MetroidPrime/Carve8016F69C.s`), the bytes were re-read this run straight off
// the disc with `python3 tools/dol_read.py 0x8016F69C 0x20`, and the body below is the C those
// bytes are the compilation of.
//
// .text 0x8016F69C..0x8016F6BC, 0x20 = 32 bytes, 1 function:
//
//   fn_8016F69C    0x8016F69C  0x20    8 instructions   a virtual `Touch`, forwarded to the base
//
// Measured off the disc (`dol_read.py 0x8016F69C 0x20`):
//
//   94 21 ff f0  stwu r1,-0x10(r1)      80 01 00 14  lwz  r0,0x14(r1)
//   7c 08 02 a6  mflr r0                7c 08 03 a6  mtlr r0
//   90 01 00 14  stw  r0,0x14(r1)       38 21 00 10  addi r1,r1,0x10
//   4b ed ce bd  bl   0x8004c564        4e 80 00 20  blr
//
// **What it is: `CBouncyGrenade::Touch(CActor&, CStateManager&)`, forwarding to
// `CActor::Touch`.**  Both halves of that are read off the tables rather than guessed:
//
//   - `0x8016F69C` appears exactly **once** in `main.dol`, and reading the address back through the
//     section table puts it in `.data` at 0x803B53D4 (file 0x3B24B4, `.data` 0x803B0C00@0x3ADCE0).
//     `0x803B53D4 - 0x40 = 0x803B5394` is a vtable, and what it holds at its base is
//     `TypesMatch__14CBouncyGrenadeCFi` (0x8009C904, `symbols.txt:3064`) - so it is **CBouncyGrenade's**
//     table, whose constructor is in this very run at 0x8016F874.
//   - `CActor`'s own table starts at 0x803B1AEC (file 0x3AEBC8): what it holds at its base is
//     `TypesMatch__6CActorCFi` (0x8009CC4C, `symbols.txt:3079`), and at **base + 0x40** - 0x803B1B2C -
//     it holds 0x8004C564, which `symbols.txt:1473` names `Touch__6CActorFR6CActorR13CStateManager`.
//     Two independently identified tables with the same 0x40 offset in them is what makes 0x40 the
//     `Touch` slot, and that is the slot `include/MetroidPrime/CActor.hpp:93` declares as
//     `virtual void Touch(CActor&, CStateManager&)`.
//   - the callee is **4 bytes** (`symbols.txt:1473`, `size:0x4`) and those four bytes are a single
//     `blr` (0x8004C564 in `build/G2ME01/main.elf`) - `CActor::Touch` does nothing by itself, so
//     the override this file writes is a pure qualified-base call and nothing is lost by it.
//
// The shape twin named in the item's brief, `fn_80004438` (`src/MetroidPrime/Carve80004438.c`,
// `Matching`), is these eight instructions with a different `bl` target, and so is `__sys_free`
// (0x80008A28, `src/MetroidPrime/main.cpp`) a third time.  That is a measure of the *frame*, not
// of the identity: what makes this one a `Touch` and not an `rstl::destroy` - the other reading of
// a 0x20-byte forwarder - is the vtable slot above, and the callee name settles it.
// `CIngBoostBallGuardian1464C.cpp`'s `fn_30_1464C` is the same body for the same reason: a `Touch`
// override that qualifies the base call.
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object `.text` order verbatim, so an
// ascending file is a permuted `.text` - 100.00% per function and a broken DOL.  Only
// `tools/flip_test.sh` catches that.
//
// Retail names none of these in this range.  `symbols.txt` carries the `fn_<addr>` placeholder and
// this file reproduces that symbol verbatim, so the definition has to stay C: a C++ one would
// mangle to `_Z<len>fn_8016F69C` and objdiff would pair nothing.  That is also why the unit is a
// `.c` rather than a `.cpp`.  The file is compiled as C for the port's host build and, like every
// other source in `files.cmake`, is syntax-checked as C++ by `tools/probe_sources.sh`; both accept
// this file as written.
//
// Its own unit because a claim may not span an unclaimed gap.  Below this range,
// 0x8016DF3C..0x8016F69C is still dtk's `auto_03_8016DF3C_text` (`fn_8016F318`, 0x384, is the
// function immediately below), and above it 0x8016F6BC..0x8016FD4C is the rest of the same
// unclaimed run (`fn_8016F6BC`, 0xE8, is the function immediately above - and is also
// CBouncyGrenade vtable base + 0xC, at 0x803B53A0).  Both neighbours stay unclaimed.
//
// The directory is retail's own, taken from the nearest claimed ranges:
// `MetroidPrime/CRumbleManager.cpp` (0x8016DC58..0x8016DF3C) is the claim below this one and
// `MetroidPrime/Carve8016FD4C.c` (0x8016FD4C..0x8016FD94) the one above, so this address is in the
// `MetroidPrime/` neighbourhood.

/** 0x8004C564, `symbols.txt:1473`, 0x4 = 4 bytes: `CActor::Touch(CActor&, CStateManager&)`, the
 *  callee of `fn_8016F69C` below.  Retail's own body is a single `blr`, so it does nothing - this
 *  override forwards to it and inherits exactly that.  Declared here under retail's own mangled
 *  name, which is what a plain C definition site emits the `bl` against.  Declared, never defined
 *  here: the range belongs to `MetroidPrime/CActor.cpp`'s existing claim
 *  (0x80049ED8..0x8004E84C), so dtk's object for that unit supplies the bytes in the DOL link and
 *  the symbol is nothing this carve can or should claim; in the host link the announced stand-in
 *  `stub_carve8016f69c_0` in `src/MetroidPrime/PortLinkStubs.cpp` defines the name.
 *  `ScriptObjects/CIngBoostBallGuardian1464C.cpp` declares the same name the same way for its own
 *  REL copy of this call. */
extern void Touch__6CActorFR6CActorR13CStateManager(const void* self, void* actor, void* mgr);

/** `fn_8016F69C` - retail `.text:0x8016F69C`, 0x20 = 32 bytes, 8 instructions:
 *  `CBouncyGrenade::Touch`, a frame and one call to `CActor::Touch` above with all three arguments
 *  untouched - no register moves before the `bl`, which is what a qualified base call produces.
 *  Its twin is `fn_80004438` (0x80004438, 0x20, `src/MetroidPrime/Carve80004438.c`, `Matching`),
 *  the same eight instructions with a different `bl` target. */
void fn_8016F69C(const void* self, void* actor, void* mgr);

void fn_8016F69C(const void* self, void* actor, void* mgr) {
  Touch__6CActorFR6CActorR13CStateManager(self, actor, mgr);
}