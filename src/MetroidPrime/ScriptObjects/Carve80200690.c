// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the addresses and
// sizes come from `config/G2ME01/symbols.txt:8369-8370`, the instructions are the ones dtk itself
// emitted into `build/G2ME01/asm/auto_03_801FFBA0_text.s:806-816` before the claim existed, and are
// still readable with
// `build/binutils/powerpc-eabi-objdump -d --start-address=0x80200690 --stop-address=0x802006b4
// build/G2ME01/main.elf`.  The body below is the C those bytes are the compilation of.
// `build/G2ME01/asm/MetroidPrime/ScriptObjects/Carve80200690.s` is this unit's own generated
// listing, not the retail one - it is our compile, so it can only confirm, never establish, what
// retail had.
//
// .text 0x80200690..0x802006B0, 0x20 = 32 bytes, 1 function:
//
//   fn_80200690    0x80200690  0x20     8 instructions   `CActor::EnsureRendered(mgr)`
//
// **The body is a frame and one unconditional `bl` - no load, no test, no return value - and the
// `bl` target is 0x8004C8FC, which `symbols.txt:1482` names
// `EnsureRendered__6CActorCFRC13CStateManager` (0xDC bytes).**  The eight words are:
//
//   80200690  stwu r1,-0x10(r1)      802006A0  lwz  r0,0x14(r1)
//   80200694  mflr r0                802006A4  mtlr r0
//   80200698  stw  r0,0x14(r1)       802006A8  addi r1,r1,0x10
//   8020069C  bl   0x8004C8FC        802006AC  blr
//
// so it forwards `r3` and `r4` untouched: that is `CActor::EnsureRendered(const CStateManager&)`
// written as a thunk over its own `this`.  **The twin is `fn_80004438`** (0x80004438, `symbols.txt:73`,
// 0x20 bytes, `src/MetroidPrime/Carve80004438.c`, a `Matching` unit, whose header derives the same
// eight-word shape from `rstl::destroy`): the same eight words with the one `bl` word differing, and
// that twin's C is one call in one line - `void fn_80004438(void* self) { fn_80004458(self); }`.
// `fn_80200690` is 8 of 8 against it, `bl` word included except for the displacement.
//
// **Retail reaches it only through a vtable, and that is measured, not assumed.**  `grep -rn 'bl
// fn_80200690' build/G2ME01/asm/` finds nothing - no direct call anywhere in the DOL - while a scan
// of the `.data` words for the value 0x80200690 finds exactly one, at 0x803B7C40.  That address is
// **slot 10 of `lbl_803B7C18`** (`symbols.txt:18321`, size 0x80 = 32 entries), whose slot 2 is
// `fn_801FFEB0`, slot 3 is the named `TypesMatch__15CScriptTextPaneCFi`, slot 9 is `fn_802006B0` -
// the 0x6C-byte function directly above this claim - and slots 13-30 are `CActor` members
// (`PreRenderAllViewports`, `HealthInfo`, `GetTouchBounds`, `GetSortingBounds`, ...).  So the two
// unnamed slots 9 and 10 are this class's overrides of two `CActor` virtuals, and the body says which
// one slot 10 is.  **Nothing here claims to know the class**: the 32-entry table has two null slots
// and a `fn_`-named destructor at slot 2, and no unit claims 0x803B7C18, so the table is dtk's
// `.data` and only its pointer is load-bearing evidence.
//
// `EnsureRendered__6CActorCFRC13CStateManager` is a **named** retail symbol, and for the DOL that is
// enough: `src/MetroidPrime/CActor.cpp:413` defines it and
// `build/G2ME01/asm/MetroidPrime/CActor.s:3234` shows the emitted `.fn
// EnsureRendered__6CActorCFRC13CStateManager, global`.  This file therefore declares retail's own
// name and never defines it - a `Matching` unit needs its callees' *symbols*, not their bodies.
// The same reference in a neighbouring unit is `ScriptObjects/CIngBoostBallGuardian13E68.cpp:39`,
// which declares this very name.
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object `.text` order verbatim, so an
// ascending file is a permuted `.text` - 100.00% per function and a broken DOL.  Only
// `tools/flip_test.sh` catches that.
//
// Retail names none of these.  `symbols.txt` carries the `fn_<addr>` placeholder and this file
// reproduces that symbol verbatim, so the definitions have to stay C: a C++ one would mangle to
// `_Z<len>fn_<addr>v` and objdiff would pair nothing.  That is also why the unit is a `.c` rather
// than a `.cpp`.  The file is compiled as C for the port's host build and, like every other source
// in `files.cmake`, is syntax-checked as C++ by `tools/probe_sources.sh` - hence the explicit
// `void*`, which is compile-time only and leaves the object byte-identical.
//
// Its own unit because a claim may not span an unclaimed gap: below it, 0x801FFBA0..0x80200690 (the
// head of `auto_03_801FFBA0_text`, 18 functions per `build/report.json`) is unclaimed, and above it
// `fn_802006B0` (0x802006B0, `symbols.txt:8370`, 0x6C = 108 bytes) and the rest of that object are
// too.  The directory is retail's own, taken from the nearest claimed range: below is
// `MetroidPrime/ScriptObjects/Carve801FFA20.cpp` (0x801FFA20..0x801FFBA0), and the nearest claimed
// range above is `MetroidPrime/ScriptLoader/SpacePirate.cpp` (from 0x80200E10) - 0x7E0 bytes past the
// end of this claim.
//
// **Port.**  The host build mangles `CActor::EnsureRendered` the Itanium way, so nothing in the port
// defines the unmangled name this file's call needs, and the `#else` half is an announced no-op
// rather than a guess at the real work: nothing in the port builds a vtable for
// `lbl_803B7C18`, and no port source names `fn_80200690`.

/** retail's own decorated name for `CActor::EnsureRendered(const CStateManager&) const`, the symbol
 *  its `bl` at 0x8020069C has to resolve to.  `symbols.txt:1482`, 0x8004C8FC, 0xDC = 220 bytes;
 *  defined by `src/MetroidPrime/CActor.cpp:413`, declared here, never defined.  Plain C, so this
 *  name needs no `extern "C"` - mwcceppc does not mangle C. */
extern void EnsureRendered__6CActorCFRC13CStateManager(const void* self, const void* mgr);

#ifndef TARGET_PC

/** `fn_80200690` - retail `.text:0x80200690`, 0x20 = 32 bytes: this class's `EnsureRendered`
 *  override, reached only through `lbl_803B7C18` slot 10.  Its measured twin is `fn_80004438`
 *  (`src/MetroidPrime/Carve80004438.c`, `Matching`): the same eight instructions. */
void fn_80200690(const void* self, const void* mgr);

void fn_80200690(const void* self, const void* mgr) {
  EnsureRendered__6CActorCFRC13CStateManager(self, mgr);
}

#else

/* Port stand-in - named, not plausible.  See "Port" above: the DOL half is the real body, and the
 * host has no `EnsureRendered__6CActorCFRC13CStateManager` to call. */
void fn_80200690(const void* self, const void* mgr) {
  (void)self;
  (void)mgr;
}

#endif
