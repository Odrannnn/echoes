// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the addresses
// and sizes come from `config/G2ME01/symbols.txt:3900-3901`, the instructions are the ones dtk
// itself emitted into `build/G2ME01/asm/auto_03_800E028C_text.s:945-997`, and the bodies below
// are the C those bytes are the compilation of.
//
// .text 0x800E0EFC..0x800E0FAC, 0xB0 = 176 bytes, 2 functions:
//
//   fn_800E0F54    0x800E0F54  0x58    22 instructions   the twin, one `bl` target different
//   fn_800E0EFC    0x800E0EFC  0x58    22 instructions   the twin
//
// **Both are byte-shape twins of one already-`Matching` function, read out of its own source
// rather than guessed.**  The seed named `rstl::single_ptr<CGameGlobalObjects>::~single_ptr()` -
// `__dt__Q24rstl32single_ptr<18CGameGlobalObjects>Fv` at 0x80006AE0, 0x58 = 88 bytes, emitted by the
// `single_ptr<CGameGlobalObjects>` instantiation in `src/MetroidPrime/main.cpp` against retail's
// own `include/rstl/single_ptr.hpp:39` (`~single_ptr() { delete mPtr; }`).  Retail compiles it to
// the same twenty-two instructions as each of these, with only the two `bl` displacements
// different (they are address-relative), and `powerpc-eabi-objdump -d
// build/G2ME01/src/MetroidPrime/main.o` shows our own object emitting the same twenty-two for the
// twin - `build/report.json` has `main/MetroidPrime/main` at 98 of 99 functions matched, and
// `main/MetroidPrime/Factories/Carve80032674` at 3 of 3 and 100.00%.
//
// The shape is the MWCC deleting-destructor convention, and it is worth spelling out because it
// decides where every `if` goes:
//
//   `stwu r1,-0x10(r1)` / `mflr r0` / `stw r0,0x14(r1)` / `stw r31,0xc(r1)` / `mr r31,r4`
//     0x10 frame; r31 is the incoming deleting flag, halfword-wide (see below)
//   `stw r30,0x8(r1)` / `mr. r30,r3` / `beq` out
//     the receiver is null-checked with `mr.` so CR0 is free for the `beq` - this is the whole
//     of the guard, and it is why `if (self)` and nothing else opens the body
//   `lwz r3,0x0(r30)` / `li r4,1` / `bl <T's own destructor>`
//     `+0x00` is the pointee, and the `1` is retail's deleting flag (`delete mPtr`)
//   `extsh. r0,r31` / `ble` out
//     the flag is re-tested **as a signed halfword**, so `flag > 0` - not `!= 0`
//   `mr r3,r30` / `bl Free__7CMemoryFPCv`
//     the receiver itself is freed, not the pointee: `CMemory::Free` is the global
//     `operator delete` here
//   `mr r3,r30` in the epilogue
//     every path returns the receiver
//
// **The `if (flag > 0)` has to stay inside `if (self)`**, or the `beq` lands on the `extsh.`
// instead of on the epilogue and the function is a different length.  The sibling carve
// `src/MetroidPrime/Factories/Carve80032674.c:92-100` writes the same thing for retail's
// `fn_8003271C` and that unit is 100.00%.
//
// **The callees are this copy's own, and they are the only thing that differs from the twin.**
//
//   `fn_800E0EFC`  calls `__dt__21CDependencyGroupTokenFv` (0x800C07E0, `symbols.txt:3546`, 0x6C,
//     weak) - so `T` is `CDependencyGroupToken` and this copy is retail's
//     `rstl::single_ptr<CDependencyGroupToken>` destructor.  `nm build/G2ME01/main.elf` has it at
//     `800c07e0 W`, from dtk's `build/G2ME01/obj/MetroidPrime/Player/CMorphBall.o` (retail's
//     claim for that address, `splits.txt`), so the DOL link needs no stand-in.
//   `fn_800E0F54`  calls `fn_800E0FAC` (0x800E0FAC, `symbols.txt:3902`, 0x90 = 144 bytes), which
//     **no unit claims** - it is the byte immediately above this claim and stays in dtk's
//     `auto_03_800E028C_text.o`.  Declared, never defined here; the port's flat link gets the
//     announced empty stand-in `stub_carve800e0efc_0` in `src/MetroidPrime/PortLinkStubs.cpp`.
//     **Nothing here claims `fn_800E0FAC` is decompiled.**  Measured from
//     `build/G2ME01/asm/auto_03_800E028C_text.s:1000-1040` it is a 144-byte destructor for a
//     `CToken`-derived class (`bl __dt__6CTokenFv` at 0x800E100C): a member at +0x38 released
//     through `fn_800E103C`, a `bool` at +0x30 gating a `CQuitGameScreen*` at +0x34, then the
//     flag test and `Free`.  It is retail's own `bl` at 0x800E0F7C, so this carve cannot drop it.
//   both  call `Free__7CMemoryFPCv` (0x802CE388) under retail's own name, spelled as retail
//     spells it: MWCC's old mangling is `[A-Za-z0-9_]` only, so a C declaration names it verbatim
//     and needs no alias.  Same trick `src/MetroidPrime/Carve800E1548.c:77` uses.
//
// **Which two `single_ptr`s these are is measured, not inferred**, the same way
// `src/MetroidPrime/Factories/Carve80032674.c:36-51` measured its three.  Both are called as
// *member* teardowns with `li r4,-1`, never with the deleting flag, which is what a `.~member()`
// looks like:
//
//   `fn_800E0EFC`  at 0x800E0ECC from `fn_800E0E7C` (0x800E0E7C, 0x80) as
//     `addi r3,r30,0x4 ; li r4,-1`, and at 0x800E0ECC from `fn_800E0BE4` (0x800E0BE4, 0x240)
//     the same way - a `rstl::single_ptr<CDependencyGroupToken>` at +0x04 of that class.
//   `fn_800E0F54`  at 0x800E0D0C from `fn_800E0BE4` as `addi r3,r30,0x3c ; li r4,-1` - a
//     `rstl::single_ptr<T>` at +0x3C whose `T` is `fn_800E0FAC`'s class.
//
// Both callers live in dtk's `auto_03_800DFA60_text.o` and `auto_03_800E028C_text.o`, so neither
// `bl` is resolved by a `bl` we own; nothing here changes what they call.
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object `.text` verbatim, so an
// ascending file is a permuted `.text` - 100.00% per function and a broken DOL.  Only
// `tools/flip_test.sh` catches that.
//
// Retail names neither of these.  `symbols.txt` carries the `fn_<addr>` placeholder and this file
// reproduces those symbols verbatim, so the definitions have to stay C: a C++ one would mangle to
// `_Z<len>fn_<addr>v` and objdiff would pair nothing.  That is also why the unit is a `.c` rather
// than a `.cpp`.
//
// Its own unit because a claim may not span an unclaimed gap, and because the neighbours are not
// trivial.  The claim is exactly 0x800E0EFC..0x800E0FAC; the run below it is `fn_800E0E7C`
// (0x800E0E7C, 0x80) and the run above is `fn_800E0FAC` (0x800E0FAC, 0x90), both of which call
// into this claim, so neither can join it.  The whole claim sits inside `auto_03_800E028C_text`
// (0x800E028C..0x800E10EC), which is what keeps `dtk dol split` from reporting a link-order cycle
// against a neighbouring unit - the failure `docs/RUNNING_THE_DECOMP.md` records for a carve taken
// from the byte a `CFrustumPlanes.cpp` claim ends on.
//
// The directory is retail's own, taken from the nearest claimed ranges: below is
// `MetroidPrime/CActorParameters.cpp` (0x800DFA60..0x800E028C, `splits.txt:529`), above is
// `MetroidPrime/Carve800E10EC.cpp` (0x800E10EC..0x800E122C, `splits.txt:535`).  For an anonymous
// function that is the only evidence there is, and it beats a lane picking the directory it
// happened to own.

/** `CDependencyGroupToken::~CDependencyGroupToken()` under **MWCC's own mangling** (0x800C07E0,
 *  `symbols.txt:3546`, weak, 0x6C).  `nm build/G2ME01/main.elf` has `800c07e0 W
 *  __dt__21CDependencyGroupTokenFv`, so the DOL link resolves it from dtk's own object.  The port's
 *  host link cannot spell the name at all - a host compiler mangles the same destructor
 *  `_ZN21CDependencyGroupTokenD1Ev` - so the announced empty stand-in
 *  `stub_carve800e0efc_1` in `src/MetroidPrime/PortLinkStubs.cpp` answers it there.
 *  **The second parameter is retail's deleting flag and it is measured, not assumed:** retail emits
 *  `li r4, 1` at 0x800E0F20, and a one-parameter declaration drops that `li` (the lesson
 *  `docs/goal-notes/carve-8001fedc.md` paid for). */
extern void __dt__21CDependencyGroupTokenFv(void* self, int deleting);

/** 0x800E0FAC, `symbols.txt:3902`, 0x90 = 144 bytes: retail's unclaimed destructor that
 *  `fn_800E0F54` calls for the object its `single_ptr` holds.  Declared, never defined here; the
 *  DOL link gets dtk's own `auto_03_800E028C_text.o` copy and the port link gets the announced
 *  empty stand-in `stub_carve800e0efc_0`.  The second parameter is the deleting flag. */
extern void fn_800E0FAC(void* self, int deleting);

/** `CMemory::Free(void const*)` (0x802CE388) under retail's own name - MWCC's old mangling is
 *  `[A-Za-z0-9_]` only, so a C declaration names it verbatim and needs no alias.  It is the
 *  global `operator delete` retail's `delete mPtr` reaches, and
 *  `src/Kyoto/Alloc/CMemory.cpp` is `Matching`, so our own object supplies these bytes in the DOL
 *  link.  Declared, never defined here. */
extern void Free__7CMemoryFPCv(const void* ptr);

/** `rstl::single_ptr<T>`'s one word, retail's layout (`include/rstl/single_ptr.hpp:18`): the
 *  pointee at +0x00.  Neither function touches the object behind it, so it is modelled as the
 *  `void*` retail's `delete mPtr` sees. */
struct SCarve800E0EFCSinglePtr {
  void* x0_ptr;
};

void* fn_800E0F54(struct SCarve800E0EFCSinglePtr* self, short flag);
void* fn_800E0EFC(struct SCarve800E0EFCSinglePtr* self, short flag);

void* fn_800E0F54(struct SCarve800E0EFCSinglePtr* self, short flag) {
  if (self) {
    fn_800E0FAC(self->x0_ptr, 1);
    if (flag > 0) {
      Free__7CMemoryFPCv(self);
    }
  }
  return self;
}

void* fn_800E0EFC(struct SCarve800E0EFCSinglePtr* self, short flag) {
  if (self) {
    __dt__21CDependencyGroupTokenFv(self->x0_ptr, 1);
    if (flag > 0) {
      Free__7CMemoryFPCv(self);
    }
  }
  return self;
}