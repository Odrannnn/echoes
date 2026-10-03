// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the addresses and
// sizes come from `config/G2ME01/symbols.txt:5008-5010`, the instructions are retail's own, read
// this run with `build/binutils/powerpc-eabi-objdump -d --start-address=0x801285DC
// --stop-address=0x8012862C build/G2ME01/main.elf`, and the body below is the C those bytes are the
// compilation of.  Before the claim existed dtk emitted them into
// `build/G2ME01/asm/auto_03_801285DC_text.s`, whose `.fn fn_801285DC` and `.fn fn_80128604`
// blocks are lines 9-20 and 23-34 of that file.  `build/G2ME01/asm/MetroidPrime/Carve801285DC.s`
// is this unit's own generated listing, not retail's - it is our compile, so it can only confirm,
// never establish, what retail had.
//
// .text 0x801285DC..0x8012862C, 0x50 = 80 bytes, 2 functions:
//
//   fn_801285DC    0x801285DC  0x28    10 instructions
//   fn_80128604    0x80128604  0x28    10 instructions
//
// **What the two are: a pair of argument-shuffling forwarders, each dropping its first argument
// and handing arguments two and three to a callee.**  Read straight off the ten words of
// `fn_801285DC`:
//
//   stwu r1,-0x10(r1) / mflr r0 / mr r3,r4 / mr r4,r5 / stw r0,0x14(r1) /
//   bl 802860BC / lwz r0,0x14(r1) / mtlr r0 / addi r1,r1,0x10 / blr
//
// `mr r3,r4` then `mr r4,r5` is the whole body: the callee sees the caller's `r4` and `r5` in
// `r3` and `r4`, and the caller's `r3` is dead.  Nothing is loaded, spilled or computed, which is
// why the frame is retail's smallest (16 bytes, LR only) and why neither function can be written
// any other way and still produce these ten words.  `fn_80128604` is the same ten words with the
// same two `mr`s and one difference: its `bl` is `48 00 00 15` to `fn_8012862C` 0x14 bytes on,
// where `fn_801285DC`'s is `48 15 DA CD` across the DOL to `CollisionUtil::AddAverageToFront`.
//
// The **frame shape is measured against a byte-shape twin already matched in this tree**, and the
// comparison was run this session rather than recalled.  `fn_800273DC` (0x800273DC, 0x28, 100.00% in
// `src/MetroidPrime/CAnimData.cpp`, retail's own `rstl::less<rstl::basic_string<char> >::operator()`
// by the name in `symbols.txt`) is ten words with the same eight: identical `stwu r1,-0x10(r1)`,
// `mflr r0`, `stw r0,0x14(r1)`, `bl`, `lwz r0,0x14(r1)`, `mtlr r0`, `addi r1,r1,0x10` and `blr`,
// and the same slot offsets `0x14` in and `0x10` out.  The two words it spends where these two
// spend `mr r3,r4 / mr r4,r5` are `srwi r3,r3,31` - which is only that functor normalising the
// `int` its `compare` returns down to a `bool`, and which a `void`-returning carrier has no reason
// to emit.  That is what makes these two twins of the *frame* and not of the body, and it is the
// only thing carried across: the call arguments and the callee names below are this copy's own.
//
// **The one callee retail names.**  `fn_801285DC`'s target is
// `AddAverageToFront__13CollisionUtilFRC18CCollisionInfoListR18CCollisionInfoList` at 0x802860BC,
// which is `CollisionUtil::AddAverageToFront(const CCollisionInfoList&, CCollisionInfoList&)` -
// declared at `include/Collision/CollisionUtil.hpp:14` and defined at `src/Collision/CollisionUtil.cpp:871`.
// MWCC's old mangling is `[A-Za-z0-9_]` only, so a C declaration can name it verbatim and needs no
// alias; that is the same trick `src/MetroidPrime/Carve800E1548.c:77` uses on
// `Free__7CMemoryFPCv`.  (`extern void f(...) asm("<mangled>")` is *not* an option - mwcceppc
// rejects it in both C and C++ mode; `src/MetroidPrime/Carve80281310.c:60` records that
// measurement.)  So the name below is retail's, spelled as retail spells it, and this file never
// defines it: `CollisionUtil.cpp` supplies the bytes.
//
// **The other callee is not claimed, and the reason is measured.**  `fn_8012862C` (0x8012862C,
// 0x160 = 352 bytes, `symbols.txt:5010`) is the one call of the pair's own file: it walks an array
// of 0x60-byte entries, picks the first whose flags (`0x30`/`0x34`) are clear and whose `+0x8` float
// beats a running minimum seeded from `lbl_8041BE40`, copies up to 0x20 entries out of it and hands
// the result to `AddAverageToFront`.  At 352 bytes with six calls it is neither a 64-byte twin nor
// a shape any matched function in this tree carries, and writing it would mean naming the entry
// struct's `0x30`/`0x34` flag words and the `lbl_8041BE40` constant - real work, on a different
// item.  It is declared here and left to dtk's own object, exactly as
// `src/MetroidPrime/Carve80281310.c:87` does with `fn_8028139C`; on the port's flat link, which
// carries no `auto_*` objects at all, `src/MetroidPrime/PortGlobals.cpp` gives it a stand-in that
// prints its own name and does nothing else.
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object `.text` verbatim, so an
// ascending file is a permuted `.text` - 100.00% per function and a broken DOL.  Only
// `tools/flip_test.sh` catches that.
//
// Retail names neither of the two.  `symbols.txt` carries the `fn_<addr>` placeholder for both and
// this file reproduces those symbols verbatim, so the definitions have to stay C: a C++ one would
// mangle to `_Z<len>fn_<addr>v` and objdiff would pair nothing.  That is also why the unit is a
// `.c` rather than a `.cpp`.  Retail does point at both from data - `lbl_803B4E88` and
// `lbl_803B4E98` each hold one of them in the third word
// (`build/G2ME01/asm/auto_07_803B4D78_data.s:88-102`), two vtable-shaped 0x10-byte blocks - which
// is consistent with a virtual method each on its own class and says nothing about the arguments.
//
// Its own unit because a unit may not claim two discontiguous ranges in one section (dtk `dol
// split` fails with "Cyclic dependency ... link order"), and because the claim is a proper prefix
// of the `auto_*` unit rather than all of it: `fn_8012862C` stays retail's, so this cannot be the
// whole `auto_03_801285DC_text` object.
//
// The directory is retail's own, taken from the nearest claimed range: the claim immediately below
// is `MetroidPrime/CGameCollision.cpp` (`.text` 0x80123510..0x801285DC, which ends *exactly* where
// this claim starts) and the one above is `MetroidPrime/CGroundMovement.cpp` (`.text`
// 0x8012878C..0x8012CA24), so this address sits in that unit's neighbourhood.  For an anonymous
// function that is the only evidence there is, and it beats a lane picking the directory it
// happened to own.  Note the boundary above is not contiguous - 0x60 bytes of gap hold
// `fn_8012862C` - so this claim cannot start at the `auto_*` unit's own 0x801285DC without
// swallowing retail's work.

/** 0x802860BC, retail's own name for it: `CollisionUtil::AddAverageToFront(const
 *  CCollisionInfoList&, CCollisionInfoList&)`.  Named by `symbols.txt`, defined by
 *  `src/Collision/CollisionUtil.cpp:871`; declared here, never defined here. */
extern void AddAverageToFront__13CollisionUtilFRC18CCollisionInfoListR18CCollisionInfoList(
    const void* in, void* out);

/** 0x8012862C, `symbols.txt:5010`, size 0x160: the nearest-slope search `fn_80128604` forwards to.
 *  Unnamed in retail and not claimed here, so dtk's own `auto_*` object supplies the bytes in the
 *  DOL link and the one `bl` resolves to retail's address.  Declared, never defined here; the port's
 *  flat link has no `auto_*` objects, so `src/MetroidPrime/PortGlobals.cpp` carries an announcing
 *  stand-in for it, exactly as it does for `fn_8028139C`. */
extern void fn_8012862C(const void* list, void* out);

void fn_80128604(void* unused, const void* in, void* out);
void fn_801285DC(void* unused, const void* in, void* out);

void fn_80128604(void* unused, const void* in, void* out) { fn_8012862C(in, out); }

void fn_801285DC(void* unused, const void* in, void* out) {
  AddAverageToFront__13CollisionUtilFRC18CCollisionInfoListR18CCollisionInfoList(in, out);
}