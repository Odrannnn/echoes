// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the address and
// size come from `config/G2ME01/symbols.txt:8294`, the instructions are the ones dtk itself
// emitted into `build/G2ME01/asm/auto_03_801FDC88_text.s:57-71`, and the body below is the C those
// bytes are the compilation of.
//
// .text 0x801FDD40..0x801FDD6C, 0x2C = 44 bytes, 1 function:
//
//   fn_801FDD40    0x801FDD40  0x2C    11 instructions
//                                       stwu r1,-0x10(r1) / mflr r0 / stw r0,0x14(r1) /
//                                       stw r31,0xc(r1) / mr r31,r3 / bl fn_801FDD6C /
//                                       lwz r0,0x14(r1) / lwz r31,0xc(r1) / mtlr r0 /
//                                       addi r1,r1,0x10 / blr
//
// **It is a byte-shape twin of a function retail itself names, 0x64A0 bytes away**, and the twin
// is what the body below is read from rather than guessed.  Read with
// `python3 tools/dol_read.py 0x801FDD40 0x2c orig/G2ME01/sys/main.dol` and the same command at
// 0x80031A00, the two ranges are the same eleven words, **the `bl` word included**:
//
//   fn_801FDD40    0x801FDD40  0x2C  11 instructions
//   GetIObjObjectFor__23TToken<13CSkinnedModel>FRCQ24rstl25auto_ptr<13CSkinnedModel>
//                                  0x80031A00  0x2C  11 instructions   (`symbols.txt:931`)
//
// Not one of the 44 bytes differs.  The `bl`s both read `48 00 00 19` because in each case the
// callee sits 0x2C bytes past the `bl`: 0x801FDD54 + 0x18 = 0x801FDD6C here, 0x80031A14 + 0x18 =
// 0x80031A2C there.  (The twin's own listing is
// `build/G2ME01/asm/MetroidPrime/Factories/CCharacterFactory.s:2564-2578`, which is **our**
// compile of `src/MetroidPrime/Factories/CCharacterFactory.cpp`, so it can only confirm the
// shape, never establish it - the evidence above is the disc.)  Retail's own name for the shape
// is `TToken<T>::GetIObjObjectFor(const rstl::auto_ptr<T>&)` whose body is
// `include/Kyoto/TToken.hpp:24-27` verbatim, one call and nothing else:
// `return TObjOwnerDerivedFromIObj<T>::GetNewDerivedObject(obj);`.
//
// Two closer twins in this tree already spell it, and both are matched:
// `src/Kyoto/Animation/CAnimCharacterSet.cpp:136-138` (`fn_8028EC50`) with the real class types,
// and `src/MetroidPrime/Carve801EF730.cpp:175-178` (`fn_801EF784`), the same arrangement with a
// local stand-in for `T` because nothing names the real one here.  This copy is the same eleven
// instructions as `fn_801EF784`, `bl`-for-`bl`.
//
// **What the return slot is, measured off retail's own caller.**  `fn_801FD37C` (0x801FD37C,
// `symbols.txt:8292`'s neighbourhood, 0xA4) is the only caller
// (`grep -rn 'bl fn_801FDD40' build/G2ME01/asm/` - one hit,
// `build/G2ME01/asm/auto_03_801FCE40_text.s:402`).  It sets
// `addi r3,r1,0x8 / addi r4,r1,0x10` before its `bl fn_801FDD40` at 0x801FD3AC, writes
// `stb r0,0x10(r1)` and `stw r4,0x14(r1)` **before** that call, and reads `lwz r3,0xc(r1)` after
// it.  So `r3` is a hidden 8-byte **result** slot at `r1+8`, the argument is the 8-byte
// `rstl::auto_ptr` at `r1+0x10`, the two stores before the call are `obj.mHas` and `obj.mItem`
// at the argument's +0 and +4, and the read after it is the **+4** word of the result - i.e. the
// returned `mItem`.  The return is therefore the two-word `{ bool mHas; T* mItem; }` that
// `include/rstl/auto_ptr.hpp:15-16` is, **8 bytes and not 12**, and the result slot does not
// overlap the argument (`r1+8..r1+0xF` against `r1+0x10..r1+0x17`, LR at `r1+0x24`).
//
// **That 8-byte result is the whole of the `mr r31,r3`.**  MWCC passes a hidden return slot in
// `r3` for a class return and keeps it in the callee-saved `r31` across the call, with the
// object argument moving to `r4`; that is what the `stw r31,0xc(r1)` / `lwz r31,0xc(r1)` pair and
// the `mr r31,r3` are for, and nothing in the body reads `r31`.  Measured this run on this
// function: a plain-C spelling returning an **8-byte** POD emits **8** instructions and never
// touches `r31`, because MWCC hands an 8-byte POD back in `r3:r4` - it does not match.  So the
// honest spelling is the class, and only C++ has the class.
//
// **Which is why this unit is a `.cpp` with `extern "C"`, not a `.c`.**  The seed's "plain C so
// the `fn_` names do not mangle" is right about the mangling and wrong about the file: `extern
// "C"` keeps the names verbatim just as well, and the `extern "C"` wrapper is the arrangement
// `src/MetroidPrime/Carve801EF730.cpp:135` and `src/Kyoto/Animation/CAnimCharacterSet.cpp:132`
// already use for Matching carves of exactly this shape.  A `.c` file cannot name
// `rstl::auto_ptr` at all.
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object `.text` verbatim, so an
// ascending file is a permuted `.text` - 100.00% per function, `tools/unit_fit.sh` still "fits",
// the link still succeeding, and a broken DOL sha1.  Only `tools/flip_test.sh` catches that.
// With one function the order is trivially satisfied and
// `python3 tools/check_decl_order.py --unit MetroidPrime/ScriptObjects/Carve801FDD40.cpp` is the
// check either way.
//
// Retail names this function nothing.  `symbols.txt` carries the `fn_<addr>` placeholder and this
// file reproduces that symbol verbatim, so the definition must stay `extern "C"` - a C++ one
// would mangle to `_Z<len>fn_801FDD40...` and objdiff would pair nothing.
//
// **The callee is declared, never defined here**, and nothing in the tree claims it:
// `fn_801FDD6C` (0x801FDD6C, `symbols.txt:8295`, 0x9C = 156 bytes) is the next unsourced
// function of the same `auto_*` run and this claim stops at 0x801FDD6C, so the DOL link takes it
// from dtk's own object of the surrounding run.  It is this copy's
// `TObjOwnerDerivedFromIObj<T>::GetNewDerivedObject(const rstl::auto_ptr<T>&)`: `li r3,0x8 /
// bl __nw__FUlPCcPCc`, three `stw` of the `__vt__4IObj` /
// `__vt__31CObjOwnerDerivedFromIObjUntyped` / `lbl_803B7C08` vtable into the new 8-byte object,
// `stb r5,0x0(r31)` releasing the source's +0 flag and `stw r4,0x4(r3)` copying its +4 pointer,
// then the `(ptr != nullptr)` normalisation into the return slot's +0 and +4.  It is 39
// instructions, the same shape as
// `GetNewDerivedObject__41TObjOwnerDerivedFromIObj<13CSkinnedModel>FRCQ24rstl25auto_ptr<13CSki`
// at 0x80031A2C (0x9C, `symbols.txt:932`) - measured this run, the two differ in 10 of 39
// instructions and all 10 are scheduling, `bl` displacement or data address - so its return type
// is the same class by value and its declaration below is the same.  For the host link it is
// new, and `tools/link_gap.py` fails the gate on a missing symbol that is not accounted for in
// `docs/research/port_link_gap_list.md`, so this file carries the same announced stand-in
// `src/MetroidPrime/Carve801EF730.cpp:197-215` carries for its `fn_801EF7B0`.  So this unit
// claims `.text` and nothing else.
//
// Its own unit because a claim may not span an unclaimed gap.  Below it
// `ScriptObjects/Carve801FDC88.c` claims 0x801FDC88..0x801FDCAC and ends at
// `fn_801FDCAC`'s start; `fn_801FDCAC`..`fn_801FDD40` is unclaimed and stays retail's, and above
// it `fn_801FDD6C`..`fn_801FEA98` is unclaimed too.  The claim is therefore exactly this one
// function's 0x2C bytes and nothing else.
//
// The directory is retail's own, taken from the nearest claimed ranges: below is
// `MetroidPrime/ScriptObjects/Carve801FDC88.c` (0x801FDC88..0x801FDCAC), above is
// `MetroidPrime/ScriptObjects/Carve801FEA98.c` (0x801FEA98..0x801FEAE0), and the run of
// `ScriptObjects/` carves continues either side of this claim, so `MetroidPrime/ScriptObjects/`
// is the neighbourhood rather than a choice.

#include "rstl/auto_ptr.hpp"

extern "C" {

/** What `fn_801FDD6C` allocates: 8 bytes whose first word is a vtable - `li r3,0x8` before its
 *  `bl __nw__FUlPCcPCc`, then `stw r0,0x0(r3)` three times for
 *  `__vt__4IObj` / `__vt__31CObjOwnerDerivedFromIObjUntyped` / `lbl_803B7C08`.  Only the size is
 *  modelled here; this unit's own bytes never load it. */
struct SOwnedObject {
  void** mVtable;
};

/** 0x801FDD6C, `symbols.txt:8295`, size 0x9C: this copy's
 *  `TObjOwnerDerivedFromIObj<T>::GetNewDerivedObject(const rstl::auto_ptr<T>&)`, byte for byte
 *  `GetNewDerivedObject__41TObjOwnerDerivedFromIObj<13CSkinnedModel>F...` at 0x80031A2C (0x9C) -
 *  hence the same return type, the class `rstl::auto_ptr<T>` by value.  Declared, never defined
 *  here: nothing in the tree claims it, so the DOL link takes it from dtk's own object of the run
 *  this claim was carved out of.  `T` is written out as the vtable-holding word the callee
 *  allocates, because `rstl::auto_ptr<T>` only ever holds a `T*` and nothing else is read. */
extern rstl::auto_ptr< SOwnedObject > fn_801FDD6C(const rstl::auto_ptr< SOwnedObject >& obj);

rstl::auto_ptr< SOwnedObject > fn_801FDD40(const rstl::auto_ptr< SOwnedObject >& obj);

/** `fn_801FDD40` - retail `.text:0x801FDD40`, 0x2C = 44 bytes, 11 instructions: a frame, the
 *  return slot copied into the callee-saved r31, one call and the epilogue.  The `mr r31,r3` and
 *  its `stw`/`lwz` pair are the sret convention for the class return - see the header - so `r3`
 *  is the caller's 8-byte result slot and the object argument is already in r4, untouched.  That
 *  is `TToken<T>::GetIObjObjectFor`, whose body is `include/Kyoto/TToken.hpp:24-27` verbatim:
 * `return TObjOwnerDerivedFromIObj<T>::GetNewDerivedObject(obj);`.  Twin of
 * `GetIObjObjectFor__23TToken<13CSkinnedModel>F...` (0x80031A00, 0x2C), these same eleven
 *  words, and of `fn_801EF784` (0x801EF784, 0x2C, `src/MetroidPrime/Carve801EF730.cpp:175`),
 *  which is the same eleven instructions matched in this tree. */
rstl::auto_ptr< SOwnedObject >
fn_801FDD40(const rstl::auto_ptr< SOwnedObject >& obj) {
  return fn_801FDD6C(obj);
}

#ifndef __MWERKS__
// Port-only stand-in, empty body, and it **is** one: `fn_801FDD6C`'s own 0x9C retail bytes are a
// spelling job of their own and nothing in this tree has claimed them.  It exists so the host
// link resolves the `bl` above, and it is the same trade
// `src/MetroidPrime/Carve801EF730.cpp:197-215` makes for `fn_801EF7B0` and
// `src/MetroidPrime/Carve801FDC88.c` makes from `src/MetroidPrime/PortLinkStubs.cpp` for
// `fn_801FDCAC`.  Before this carve nothing in the port referenced the symbol - only dtk's
// `auto_*` objects did - so it is new to the port's link, and `tools/link_gap.py` fails the gate
// on a missing symbol that is not accounted for in
// `docs/research/port_link_gap_list.md`.  The guard is `__MWERKS__`, not `TARGET_PC`, to match
// those files: the matching build must take the symbol from dtk's own object of the surrounding
// run, and a second definition there would be a duplicate.  It returns an empty
// `rstl::auto_ptr`, which is what its default constructor already holds.
rstl::auto_ptr< SOwnedObject >
fn_801FDD6C(const rstl::auto_ptr< SOwnedObject >& obj) {
  (void)obj;
  return rstl::auto_ptr< SOwnedObject >();
}
#endif

} // extern "C"
