// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the addresses and
// sizes come from `config/G2ME01/symbols.txt:73-74`, the instructions are the ones dtk itself
// emitted into `build/G2ME01/asm/auto_03_8000408C_text.s:287-306` before the claim existed (the
// same range is now `build/G2ME01/asm/MetroidPrime/Carve80004438.s`), and the bodies below are the
// C those bytes are the compilation of.
//
// .text 0x80004438..0x8000447C, 0x44 = 68 bytes, 2 functions:
//
//   fn_80004438    0x80004438  0x20     8 instructions   `rstl::destroy< CWorldState >(T*)`
//   fn_80004458    0x80004458  0x24     9 instructions   `rstl::destroy_impl< CWorldState >(T*)`
//
// **The two are the `rstl::destroy` pair for the 36-byte `CWorldState` element**, and the spellings
// below are the byte-shape twins' own, measured in this tree rather than guessed:
//
//   fn_80004458  materialises `li r4,-1` and calls `fn_8000447C` above, taking nothing else from
//                its own argument - that is `destroy_impl`'s `in->~T()`, and the -1 is MWCC's
//                "destroy, do not free me afterwards" flag (`include/rstl/construct.hpp`: `destroy`
//                at :92-95, `destroy_impl` at :84-90 with `in->~T()` at :89).  The twin is exact:
//                `fn_80004C6C` (0x80004C6C, `Player/Carve80004C4C.c`, `Matching`) is these nine
//                instructions word for word with `bl fn_80004A4C` in place of `bl fn_8000447C`,
//                and `destroy_impl< 11CTweakValue >__4rstlFP11CTweakValue` (0x80006850, 0x24) is
//                the same nine again.  Its callers are `fn_80004438` below at 0x80004444 and
//                `fn_801435D4` (`src/MetroidPrime/Player/CGameState.cpp:984`) at 0x801435E0 - both
//                forward with -1, which `CGameState.cpp:1317-1319` states outright.
//   fn_80004438  is a frame and one unconditional `bl fn_80004458` - no load, no test, no return
//                value - so it is the `destroy` of the pair, whose whole body is
//                `destroy_impl(in)`.  The twin is `fn_80004C4C` (0x80004C4C, same neighbouring
//                carve, `Matching`), the same 8 instructions with a different `bl` target, and so
//                is `__sys_free` (0x80008A28, `src/MetroidPrime/main.cpp`).
//
// **The element identity is measured from both sides, not assumed.**  The only caller of
// `fn_80004438` in retail (measured with `grep -rn 'bl fn_80004438' build/G2ME01/asm/`) is
// 0x8000440C inside `fn_800043E8` (0x800043E8, 0x50), which walks a `rstl::vector` with
// `addi r31,r31,36` from its `begin` to its `end`, destroying every element - that is the
// `rstl::vector< CWorldState > mWorldStates` array (`include/MetroidPrime/Player/CGameState.hpp:254`)
// whose 0x24-byte element is `CWorldState` (`CHECK_SIZEOF(CWorldState, 0x24)`,
// `include/MetroidPrime/Player/CWorldState.hpp:48`).  The callee above the claim, `fn_8000447C`
// (0x8000447C, 0x94), releases three `rc_ptr` payloads at +0x1C/+0x10/+0x08 and frees behind the
// flag's `extsh`/`ble` - the destructor those three members need, which is the same function this
// file's `fn_80004458` calls with -1.
//
// `fn_8000447C` (0x8000447C, `symbols.txt:75`) is **above** this claim and is therefore declared,
// never defined here.  Its own 0x94 bytes are a spelling job of its own, which is why the claim
// stops where it does, and its `bl` at 0x80004468 is retail's, so the carve cannot drop the call.
// For the DOL dtk's own `auto_*` object defines it; for the port link it is the announced stand-in
// `stub_199` in `src/MetroidPrime/PortLinkStubs.cpp`, the same trade `stub_186` makes for
// `Carve80004010.c`'s callee.  Nothing here claims `fn_8000447C` is decompiled.
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object `.text` verbatim, so an
// ascending file is a permuted `.text` - 100.00% per function and a broken DOL.  Only
// `tools/flip_test.sh` catches that.
//
// Retail names neither of these.  `symbols.txt` carries the `fn_<addr>` placeholders and this file
// reproduces those symbols verbatim, so the definitions have to stay C: a C++ one would mangle to
// `_Z<len>fn_<addr>v` and objdiff would pair nothing.  That is also why the unit is a `.c` rather
// than a `.cpp`.
//
// Its own unit because a claim may not span an unclaimed gap: below it,
// 0x8000408C..0x80004438 (`fn_8000408C` .. `fn_800043E8`, `__dt__10CGameStateFv`) is unclaimed,
// and above it, 0x8000447C..0x800045A0 (`fn_8000447C`, `fn_80004510`) is too.  The directory is
// retail's own, taken from the nearest claimed ranges: below is `MetroidPrime/Carve80004010.c`
// (0x80004010..0x8000408C), above is `MetroidPrime/Carve800045A0.c` (0x800045A0..0x80004744).

/** 0x8000447C, `symbols.txt:75`, 0x94 = 148 bytes: the 36-byte element's destructor, the function
 *  this file's `fn_80004458` calls with the `-1` "do not free me" flag.  Declared, never defined
 *  here; the port link's stand-in is `stub_199` in `src/MetroidPrime/PortLinkStubs.cpp`.  The
 *  signature matches the declaration `src/MetroidPrime/Player/CGameStateStreamCtor.cpp:236`
 *  already carries.  `-1` is an `int`, not a `short`: the twin's `li r4,-1` is the same
 *  instruction either way, and the callee's `extsh` reads the caller's flag word. */
extern void fn_8000447C(void* self, int flag);
/* The ninth upstream sync (2026-10-02) named 0x8000447C `__dt__11CWorldStateFv` in `symbols.txt`,
 * so retail's link has no `fn_8000447C` any more and this file's `bl` must spell the new name.
 * The port keeps the old one, which nothing defines there: it is one of the listed undefined
 * symbols in `docs/research/port_link_gap_list.md`, not `stub_199` as the note above says
 * (that stub is `fn_801FDAE8`). */
#ifdef __MWERKS__
extern void __dt__11CWorldStateFv(void* self, int flag);
#define fn_8000447C __dt__11CWorldStateFv
#endif

/** `fn_80004458` - retail `.text:0x80004458`, 0x24 = 36 bytes: `rstl::destroy_impl< CWorldState >`,
 *  i.e. the element's destructor called with the "do not free me" flag.  The twin is `fn_80004C6C`
 *  (`Player/Carve80004C4C.c`, `Matching`), these nine instructions word for word apart from the
 *  `bl` target. */
void fn_80004458(void* self);

void fn_80004458(void* self) { fn_8000447C(self, -1); }

/** `fn_80004438` - retail `.text:0x80004438`, 0x20 = 32 bytes: `rstl::destroy< CWorldState >`, a
 *  frame and one call to `fn_80004458` above and nothing else, as `include/rstl/construct.hpp:92-95`
 *  spells it.  Its measured twin is `fn_80004C4C` (0x80004C4C, `Player/Carve80004C4C.c`,
 *  `Matching`), the same 8 instructions with a different `bl` target. */
void fn_80004438(void* self);

void fn_80004438(void* self) { fn_80004458(self); }
