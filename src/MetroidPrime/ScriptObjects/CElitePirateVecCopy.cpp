// CElitePirateVecCopy.cpp - a carve of ElitePirate's (module 15) `.text
// 0x0000C02C..0x0000C094`: one function, `fn_15_C02C`, 0x68 = 104 bytes, 26 instructions. It sits
// inside the module's unclaimed `auto_00_00000178_text` run (0x178..0xC0D0) and is carved out here
// as its own unit, so `build/report.json` counts it instead of leaving it retail bytes.
//
// **What it is.** `rstl::uninitialized_copy` over this module's 0x68-byte record: one walk that
// copy-constructs every element through this copy's own `fn_15_32C0` and returns the new end.
// That reading is measured off the module's own disassembly
// (`build/G2ME01/ElitePirate/asm/auto_00_00000178_text.s`), and the two arguments around it pin it
// down:
//
//   * The stride is in the bytes twice and it is 0x68 both times. `fn_15_BF6C` (.text 0xBF6C, the
//     block's own `reserve`) allocates `mulli r3,r28,0x68` / `bl allocate__Q24rstl17rmemory_allocatorFi`
//     and walks the old buffer `addi r30,0x68` / `addi r31,0x68` per element
//     (`add r31,r30,r0` where `r0 = self->x04 * 0x68`); `fn_15_C02C` walks `addi r31,0x68` /
//     `addi r30,0x68`. Same 104.
//   * The copy it calls is at `.text:0x32C0` (`symbols.txt` line 234 for it is `fn_15_C02C`, line
//     233's neighbour): `fn_15_32C0` is 0x20 bytes and forwards to `fn_15_32E0` (0x28), which is
//     `if (dst == nullptr) return;` plus a `bl fn_15_3308` - the MWCC
//     `if (p) p->T::T(const T&)` shape, destination in r3 and source in r4.
//   * `fn_15_BF6C` calls it exactly once, from its "copy the live elements across" step
//     (`bl fn_15_C02C` at .text 0xBFCC), after materialising the two iterator temporaries and
//     before the destroy loop and the `Free__7CMemoryFPCv`. So `fn_15_C02C` **is referenced from
//     code inside the module**, which is why it needs no `force_active:` entry: the dead-strip
//     trap `ForgottenObject` and `ScriptCoin` hit is an orphan, and this is not one.
//
// **The element, read off `fn_15_3308` (0xC0 bytes, the copy constructor behind `fn_15_32C0`).**
// It copies, in ascending offset order: a `TUniqueId` (`lwz r5,0` / `lwz r0,4` -> `stw r5,0` /
// `stw r0,4`), two `bool`s at +8 and +9 (`lbz`/`stb` each), eight floats at +0x0C..+0x28 (two
// `CVector3f`), an `rstl::basic_string` at +0x2C (`addi r4,r31,0x2c` / `addi r3,r30,0x2c` / `bl
// __ct__Q24rstl66basic_string<...>::basic_string(...)`), a `u16` at +0x3C (`lhz`/`sth`) and a float
// at +0x40, then a `CMatrix3f` at +0x44 (`addi r4,r31,0x44` / `addi r3,r30,0x44` / `bl
// __ct__9CMatrix3fFRC9CMatrix3f`). 0x44 + 0x24 = 0x68, which is the stride. The block holding
// them is the same three words at the same offsets as `rstl::vector`'s: `x04` is the live count,
// `x08` the capacity `fn_15_BF6C` compares with a signed `cmpw`, `x0c` the base pointer. Its
// destroy loop calls `internal_dereference__Q24rstl66basic_string<...>` on `elem + 0x2C`, which is
// the string read out above, and then `Free__7CMemoryFPCv` on `x0c`. `fn_15_36CC` fills the block
// with `fn_15_BF6C(&block, 0x0e)` and then `fn_15_3424(self, lbl_15_rodata_0, 3, &block, ...)`.
//
// **The record type is not declared, and that is deliberate.** The twin this copy is written from
// - `fn_801FF6B8` (`src/MetroidPrime/ScriptObjects/Carve801FF5A0.cpp`, Matching) - also reaches its
// element as `char*` and its element copy as an `extern "C"` name; a struct with a real copy
// constructor would make mwcceppc emit a weak inline copy of it into this object, which the
// 0xC02C..0xC094 claim has no room for and which `tools/unit_fit.sh` would then list as a function
// retail's object does not define. Nothing here reads an offset, so the unit stays
// layout-immune; the layout above is recorded as the evidence, not as a declaration.
//
// **The declaration that produced the shape** (the thing to copy, per the item): the two iterators
// arrive **by value, as one-word classes**, and that is what the bytes are:
//
//   0000C02C  stwu r1,-0x20(r1)
//   0000C030  mflr r0
//   0000C034  stw  r0,0x24(r1)
//   0000C038  stw  r31,0x1c(r1)
//   0000C03C  lwz  r31,0(r3)        <- begin, the address of the caller's temporary
//   0000C040  stw  r30,0x18(r1)
//   0000C044  mr   r30,r5           <- dst
//   0000C048  stw  r29,0x14(r1)
//   0000C04C  mr   r29,r4           <- end, kept by address
//   ...
//   0000C068  lwz  r0,0(r29)        <- the bound is RE-READ every iteration
//   0000C06C  cmplw r31,r0
//   0000C070  bne  .L
//
// `Carve801FF5A0.cpp` records the same thing from the other side and it is worth restating: the
// reload at the loop head is why the bound must not be hoisted into a local, and the two-word
// temporaries `fn_15_BF6C` builds for its own two arguments (`stw r6,0x8` / `stw r6,0xc` for `end`
// at `addi r4,r1,0xc`, `stw r0,0x10` / `stw r0,0x14` for `begin` at `addi r3,r1,0x14`) are the
// callee side of the same ABI. `SStateIter` in that file is the declaration; this is that
// declaration with a 104-byte stride and this module's callee. Written as plain C the shape still
// holds - `Carve801FF5A0.cpp` measured `fn_801FF6B8` byte-exact in both modes - but the unit is
// C++ because `Carve80213320.cpp`'s copy of the same shape has to be, and a C++ definition keeps
// one set of conventions across the family.
//
// The name is retail's own `fn_15_<off>` string, so `config/G2ME01/rels/ElitePirate/symbols.txt`
// needs no rename, and the definition is `extern "C"` so it stays unmangled: that file carries the
// placeholder and objdiff pairs by name.
//
// Definitions are in descending retail text order (only one here), because mwcceppc emits them in
// reverse source order and mwldeppc keeps the object's `.text` order verbatim - ascending would
// permute the module's bytes with objdiff still at 100% and the module hash breaking. Checked with
// `python3 tools/check_decl_order.py --unit MetroidPrime/ScriptObjects/CElitePirateVecCopy.cpp`.
//
// **This unit needs `mw_version="GC/2.7"`, and that is half of what makes it match.** REL objects
// default to GC/1.3.2 (`configure.py`'s `Rel(...)`), and GC/1.3.2 emits 25 of these 26 words and
// sinks the 26th - the `lwz r31,0(r3)` that reads the by-value `begin` - to sixth place, after the
// three callee-save `stw`/`mr` pairs, where retail has it second. GC/2.7 emits it second and every
// other word identically. Same per-object override `CLumiteRelTail.cpp`, `CSandBossRelTail.cpp` and
// `CGameOptions.cpp` use, and for the same reason (a scheduler-order difference between the two
// code generators, nothing else). **The other 24 copies of this shape in 24 modules should expect
// it too**: check which compiler reproduces retail's prologue before writing any of them, because
// under the default REL compiler this shape is 25 of 26 words and reads as a source problem.
//
// **`+0x68` on both cursors is not written by hand in the twins.** mwcceppc's induction-variable
// walk turns `++ptr` on a `T*` into `addi rPtr,rPtr,sizeof(T)`, which is why both matched twins
// stride by `sizeof` their element (`fn_801FF6B8` by 36, the `rstl` instantiation at 0x80137584 by
// 104) even though `rstl::pointer_iterator::operator++` is spelled `++this->current` on a plain
// `T*` (`include/rstl/pointer_iterator.hpp:78`). Measured with a scratch probe compiled with this
// unit's exact command line: a one-word iterator over a 0x68-byte struct emits `addi r31,r31,0x68`
// and `addi r30,r30,0x68`. Below the record is reached as `char*`, so the stride **is** the literal
// 104 - byte-identical output, and it is what the sibling `Carve801FF5A0.cpp` does.

namespace {

/** The one pointer of the block's record iterator, `current`.  What the bytes need of the class is
 *  exactly this: a one-word class passed **by value**, built by a converting constructor - which
 *  is what makes `fn_15_BF6C` materialise the two-word temporaries the `lwz r31,0(r3)` and
 *  `lwz r0,0(r29)` above then read. */
struct SElitePirateVecIter {
  void* mCur;
  SElitePirateVecIter(void* cur) : mCur(cur) {}
};

} // namespace

extern "C" {

// .text 0x32C0, 0x20 bytes, unclaimed: this module's own copy constructor for the 0x68-byte record,
// forwarding to `fn_15_32E0`.  Called with the destination first (`mr r3,r30` then `mr r4,r31`).
// It lives in the module's own unclaimed code, so it stays retail's and is declared, not defined.
//
// Listed in `files.cmake` for the reason `CMetareeSwarmDes.cpp` and `DigitalGuardianVecList.cpp`
// give: this unit defines no RELMain/RELExit, so it does not collide in a flat link, and the one
// relocation outside itself is behind the guard the port cannot resolve - `fn_15_32C0` is a module
// code address with no PC-side definition.  `powerpc-eabi-nm -u` on the host object therefore
// prints nothing and the port's undefined count does not move.
#ifdef __MWERKS__
void fn_15_32C0(void* dst, const void* src);

// .text 0xC02C, 0x68 bytes.  `uninitialized_copy`: one 0x68-strided walk that copy-constructs each
// record and returns the new end, which is what `fn_15_BF6C` leaves in r3.  The bound is re-read
// from `end` every iteration, so it must not be hoisted into a register.
void* fn_15_C02C(SElitePirateVecIter begin, SElitePirateVecIter end, void* dst) {
  SElitePirateVecIter cur = begin;
  char* out = static_cast< char* >(dst);
  for (; cur.mCur != end.mCur; cur.mCur = static_cast< char* >(cur.mCur) + 104, out += 104) {
    fn_15_32C0(out, static_cast< const void* >(cur.mCur));
  }
  return out;
}
#endif

} // extern "C"
