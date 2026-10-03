// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the address
// and size come from `config/G2ME01/symbols.txt:9839`, the instructions are retail's own in
// `build/G2ME01/asm/auto_03_8022A058_text.s:15-40` (the 10-function auto unit this claim is
// carved out of; the split below breaks that unit in two, see the last paragraph),
// re-confirmed against `build/G2ME01/main.elf` with `powerpc-eabi-objdump -d
// --start-address=0x8022A060 --stop-address=0x8022A0C0`, and the body below is the C those
// bytes are the compilation of.
//
// .text 0x8022A060..0x8022A0C0, 0x60 = 96 bytes, 1 function:
//
//   fn_8022A060    0x8022A060  0x60    24 instructions
//                                       stwu r1,-0x10(r1) / mflr r0 / stw r0,0x14(r1) /
//                                       stw r31,0xc(r1) / mr r31,r4 / stw r30,0x8(r1) /
//                                       mr. r30,r3 / beq <epilogue> / lis r5,lbl_803B86B8@ha /
//                                       li r4,0x0 / addi r0,r5,lbl_803B86B8@l /
//                                       stw r0,0x0(r30) / bl __dt__6CActorFv /
//                                       extsh. r0,r31 / ble <epilogue> / mr r3,r30 /
//                                       bl Free__7CMemoryFPCv / ... epilogue, with
//                                       `mr r3,r30` before `lwz r31` / `lwz r30`
//
// **What it is: the deleting destructor of the `CActor` subclass this vtable belongs to.**
// That is read off the vtable, not guessed: `lbl_803B86B8` (`.data:0x803B86B8`,
// `size:0x80`, `symbols.txt:18370`) is listed in `build/G2ME01/asm/auto_07_803B8578_data.s`
// and its **third** `.4byte` - i.e. entry 0x8, the slot after the two leading
// `0x00000000` words - is `fn_8022A060`.  A table of 0x80 = 32 words with one destructor entry
// and then the virtuals is MWCC's shape for a `virtual ~T(bool memFreeing)`, and the body is
// that spelling: the receiver guard `mr. r30,r3 / beq`, the derived vptr store at offset 0, the
// base destructor, the 16-bit flag test and the free.  The vtable's other entries put the class
// in the `CScriptAIHint` family of `CActor` subclasses and none the other way: entry 0xC is
// `TypesMatch__13CScriptAIHintCFi` and entry 0x10 is `PreThink__7CEntityFfR13CStateManager`
// (same listing), and the members the rest of the range touches sit at +0x16C/+0x16E/+0x170
// (`SetInUse__13CScriptAIHintFb`, `GetInUse__13CScriptAIHintCF9TUniqueId`,
// `GetValueParm__13CScriptAIHintCFv`, all in the same listing).  Retail gives the class no name
// in `symbols.txt`, so neither does this file; `fn_8022A060` is the whole of what is known.
//
// **The three load-bearing details of the spelling** are the ones
// `src/MetroidPrime/ScriptObjects/CFogOverlayRel.cpp:183-187` measures on the byte-identical
// shape (`fn_23_0`, its own 0x60-byte deleting destructor in module 23), and they are:
// the flag parameter is a **`short`** (`int` gives `cmpwi r31,0` where retail has
// `extsh. r0,r31`), the return type is a **pointer** (a `void` leaf loses the trailing
// `mr r3,r30`), and the base destructor is called with a literal `0`, which is what puts
// `li r4, 0` between the vtable `lis` and the `addi`.  `fn_8022EB54`
// (`src/MetroidPrime/ScriptLoader/Carve8022EB54.c:106`) is the same shape with the two
// statements of the `if (self)` body left out, so it is the closer of the two twins.
//
// **The vptr store is written by hand**, exactly as `fn_23_0` writes it, and for the reason
// that file gives: a class whose first non-inline virtual is defined here is the translation
// unit that emits `__vt__`, and an emitted `__data` object that no split claims moves the DOL's
// bytes.  So this file stores through a `void**` and never declares a class.
//
// **Both callees are retail's own linker names, and both are already in the DOL.**  Measured
// against `config/G2ME01/splits.txt`: `__dt__6CActorFv` (`.text:0x8004DBE4`, `size:0x1A4`,
// `symbols.txt:1496`) is claimed by `MetroidPrime/CActor.cpp` (0x80049ED8..0x8004E84C) and
// `Free__7CMemoryFPCv` (`.text:0x802CE388`, `size:0x64`, `symbols.txt:12992`) by
// `Kyoto/Alloc/CMemory.cpp` (0x802CE224..0x802CE72C).  Neither is declared in a header here
// and neither is defined by this file, so the DOL link pairs both from those units.
//
// **The vtable is claimed by no unit of ours**, so it is `extern` in the matching build and
// the host gets its own definition - the arrangement `Carve80229EAC.c` (the same directory,
// the same 8-byte-carve-from-an-auto-range shape) already uses for its unclaimed `.sbss` slot.
// `lbl_803B86B8` lives in dtk's `auto_07_803B8578_data.o`: the nearest `.data` claim in
// `config/G2ME01/splits.txt` ends at 0x803B8578, and `grep -rn 803B86B8 src/ include/` finds
// only the comment in `src/MetroidPrime/Carve8010EE5C.c:30`, so there is exactly one
// definition of it in the tree and it is this one.  Nothing in `src/` dispatches through it -
// the class has no claimed unit, no instance and no constructor - so the port never reaches
// the store; the host definition is zero-initialised on purpose and unreferenced otherwise.
//
// **The base destructor is the port's own `CActor::~CActor` outside the matching build.**
// `src/MetroidPrime/CActor.cpp:151` defines it and `include/MetroidPrime/CActor.hpp:72`
// declares it virtual, so the host link has `_ZN6CActorD1Ev` (`nm` on
// `build-port-link/CMakeFiles/mp_game.dir/src/MetroidPrime/CActor.cpp.o` -> `T
// _ZN6CActorD1Ev`); the matching build has retail's `__dt__6CActorFv` and nothing named
// `_ZN6CActorD1Ev`, so each build gets the one it can link.  Calling it is not a stand-in:
// `class CActor : public CEntity` (`CActor.hpp:44`) over a base-less `class CEntity`
// (`CEntity.hpp:13`) is single inheritance with no virtual bases, so the host `this` is the
// object address - the same pointer retail passes in r3 and the one the vptr is stored
// through - and retail's flag `0` is the host's "do not delete", which is what `~CActor`
// does.  This is the arrangement `src/MetroidPrime/Carve8010EE5C.c:143-149` already uses for
// `__ct__6CAABoxFRC9CVector3fRC9CVector3f`, and `__MWERKS__` is the discriminator the tree
// already uses for that split.
//
// Retail names this function nothing.  `symbols.txt` carries the `fn_<addr>` placeholder and
// this file reproduces that symbol verbatim, so the definition has to stay C: a C++ one would
// mangle to `_Z<len>fn_8022A060v` and objdiff would pair nothing.  That is also why the unit
// is a `.c` rather than a `.cpp`.
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object's `.text` order verbatim,
// so an ascending file is a permuted `.text` - 100.00% per function, objdiff still happy,
// `unit_fit.sh` still "fits", the link still succeeding, and a broken DOL sha1.  Only
// `tools/flip_test.sh` catches that.  A one-function file cannot violate the rule either way.
//
// Its own unit because a claim may not span an unclaimed gap and a unit may not claim two
// discontiguous ranges in one section (dtk `dol split` fails with "Cyclic dependency ...
// link order").  Below this claim `MetroidPrime/ScriptLoader/PuddleSpore.cpp` holds
// 0x8022A02C..0x8022A058 (its own header, and `fn_8022A058` - the 8-byte `gLoader_PuddleSpore`
// setter - between them and here is another lane's item), and above it
// `MetroidPrime/ScriptLoader/Carve8022A3F4.c` starts at 0x8022A3F4; the 0x334 bytes between
// this claim and it hold `fn_8022A0C0`, the 0x20C-byte loader proper, and are not trivial.
//
// **What the split does to the auto unit, measured after the build.**  Before it,
// `main/auto_03_8022A058_text` held `# 0x8022A058..0x8022A3F4 | size: 0x39C` and **ten**
// functions; a claim in the middle of an auto unit splits it in two, and after it the two
// listings read `# 0x8022A058..0x8022A060 | size: 0x8` (one function) and
// `# 0x8022A0C0..0x8022A3F4 | size: 0x334` (eight), with this unit's own third.  Ten functions
// in, ten out, and `total_functions` is still 28465 - the check that matters after a
// `splits.txt` edit.  Re-measure both names rather than recalling them: a claim in the middle
// of an auto unit splits it in two (this one), a claim at its end only shortens it
// (`docs/goal-notes/carve-80229eac.md`), and a claim over its first bytes renames it.
//
// The directory is retail's own, taken from the nearest claimed ranges: both neighbours above
// and below are `MetroidPrime/ScriptLoader`.  For an anonymous function that is the only
// evidence there is, and it beats a lane picking the directory it happened to own.

#ifdef __MWERKS__
/** `__dt__6CActorFv` (`.text:0x8004DBE4`, `size:0x1A4`, `symbols.txt:1496`): retail's own
 *  linker name for `CActor::~CActor`, claimed by `MetroidPrime/CActor.cpp` in the DOL, so this
 *  file only calls it.  `flag` is the same deleting flag this function's own second argument
 *  is, and retail passes a literal `0` ("run the base destructor, do not free"). */
extern void* __dt__6CActorFv(void* self, short flag);
#define dt_cactor(self, flag) __dt__6CActorFv((self), (flag))
#else
/** The port's own `CActor::~CActor` (`src/MetroidPrime/CActor.cpp:151`), mangled the Itanium
 *  way: single inheritance from a base-less `CEntity`, so `this` is the object address and
 *  the host destructor does exactly what retail's flag-`0` call does - no delete. */
extern void _ZN6CActorD1Ev(void* self);
#define dt_cactor(self, flag) _ZN6CActorD1Ev((self))
#endif

/** `Free__7CMemoryFPCv` (`.text:0x802CE388`, `symbols.txt:12992`): retail's `operator delete`,
 *  claimed by `Kyoto/Alloc/CMemory.cpp` in the DOL and defined for the host link by
 *  `src/Kyoto/Alloc/PortMwccNew.cpp:39`.  Declared, never defined here, so the port's
 *  undefined-symbol count cannot move. */
extern void Free__7CMemoryFPCv(const void* ptr);

/** `.data 0x803B86B8`, `symbols.txt:18370`, `size:0x80`: the vtable this destructor is entry
 *  0x8 of.  Claimed by no unit of ours, so the matching build takes it from dtk's
 *  `auto_07_803B8578_data.o` and this file only stores its address.
 *
 *  **The `char[]` spelling is load-bearing, not decoration.**  Declared as an ordinary object
 *  (`extern void* lbl_803B86B8;`) mwcceppc emits a small-data reference, and mwldeppc refuses
 *  the link with
 *
 *      Small data relocation (109) in function 'fn_8022A060' in referencing file
 *      'Carve8022A060.o' requires that symbol 'lbl_803B86B8' in symbol definition file
 *      'auto_07_803B8578_data.o' be in a small data section but is in .data
 *
 *  (measured on this unit, `tools/decomp_build.sh -r`).  An extern of **unknown-size array
 *  type** is the tree's way of saying "this is not small data", and it produces retail's
 *  `R_PPC_ADDR16_HA/LO` pair, the `lis r5,@ha` / `addi r0,@l` these bytes are.  Same spelling
 *  and same reason at `src/MetroidPrime/CErrorOutputWindowCtor.cpp:105` for the unclaimed
 *  vtable `.data 0x803B5910`, and at `src/MetroidPrime/CMainFlowCtor.cpp:37`. */
extern const char lbl_803B86B8[];
/** The address of that table, in the type the vptr store wants.  `const` is cast away exactly
 *  as `CErrorOutputWindowCtor.cpp:132` casts it away for the same store. */
#define LBL_803B86B8 ((void*)lbl_803B86B8)

/** `fn_8022A060` - retail `.text:0x8022A060`, 0x60 = 96 bytes, 24 instructions: the deleting
 *  destructor of the class whose vtable is `lbl_803B86B8`.  A frame, the receiver guard, the
 *  derived vptr store at offset 0, `__dt__6CActorFv(self, 0)`, and the free when the flag says
 *  so - the body of `virtual ~T(bool memFreeing)` with nothing of its own to destroy.  The
 *  receiver comes back in r3, so the function returns `self`. */
void* fn_8022A060(void* self, short flag) {
  if (self) {
    *(void**)self = LBL_803B86B8;
    dt_cactor(self, 0);
    if (flag > 0) {
      Free__7CMemoryFPCv(self);
    }
  }
  return self;
}

#ifndef __MWERKS__
// Host-only definition of the one symbol this unit references that nothing else in `src/`
// defines.  The matching build does not compile this block, so `main.dol` still takes the real
// 0x80-byte table from dtk's auto data object above and the function keeps its retail bytes.
// Without it the port's flat link - which carries our sources and not dtk's objects - loses
// `lbl_803B86B8`, and `tools/link_check.sh --strict`, whose verdict `tools/probe_sources.sh`
// reports, names it as a new undefined symbol.  Sized at retail's 0x80 and zero-initialised on
// purpose: nothing in `src/` dispatches through it, because no unit of ours claims the class
// this vtable belongs to, so there is no instance for the port to tear down.
const char lbl_803B86B8[0x80] = {0};
#endif