// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the addresses
// and sizes come from `config/G2ME01/symbols.txt:7855-7857`, the instructions are the ones dtk
// emitted into `build/G2ME01/asm/auto_03_801E782C_text.s` while this range was still unclaimed
// (that file and `auto_03_801E7CB0_text.s` are the two runs left either side of the claim), and
// the bodies below are the C those bytes are the compilation of.  `build/G2ME01/asm/MetroidPrime/
// Cameras/Carve801E7C14.s` is this unit's own generated listing, not retail's - it can confirm,
// never establish, what retail had.
//
// .text 0x801E7C14..0x801E7CB0, 0x9C = 156 bytes, 3 functions:
//
//   fn_801E7C14    0x801E7C14  0x20     8 instructions
//   fn_801E7C34    0x801E7C34  0x24     9 instructions
//   fn_801E7C58    0x801E7C58  0x58    22 instructions
//
// **What the three are: the tail of a deleting-destructor chain**, each step calling the next.
// Retail names none of them, so this is read off the call edges and the argument registers:
//
//   fn_801E7C14  a frame, one unconditional `bl fn_801E7C34` with r3 forwarded untouched, and an
//                epilogue - no load, no test, no return value.  That is the `destroy`-shaped
//                forwarder whose whole body is the call.  Its twin is `__sys_free`
//                (0x80008A28, 0x20 = 32 bytes, `src/MetroidPrime/main.cpp:396`), the same eight
//                instructions with `bl Free__7CMemoryFPCv` in place of this `bl`.
//   fn_801E7C34  a frame, `li r4,-1`, and one `bl fn_801E7C58`: the second step of the same
//                chain, taking nothing else from its own argument.  **The twin is exact** -
//                `destroy_impl< 11CTweakValue >__4rstlFP11CTweakValue` (0x80006850, 0x24 = 36
//                bytes, `symbols.txt:131`) is these nine instructions word for word, including
//                the schedule `li r4,-1` before `stw r0,0x14(r1)`, with `bl
//                __dt__11CTweakValueFv` in place of `bl fn_801E7C58`.
//   fn_801E7C58  the destructive step: `mr. r30,r3 / beq` guards the receiver, `li r4,-1` says
//                "do not free me afterwards", the member at +0xC is destroyed through
//                `__dt__17CCameraShakerDataFv`, and only a positive flag - re-tested with
//                `extsh. r0,r31 / ble` - reaches `Free__7CMemoryFPCv(self)` before the receiver is
//                returned in r3.  **Its twin is exact too**:
//                `__dt__Q212CPlayerState16SPersistentStateFv` (0x80009508, 0x58 = 88 bytes,
//                `symbols.txt:199`, `src/MetroidPrime/main.cpp:2208`) is these 22 instructions with
//                `bl __dt__Q24rstl81vector<...>Fv` in place of the call here.  The flag is a
//                **short** (the `extsh.`, not the `extsb.` a `bool` gives) and `if (flag > 0)` sits
//                **inside** `if (self)`, so the receiver's `beq` lands on the epilogue.
//
// **The object at +0xC is a `CCameraShakerData`.**  `__dt__17CCameraShakerDataFv`
// (`symbols.txt:3093`, 0x8009D174, 0x70 = 112 bytes, `scope:weak`) calls `bl
// __dt__11CMayaSplineFv` on `addi r3,r30,{0xa0,0x5c,0x18}` in that order - the three
// `CMayaSpline` members at +0x18/+0x5C/+0xA0 of
// `include/MetroidPrime/Cameras/CCameraShakerData.hpp`, whose `CHECK_SIZEOF` is 0xf4.  So +0xC is
// not that class's own offset: the receiver here is a class holding one at +0xC, and 0xC + 0xF4 =
// 0x100 plus the byte fn_801E7CB0 copies at +0x100 is the 0x104 stride the loops below step by.
//
// The three call sites, all in the same unclaimed run and none of them ours:
//
//   0x801E7BD0  in `fn_801E7B5C` (0x801E7B5C, 0xB8 = 184 bytes), on the element a 0x104-stride
//               move-down walk has just left at the end of the array.
//   0x801E7DB4  in `fn_801E7D88` (0x801E7D88, 0x60 = 96 bytes), the count-at-+0 /
//               first-element-at-+4 walk: `mr r3,r31 / bl fn_801E7C14 / addi r31,r31,0x104`
//               against `lwz r0,0x0(r29) / cmpw r30,r0 / blt`, so the element size is the stride.
//   0x801E7FAC  in `fn_801E7EC0` (0x801E7EC0, 0x120 = 288 bytes, `CCameraShakeManager`'s
//               add-a-shake entry `include/MetroidPrime/CCameraShakeManager.hpp:11`), the one
//               direct call to fn_801E7C58: on the stack temporary at `r1+0xFC` with `li r4,-1`.
//
// Its own unit because a claim may not span an unclaimed gap: `fn_801E7B5C` ends exactly where
// this claim begins, and `fn_801E7CB0` (0x801E7CB0, 0x64 = 100 bytes) begins exactly where it
// ends.  What is left of the auto range either side is `auto_03_801E782C_text`
// (0x801E782C..0x801E7C14) and `auto_03_801E7CB0_text` (0x801E7CB0..0x801E8AEC).
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object `.text` verbatim, so an
// ascending file is a permuted `.text` - 100.00% per function and a broken DOL.  Only
// `tools/flip_test.sh` catches that.
//
// Retail names none of these, so `symbols.txt` carries the `fn_<addr>` placeholder and this file
// reproduces that symbol verbatim, which is why the definitions have to stay C: a C++ one would
// mangle to `_Z<len>fn_<addr>v` and objdiff would pair nothing.
//
// The directory is from the nearest claimed range: this address is inside the run
// `MetroidPrime/Cameras/CCameraShakerData.cpp` claims up to 0x801E782C and directly precedes the
// `MetroidPrime/Cameras/` neighbourhood's `CCameraShakeManager` code, so
// `MetroidPrime/Cameras/` is the neighbourhood.  For an anonymous function that is the only
// evidence there is, and it beats a lane picking the directory it happened to own.

/** 0x8009D174, `symbols.txt:3093`, 0x70 = 112 bytes, `scope:weak`: the deleting destructor of
 *  `CCameraShakerData`.  It destroys the three `CMayaSpline` members at +0x18/+0x5C/+0xA0 and
 *  frees its own receiver behind its own `extsh.` flag test.  Supplied by
 *  `MetroidPrime/TypesMatch.cpp` in the DOL, which owns the bytes; declared, never defined here
 *  for the matching build.  See the port-only stand-in at the end of this file. */
extern void __dt__17CCameraShakerDataFv(void* self, int flag);

/** 0x802CE388, `symbols.txt:12992`: retail's `CMemory::Free(void const*)`, size 0x64.  Claimed by
 *  `Kyoto/Alloc/CMemory.cpp` in the DOL and defined under this name for the host link by
 *  `src/Kyoto/Alloc/PortMwccNew.cpp:34`.  Declared, never defined here. */
extern void Free__7CMemoryFPCv(const void* ptr);

void* fn_801E7C58(void* self, short flag);
void fn_801E7C34(void* self);
void fn_801E7C14(void* self);

/** `fn_801E7C58` - retail `.text:0x801E7C58`, 0x58 = 88 bytes: the destructive step of the chain,
 *  and the only one that reads the object.  The whole body is the member at +0xC plus the flag
 *  test; the flag is a **short**, which is what `extsh. r0,r31` compiles from. */
void* fn_801E7C58(void* self, short flag) {
  if (self) {
    __dt__17CCameraShakerDataFv((unsigned char*)self + 12, -1);
    if (flag > 0) {
      Free__7CMemoryFPCv(self);
    }
  }
  return self;
}

/** `fn_801E7C34` - retail `.text:0x801E7C34`, 0x24 = 36 bytes: a frame, an `li r4,-1` and one call
 *  to fn_801E7C58 - the middle step, word for word `destroy_impl< 11CTweakValue >`'s nine
 *  instructions but for the `bl` target. */
void fn_801E7C34(void* self) { fn_801E7C58(self, -1); }

/** `fn_801E7C14` - retail `.text:0x801E7C14`, 0x20 = 32 bytes: a frame and one call to fn_801E7C34
 *  and nothing else - the head of the chain.  Same eight instructions as `__sys_free`
 *  (`src/MetroidPrime/main.cpp:396`) but for the `bl` target; 0x801E7BD0 and 0x801E7DB4 are its
 *  two call sites inside this run. */
void fn_801E7C14(void* self) { fn_801E7C34(self); }

#ifndef __MWERKS__
// Port-only stand-in, empty body, and it **is** one: `CCameraShakerData`'s destructor is not
// decompiled here (its own 0x70 retail bytes are a spelling job of their own).  It exists so the
// host link resolves the `bl` above, and it is the same trade `src/MetroidPrime/PortLinkStubs.cpp`
// makes for its 162 symbols - an announced stand-in, kept beside the one reference that asks for
// it.  The host has no other definition: `__dt__17CCameraShakerDataFv` sits inside
// `MetroidPrime/TypesMatch.cpp`'s claim (`.text` 0x800972BC..0x8009D644) and that unit is a
// deliberate `files.cmake` exclusion (`tools/check_files_cmake.py:136` - it does not build on the
// host), so nothing else in the port names this symbol.  Measured twice in this tree with
// `tools/link_check.sh --strict`: with this block it prints `unique undefined symbols 291` and
// `STRICT PASS - regression gate: 291 undefined against a baseline of 291 (no growth), 0
// duplicate(s), 0 compile error(s), linker_ran=1`; with the block's guard forced false the same
// run prints `292 ... (GREW)` and its list of added gap symbols names
// `NEW  __dt__17CCameraShakerDataFv`.
// The guard is `__MWERKS__`, not `TARGET_PC`, to match the port-only blocks of
// `src/MetroidPrime/ScriptObjects/CUnknown90.cpp` and `src/MetroidPrime/CWorldSaveGameInfo.cpp`:
// the matching build must take the symbol from TypesMatch.cpp's object, and a second definition
// there would be a duplicate.
void __dt__17CCameraShakerDataFv(void* self, int flag) {
  (void)self;
  (void)flag;
}
#endif
