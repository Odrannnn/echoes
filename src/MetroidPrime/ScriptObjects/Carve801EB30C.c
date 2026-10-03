// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the addresses
// and sizes come from `config/G2ME01/symbols.txt:7907-7908`, the instructions are the ones dtk
// itself emitted into `build/G2ME01/asm/auto_03_801E8AF4_text.s:2792-2823` before the claim
// existed (the same range is now `build/G2ME01/asm/MetroidPrime/ScriptObjects/Carve801EB30C.s`),
// and the bodies below are the C those bytes are the compilation of.
//
// .text 0x801EB30C..0x801EB374, 0x68 = 104 bytes, 2 functions:
//
//   fn_801EB30C    0x801EB30C  0x48 = 72 bytes  18 instructions  `push_back` of a 0x20-strided array
//   fn_801EB354    0x801EB354  0x20 = 32 bytes   8 instructions  a frame and one unconditional `bl`
//
// **What the two are.**  Retail names neither, so the twins identify them, and the first twin is
// exact - same 18 instructions, same operand schedule, only the `bl` target differs:
//
//   fn_801EB30C  is `rstl::reserved_vector<SDSPStreamVoice, 4>::push_back(const SDSPStreamVoice&)`
//                (0x8033D030, `symbols.txt:15280`, `size:0x48`, spelled
//                `construct(data() + mCount, in); ++mCount;` at
//                `include/rstl/reserved_vector.hpp:57-60`).  Its twin's bytes, at
//                `build/G2ME01/asm/Kyoto/Audio/CDSPStreamManager.s:1713-1732`, are word for word
//                the 18 instructions here, `bl "construct<15SDSPStreamVoice>__4rstlFPvRC15SDSPStreamVoice"`
//                in place of `bl fn_801EB354`.  The receiver is counted at +0 and indexed at +4,
//                and `slwi r0, r0, 5` is the element stride: 32 bytes.
//   fn_801EB354  is the byte-shape twin of `fn_80004438` (`src/MetroidPrime/Carve80004438.c:97`,
//                0x80004438, 0x20), which is `rstl::destroy` - a frame, one unconditional `bl`,
//                no load, no test, no returned value, exactly as
//                `include/rstl/construct.hpp:92-95` spells it.  Here the same frame wraps
//                `fn_801EB374` instead.
//
// **The element and the array are measured from retail, not assumed.**  `fn_801EB374` (0x801EB374,
// `symbols.txt:7909`, 0x4C = 76 bytes) is the thing `fn_801EB354` calls, and its own bytes are
// `rstl::construct`'s: `cmplwi r3,0` / `beqlr` is the same null guard
// `construct<15SDSPStreamVoice>__4rstlFPvRC15SDSPStreamVoice` (0x8033D078) opens with, followed by
// a member-wise copy of the 0x20-byte element - a `lhz` at +0 and seven `lfs` at +4..+0x1C.
// `fn_801EB3C0` (0x801EB3C0, `symbols.txt:7910`, 0x3C) walks the same array with `addi r5, r5, 0x20`
// and compares that same 2-byte key at +0, and `RemoveSafeZone__16CSafeZoneManagerFRC9TUniqueId`
// (0x801EB1E8) indexes it with `slwi r0, r3, 5` plus `addi r4, r4, 0x4` and then calls
// `fn_801EABB4` on it.  So the receiver is the 0x40-entry, 0x20-strided array of `CSafeZoneManager`
// and this pair is its `push_back`.
//
// **The one call site is retail's and it cannot be dropped.**  `grep -rn 'bl fn_801EB30C'
// build/G2ME01/asm/` returns exactly one hit, 0x801EB2AC, inside `fn_801EB230` (0x801EB230,
// `symbols.txt:7906`, 0xDC), on `r27` with `r4 = r1 + 8` and behind `cmpwi r0, 0x40` / `bge` - the
// capacity check of that array.  `fn_801EB354` is called once, from `fn_801EB30C` at 0x801EB330,
// and `fn_801EB374` once, from `fn_801EB354` at 0x801EB360; `fn_801EB374` is therefore above this
// claim and is declared here, never defined.  Its 0x4C bytes are a spelling job of their own, which
// is why the claim stops where it does.  For the DOL dtk's own `auto_*` object defines it; for the
// port link it is the announced stand-in `stub_carve801eb30c_0` in
// `src/MetroidPrime/PortLinkStubs.cpp`, the same trade `stub_80004438_0` makes for
// `Carve80004438.c`'s callee.  Nothing here claims `fn_801EB374` is decompiled.
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object `.text` order verbatim, so
// an ascending file is a permuted `.text` - 100.00% per function, `unit_fit.sh` still saying
// "fits", the link still succeeding, and a broken DOL.  Only `tools/flip_test.sh` catches it.
// `fn_801EB354` (0x801EB354) is the *higher* address and therefore comes first here; the object
// then leads with `fn_801EB30C` at offset 0, which is where retail has it.
// `python3 tools/check_decl_order.py --unit MetroidPrime/ScriptObjects/Carve801EB30C` is the cheap
// check for the order.
//
// Retail names neither of these.  `symbols.txt:7907-7908` carries the `fn_<addr>` placeholders and
// this file reproduces those symbols verbatim, so the definitions have to stay C: a C++ one would
// mangle to `_Z<len>fn_<addr>v` and objdiff would pair nothing.  That is also why the unit is a
// `.c` rather than a `.cpp`.
//
// Its own unit because a claim may not span an unclaimed gap: below it,
// 0x801EB230..0x801EB30C (`fn_801EB230`, retail-named caller, 0xDC bytes) is unclaimed, and above
// it, 0x801EB374..0x801ECD8C (`fn_801EB374` .. `fn_801ECD8C`) is too.  The range sits inside dtk's
// `auto_03_801E8AF4_text`, whose lower part starts exactly at 0x801E8AF4 where
// `MetroidPrime/ScriptObjects/Carve801E8AEC.c` ends; the claim is in the *middle* of that auto
// object, not at either end of it, so no unit boundary is created and there is no dtk link-order
// cycle - see "The carve vein" in `docs/RUNNING_THE_DECOMP.md` for the case that does fail.  The
// directory is retail's own, taken from the nearest claimed range: below is
// `MetroidPrime/ScriptObjects/Carve801E8AEC.c` and above is `MetroidPrime/CActorField25.cpp`.

/** 0x801EB374, `symbols.txt:7909`, 0x4C = 76 bytes: the member-wise copy of one 0x20-byte
 *  safe-zone element, i.e. `rstl::construct`'s body.  Declared, never defined here; the port
 *  link's stand-in is `stub_carve801eb30c_0` in `src/MetroidPrime/PortLinkStubs.cpp`, an
 *  announced empty body.  `dest` is the element slot this unit computed and `src` is the
 *  aggregate `fn_801EB30C`'s caller built, both `void*` because retail names neither type. */
extern void fn_801EB374(void* dest, const void* src);

/** The receiver of `fn_801EB30C`, laid out from the three measurements above and from retail's own
 *  instructions: an `int` count at +0, then the elements from +4, 0x20 bytes each, capped at 0x40
 *  by the `cmpwi r0, 0x40` at 0x801EB29C.  `mData` is a `uchar[]` rather than an element array
 *  **on purpose**: that is what `rstl::reserved_vector`'s `mData`/`data()` are
 *  (`include/rstl/reserved_vector.hpp:23,67`), and it is what makes mwcceppc emit
 *  `slwi r0, r0, 5` ; `add r3, r31, r0` ; `addi r3, r3, 0x4` - the scaled index added to the
 *  receiver first and the +4 *after* it.  Spelled as an `Elem*` array or as
 *  `(unsigned char*)self + count * 0x20 + 4` the same 18 instructions come out with the constant
 *  folded into the index instead (`addi r3, r3, 4` before `add r3, r31, r3`), which is 3 of the
 *  18 instructions different; see the note on `fn_801EB30C` below. */
struct CSafeZoneArray {
  unsigned int mCount;
  unsigned char mData[0x20 * 0x40];
};

void fn_801EB354(void* dest, const void* src);
void fn_801EB30C(struct CSafeZoneArray* self, const void* src);

/** `fn_801EB354` - retail `.text:0x801EB354`, 0x20 = 32 bytes: 8 instructions, a 0x10 frame, the
 *  link register, one unconditional `bl fn_801EB374` and the epilogue.  No callee-saved register
 *  is touched, which is what says the second argument is not used after the call.  The twin is
 *  `fn_80004438` (`src/MetroidPrime/Carve80004438.c:97`), these 8 instructions word for word with
 *  a different `bl` target, and its own note records the same reading there: `rstl::destroy`'s
 *  whole body is the one call. */
void fn_801EB354(void* dest, const void* src) { fn_801EB374(dest, src); }

/** `fn_801EB30C` - retail `.text:0x801EB30C`, 0x48 = 72 bytes: the array's `push_back`, spelled
 *  as the twin spells it, `construct(data() + mCount, in)` then `++mCount`
 *  (`include/rstl/reserved_vector.hpp:57-60`).  The receiver is saved in r31 because its count
 *  word is read again *after* the call - retail reloads it (`lwz r3, 0x0(r31)`) rather than
 *  holding it, so this reads the member twice and keeps it in a local.  **The `data()` cast is
 *  load-bearing, not decoration**: `mData` is a `uchar[]` reinterpreted to `Elem*`, and that is
 *  what puts the `addi r3, r3, 0x4` *after* the `add r3, r31, r0` instead of folding the +4 into
 *  the scaled index.  Measured, all with this unit's own flags: `(struct ElemS*)self->data +
 *  self->count`, `self->data + self->count * 0x20` through a `uchar*` local, and
 *  `(struct ElemS*)((unsigned char*)self->data + self->count * 0x20)` are all **18/18 byte-exact**,
 *  while `&self->data[self->count]` on an `ElemS[0x40]` member, `(void*)(self->data +
 *  self->count * 0x20)` and `(unsigned char*)self + count * 0x20 + 4` are 18 instructions with 3
 *  of them differing - the same 3 either way, always the `addi`/`add` pair.  A local that holds
 *  the count across the call (`unsigned int n = self->count;`) costs r30 as well and is 19
 *  instructions, so retail does not have one. */
void fn_801EB30C(struct CSafeZoneArray* self, const void* src) {
  fn_801EB354((unsigned char*)self->mData + self->mCount * 0x20, src);
  self->mCount = self->mCount + 1;
}
