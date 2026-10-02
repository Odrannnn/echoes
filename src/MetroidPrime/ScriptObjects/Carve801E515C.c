// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the addresses and
// sizes come from `config/G2ME01/symbols.txt:7824-7825`, the instructions are retail's own at
// `.text 0x801E515C..0x801E51A4`, read out of the disc (`orig/G2ME01/sys/main.dol`, text section 1
// at file offset 0x640 / address 0x80003840 / size 0x3a1c60, parsed from the DOL header this run)
// and **not** out of `build/G2ME01/main.elf`, which holds our bytes once this unit is in the link.
// The body below is the C those bytes are the compilation of.
//
// .text 0x801E515C..0x801E51A4, 0x48 = 72 bytes, 2 functions:
//
//   fn_801E515C    0x801E515C  0x24    9 instructions   `this + 4` forwarder, `bl fn_801E51A4`
//   fn_801E5180    0x801E5180  0x24    9 instructions   `this + 4` forwarder, `bl fn_801E5230`
//
// Both are the same nine words - `stwu r1,-0x10` / `mflr r0` / `addi r3,r3,0x4` / `stw r0,0x14(r1)` /
// `bl` / `lwz r0,0x14(r1)` / `mtlr r0` / `addi r1,r1,0x10` / `blr` - and the second argument (`r4`)
// is passed through untouched, so the call is `(this + 4, key)` with the receiver's keyed subobject
// and the caller's own argument.
//
// **The twin is exact, `bl` aside.**  `GetResourceIdByName__11CResFactoryCFPCc` (retail 0x80006B80,
// 0x24, `symbols.txt:138`) is these nine words **8 of 9 identical** to each of the two (both disc
// reads this run); the one differing word is the `bl` at offset 0x10 (twin `482F60B5` ->
// 0x802FCC44).  That function is 100.00% matched inside `MetroidPrime/main.cpp` (a `NonMatching`
// unit, 98/99 functions), and it is
//
//   const SObjectTag* CResFactory::GetResourceIdByName(const char* name) const {
//     return mResLoader.GetResourceIdByName(name);
//   }
//
// (`include/Kyoto/CResFactory.hpp:64-65`), i.e. the same `this + 4` forwarder for the same reason:
// a subobject held at a fixed offset in the receiver.  The LI of each `bl` decodes to this copy's
// own callee: 0x801E516C + 0x38 = 0x801E51A4, 0x801E5190 + 0xA0 = 0x801E5230.
//
// Nothing is asserted here about the receiver's class.  No member of it is named in either body -
// the only offset is the `+ 4` the bytes themselves carry - so both pointers are `void*` and the
// adjustment is spelled `(char*)self + 4`.  The two retail callers fix the argument list as one
// pointer plus a pass-through second argument and say nothing more:
//   fn_801E4F0C (0x801E4F0C, 0x90, unclaimed) calls `fn_801E515C` at 0x801E4F50 with
//     `r3 = lwz 0x4028(r27) + r31` (r31 stepping 0x10 per element) and `r4 = r28`, and calls the
//     callee **directly** at 0x801E4F2C with `addi r3,r27,0x402c` - 0x402c is 0x4028 + 4, the same
//     adjustment this file makes.  Its result is consumed as a truth value (`or r0,r30,r3` then
//     `clrlwi r0,r0,24`), which is `fn_801E51A4`'s `li r3,0` / `li r3,1` passed through.
//   fn_801E4F9C (0x801E4F9C, 0xBC, unclaimed) calls `fn_801E5180` at 0x801E5004 with
//     `r3 = 0x4028(r28) + (lha index << 4)` and `r4 = r29`, and calls `fn_801E5230` directly at
//     0x801E5028 with `addi r3,r28,0x402c`.
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object `.text` verbatim, so an
// ascending file is a permuted `.text` - 100.00% per function and a broken DOL.  Only
// `tools/flip_test.sh` catches that.
//
// Retail names none of these.  `symbols.txt` carries the `fn_<addr>` placeholder and this file
// reproduces that symbol verbatim, so the definitions have to stay C: a C++ one would mangle to
// `_Z<len>fn_<addr>v` and objdiff would pair nothing.  That is also why the unit is a `.c` rather
// than a `.cpp`.  The file is compiled as C for the port's host build and, like every other source
// in `files.cmake`, is syntax-checked as C++ by `tools/probe_sources.sh` - hence the explicit
// `void*` casts, which are compile-time only and leave the object byte-identical.
//
// Its own unit: these 72 bytes are the only run in the dtk `auto_03_801E3E38_text` range with a
// measured twin, and the functions on either side of them are not trivial - in front, `fn_801E4F0C`
// (0x90) and `fn_801E4F9C` (0xBC) are unclaimed, and behind, `fn_801E51A4` (0x801E51A4, 0x8C)
// starts exactly where the claim ends and is left to dtk: it is a real body (a halfword-keyed list
// search that unlinks its hit through `fn_801E527C`), not a forwarder, and it needs that
// container's layout rather than a twin.
//
// The directory is retail's own, taken from the nearest claimed ranges: the claim is the only one
// inside the unclaimed 0x801E3E38..0x801E70F0, whose nearest claimed neighbours are
// `ScriptObjects/Carve801E3E34.c` (0x801E3E34..0x801E3E38, ends exactly where that range starts)
// below and `Cameras/CCameraShakerData.cpp` (0x801E70F0..0x801E782C) above, so the code is the
// `MetroidPrime/ScriptObjects/` neighbourhood - which is where the item's seeder put it and where
// its two callers sit as well.

/** `fn_801E51A4` - retail `.text:0x801E51A4`, 0x8C = 140 bytes: the halfword-keyed list search
 *  `fn_801E5180` (below) and `fn_801E4F0C` both call, walking the chain at +4 of the receiver and
 *  comparing the key against the halfword at +8 of each node, unlinking its hit through
 *  `fn_801E527C` (0x801E527C, 0x10).  Retail's bytes, unclaimed, so it cannot be dropped from this
 *  unit's calls.  Declared here, never defined. */
extern void* fn_801E51A4(void* self, const void* key);

/** `fn_801E5230` - retail `.text:0x801E5230`, 0x4C = 76 bytes: the push-front onto that same chain
 *  (allocate a node through `fn_801E528C`, store the key at +0, link the old head at +4, store the
 *  node back at +4 of the receiver).  Retail's bytes, unclaimed, same reason.  Declared here, never
 *  defined. */
extern void* fn_801E5230(void* self, const void* key);

/** `fn_801E5180` - retail `.text:0x801E5180`, 0x24 = 36 bytes: the `this + 4` forwarder to
 *  `fn_801E5230`.  The twin is `GetResourceIdByName__11CResFactoryCFPCc` - see the header. */
void* fn_801E5180(void* self, const void* key);

void* fn_801E5180(void* self, const void* key) { return fn_801E5230((char*)self + 4, key); }

/** `fn_801E515C` - retail `.text:0x801E515C`, 0x24 = 36 bytes: the `this + 4` forwarder to
 *  `fn_801E51A4`.  The twin is `GetResourceIdByName__11CResFactoryCFPCc` - see the header. */
void* fn_801E515C(void* self, const void* key);

void* fn_801E515C(void* self, const void* key) { return fn_801E51A4((char*)self + 4, key); }
