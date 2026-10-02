// Carved out of an unclaimed dtk `auto_*` range by lane 1, goal item `carve-800e9c14`.
// Every number here is measured: the address and size come from the range this claim names
// in `config/G2ME01/splits.txt`, the instructions are the ones dtk itself emitted into
// `build/G2ME01/asm/auto_03_800E9C14_text.s`, and the body below is the C those bytes are
// the compilation of.
//
// .text 0x800E9C14..0x800E9C1C, 0x8 = 8 bytes, 1 function:
//
//   fn_800E9C14    0x800E9C14  0x8    lwz r3, 0x2c8(r3) / blr
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits
// function definitions in *reverse* source order and mwldeppc keeps the object `.text`
// verbatim, so an ascending file is a permuted `.text` - 100.00% per function and a broken
// DOL.  Only `tools/flip_test.sh` catches that.  With one function the order cannot bite,
// and the rule is kept so that adding a second one later is already right.
//
// Retail names none of these.  `powerpc-eabi-nm build/G2ME01/main.elf` has
// `800e9c14 T fn_800E9C14`, the `fn_<addr>` placeholder, so the definitions have to stay C:
// a C++ one would mangle to `_Z<len>fn_<addr>Pv` and objdiff would pair nothing.  That is
// also why the unit is a `.c` rather than a `.cpp`.
//
// Its own unit because a unit may not claim two discontiguous ranges in one section
// (dtk `dol split` fails with "Cyclic dependency ... link order"), and because the
// functions on either side of this run are not trivial.
//
// The item's seeder found no twin - no already-matched function with this shape at this
// length - so the body is read off the two instructions.  The signature is measured, not
// guessed: there are six `bl fn_800E9C14` sites in `build/G2ME01/asm/`, two in
// `MetroidPrime/CGroundMovement.s`, two in `auto_03_80218E28_text.s`, one in
// `Player/CPlayerDynamics.s` and one in `Player/CPlayer.s`.  Every one of them puts an
// object in r3, and every one of them uses the returned r3 as a pointer - `cmplwi r3, 0x0`
// at 0x8012A488, `mr. r28, r3` then `beq` at 0x80186580 - which is `void*` in and `void*`
// out.  A `lwz` returning the word directly is what a pointer-typed field load compiles to;
// there are 186 two-instruction `lwz r3, X(r3)` / `blr` accessors in the asm, 133 of them
// in claimed units, so the shape is retail's ordinary getter and not a special case.
//
// The directory is retail own, taken from the nearest claimed range on either side: the
// range below ends at 0x800E8E50 (`MetroidPrime/Carve800E8E4C.c`) and the range above
// starts 8 bytes past this one, at 0x800E9C1C (`MetroidPrime/CPhysicsActor.cpp`).  Both
// are `MetroidPrime/`, so that is the directory.  For an anonymous function that is the
// only evidence there is, and it beats a lane picking the directory it happened to own.
void* fn_800E9C14(void* self) {
    return *(void**)((char*)self + 0x2C8);
}