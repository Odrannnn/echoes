// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the address and
// size come from `config/G2ME01/symbols.txt:10671`, the instructions are the ones dtk itself
// emitted into `build/G2ME01/asm/auto_03_8025F468_text.s:766-776` before the claim existed, and
// the body below is the C those bytes are the compilation of.  Re-measured on `main.elf` with
// `build/binutils/powerpc-eabi-objdump -d --start-address=0x8025FE8C
// --stop-address=0x8025FEAC build/G2ME01/main.elf`.
//
// .text 0x8025FE8C..0x8025FEAC, 0x20 = 32 bytes, 1 function:
//
//   fn_8025FE8C    0x8025FE8C  0x20    8 instructions
//                                       stwu r1,-0x10(r1) / mflr r0 / stw r0,0x14(r1) /
//                                       bl fn_8025FE00 / lwz r0,0x14(r1) / mtlr r0 /
//                                       addi r1,r1,0x10 / blr
//
// **It is a byte-shape twin of `fn_80004438`** (`src/MetroidPrime/Carve80004438.c:91-97`,
// `Matching`): the same eight instructions, word for word, with `4b ff ff 69` (`bl 8025fe00`)
// where the twin has its `48 00 00 15` (`bl fn_80004458`).  A second twin is `fn_80004C4C`
// (0x80004C4C, 0x20, `src/MetroidPrime/Player/Carve80004C4C.c`, `Matching`), also identical apart
// from the call target.  What the twin proves is only the *shape* - a frame, one unconditional
// tail call, no load and no test - and this copy is **not** the `rstl::destroy` the twin is: see
// the callee below.  The shape is what is written here; nothing else is claimed.
//
// **The callee is `fn_8025FE00` (0x8025FE00, `symbols.txt:10670`, `size:0x8C` = 140 bytes), and
// its own 0x8C bytes say what this forwarder forwards.**  It reads, in order: `mr r30,r4` and
// `mr r29,r3` (both incoming arguments kept), then `bl GetClassID__20CParticleDataFactoryFR12CInputStream`
// with **r3 and r4 untouched** - and that call is `static`, which is measured off retail rather
// than assumed: `GetModel__20CParticleDataFactoryFR12CInputStreamP11CSimplePool` (0x802E14E4,
// `src/Kyoto/Particles/CParticleDataFactory.cpp`, `Matching`) does `mr r3,r30` before the very
// same call, and `fn_8025FE00` has no such move.  So `fn_8025FE00`'s r3 is the stream, not a
// receiver.  It then range-checks the returned id - `subis r0,r3,0x4450` / `cmplwi r0,0x534d`,
// i.e. the half-open range `[0x4450, 0x978D)` - and returns `li r3,0x0` when it misses; on a hit it
// runs `__nw__FUlPCcPCc(0x60, lbl_803ADD68, 0)`, the 0x6C-byte `fn_80262CD0` on the new pointer,
// then `fn_8025F4F4(new, stream, arg)` (0x8025F4F4, `size:0x4A4`), and hands back the new pointer
// in r3.  That is a class-id-checked object factory, and the two arguments are the stream and one
// further word; both arrive here in r3 and r4 and are forwarded without being moved.
//
// **That is also why the return type is a pointer.**  The only caller of `fn_8025FE8C` in retail -
// `grep -rn 'bl fn_8025FE8C' build/G2ME01/asm/` finds exactly one - is at **0x8026020C**, inside
// the function dtk lists as `FDecalDataFactory__FRC10SObjectTagR12CInputStreamRC15CVParamTransfer`
// (0x802601D0, 0x6C, the unclaimed run this claim sits inside).  It sets `r3` from its own `r5`
// (0x802601EC), sets `r4` from `*(its r6[0] + 4)` (0x80260204-0x80260208), and after the call does
// `mr r0,r3` (0x80260210) to pass the result as the second argument of `fn_8026023C`.  So r3 is
// read back, and the epilogue below has no `mr r3,...` precisely because a tail call leaves the
// callee's r3 where it is.  Declaring the result `void` would compile to the same bytes - the twin
// is `void` - but it would be a signature retail's own call site contradicts.
//
// **The callee is declared, never defined here.**  `fn_8025FE00` is the next unsourced function
// of the same `auto_*` run and this claim stops at 0x8025FEAC, so the DOL link takes it from
// dtk's own object of the run.  Nothing here claims it is decompiled; only its signature reaches
// this unit's bytes.
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object `.text` order verbatim, so
// an ascending file is a permuted `.text` - 100.00% per function, objdiff still happy,
// `unit_fit.sh` still "fits", the link still succeeding, and a broken DOL sha1.  Only
// `tools/flip_test.sh` catches that.  A one-function file cannot violate the rule either way, so
// `python3 tools/check_decl_order.py --unit Weapons/Carve8025FE8C.c` has nothing to compare.
//
// Retail names this function nothing.  `symbols.txt` carries the `fn_<addr>` placeholder and this
// file reproduces that symbol verbatim, so the definition has to stay C: a C++ one would mangle to
// `_Z<len>fn_<addr>v` and objdiff would pair nothing.  That is also why the unit is a `.c` rather
// than a `.cpp`.
//
// Its own unit because a claim may not span an unclaimed gap: below it, 0x8025FE00..0x8025FE8C
// (`fn_8025FE00`) is unclaimed, and above it, 0x8025FEAC..0x8025FF3C (`fn_8025FEAC`, which sets
// `__vt__31CObjOwnerDerivedFromIObjUntyped` and then `__vt__4IObj`) is too.  The claim spans no gap
// at all: it is exactly one function.  It also does **not** start where another unit's `.text`
// ends, which is the arrangement that gives `dtk dol split` a link-order cycle (see "The carve
// vein" in `docs/RUNNING_THE_DECOMP.md`): it starts at the end of `fn_8025FE00`, which no unit
// claims.
//
// The directory is retail's own, taken from the nearest claimed ranges: below is
// `Weapons/CCollisionResponseData.cpp` (`.text` 0x8025DD1C..0x8025F41C) and above is
// `Weapons/Carve8026023C.cpp` (0x8026023C..0x8026030C), so this address is in the `Weapons/`
// neighbourhood - the same reasoning `src/Weapons/Carve8026023C.cpp:107-111` records for the run
// this claim is carved out of.

/** 0x8025FE00, `symbols.txt:10670`, `size:0x8C`: the class-id-checked factory this function
 *  forwards to, taking the stream and one further word and returning the new object (or `0` when
 *  the id is out of `[0x4450, 0x978D)`).  Declared, never defined here; the port link's stand-in
 *  is the announced empty body below.  Both parameters are `void*` because retail names neither
 *  and no header in this tree declares them: only their arrival in r3 and r4 reaches this unit's
 *  eight instructions. */
extern void* fn_8025FE00(void* stream, void* arg);

/** `fn_8025FE8C` - retail `.text:0x8025FE8C`, 0x20 = 32 bytes, 8 instructions: a frame, one
 *  unconditional tail call to `fn_8025FE00` with r3 and r4 forwarded untouched, and the
 *  epilogue.  Byte-shape twin of `fn_80004438` (`src/MetroidPrime/Carve80004438.c:91-97`) and
 *  `fn_80004C4C`; the result is read back by its one caller at 0x8026020C, so the pointer comes
 *  straight out of the callee's r3 with no move in the epilogue. */
void* fn_8025FE8C(void* stream, void* arg) { return fn_8025FE00(stream, arg); }

#ifndef __MWERKS__
// Port-only stand-in, empty body, and it **is** empty.  `fn_8025FE00` is 0x8C retail bytes of a
// class-id test, an allocation and a constructor - none of it claimed by anything in this tree -
// so the DOL link takes it from dtk's object of the run this claim was carved out of, and the
// host link needs a definition because `files.cmake` compiles this file.  It is the same trade
// `src/Weapons/Carve8026023C.cpp:215-239` makes for `fn_8026030C` and `fn_802603A8`: an announced
// stand-in, kept beside the one reference that asks for it, so the port's undefined-symbol count
// cannot move.  The guard is `__MWERKS__`, not `TARGET_PC`, to match those files: the matching
// build must take the symbol from dtk's object, and a second definition there would be the
// duplicate `tools/gate.sh`'s `port link dups` step exists to catch.  It returns nothing, because
// "no object" is the only answer an empty body can honestly give and retail's own miss path is
// `li r3,0x0`.
void* fn_8025FE00(void* stream, void* arg) {
  (void)stream;
  (void)arg;
  return 0;
}
#endif
