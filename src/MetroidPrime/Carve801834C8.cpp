// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the addresses
// and sizes come from `config/G2ME01/symbols.txt:6431-6432`
// (`fn_801834C8 = .text:0x801834C8; // type:function size:0xAC` and
// `fn_80183574 = .text:0x80183574; // type:function size:0xD4`), the instructions are the ones dtk
// itself emitted into `build/G2ME01/asm/auto_03_80182830_text.s:943-1053`, and the bodies below
// are the C++ those bytes are the compilation of.  `tools/flip_test.sh` is what says so.
//
// .text 0x801834C8..0x80183648, 0x180 = 384 bytes, 2 functions:
//
//   fn_801834C8    0x801834C8  0xAC   43 instructions
//   fn_80183574    0x80183574  0xD4   53 instructions
//
// **Both are `rstl::vector` bodies, and both are twins of functions this tree already matches at
// 100.00%, differing only in their `bl` displacements.**  The twins are why the operands are read
// off the bytes rather than guessed:
//
//   fn_801834C8  `__ct__Q24rstl42vector<6CColor,Q24rstl17rmemory_allocator>FR12CInputStreamRCQ24rstl17rmemory_allocator`
//                (0x802CFA30, `build/G2ME01/asm/Kyoto/Particles/CColorElement.s:911-959`,
//                `Matching` 45/45) - `rstl::vector<T, rmemory_allocator>::vector(CInputStream&,
//                const rmemory_allocator&)` against `include/rstl/vector.hpp:46` and
//                `include/Kyoto/Streams/CInputStream.hpp:207-215`: the three zero stores are the
//                member initialiser list, `lwz r4,0x8(r4) / addi r0,r4,4 / stw r0,0x8(r29) /
//                lwz r31,0x0(r4)` is `in.ReadInt32()` (`CInputStream`'s read pointer is at +0x8,
//                below), the call right after it is `reserve(count)`, and the loop body is
//                `push_back_unsafe(in.Get<T>())`: the element constructor called with a stack
//                temporary at +0x8, then `slwi r3,r3,2 / stw r0,0x4(r28) / lwz r0,0x8(r1) /
//                stwx r0,r4,r3`, which is `mItems[mCount++] = tmp` with the 4-byte element.
//   fn_80183574  `reserve__Q24rstl47vector<10CRuleValue,Q24rstl17rmemory_allocator>Fi`
//                (0x801F6E2C, `build/G2ME01/asm/MetroidPrime/CRuleSet.s:1306-1366`,
//                `Matching` 53/53) - `rstl::vector<T, rmemory_allocator>::reserve(int)` against
//                `include/rstl/vector.hpp:166-180`: the `cmpw r31,r0 / ble` is
//                `if (newSize <= mCapacity) return;`, `slwi r3,r31,3 /
//                bl allocate__Q24rstl17rmemory_allocatorFi` is `mAllocator.allocate(newData,
//                newSize)` with this copy's 0x8-byte element, the copy loop with its
//                `cmplwi r4,0x0 / beq` is `uninitialized_copy` over `CRuleValue`-sized elements
//                (`include/rstl/construct.hpp:120-131`), the empty `cmplw` loop after it is
//                `destroy(begin(), end())` for a trivially destructible element, and
//                `bl Free__7CMemoryFPCv` is `mAllocator.deallocate(mItems)`.
//
// **What the element types are, and how far that is measured.**  `fn_801834C8`'s element is a
// 4-byte one and its caller settles which: the function that calls it four times,
// `fn_80182EC8` (0x80182EC8, 0x224 = the size of a `CWorldSaveGameInfo`, which
// `src/MetroidPrime/CWorldSaveGameInfo.cpp:6-9` already identifies as this unit's own
// constructor), calls `__as__Q24rstl45vector<9TEditorId,...>` and
// `__dt__Q24rstl45vector<9TEditorId,...>` either side of the first call (0x80182F94, 0x80182FA0),
// read off `build/G2ME01/asm/auto_03_80182830_text.s:553-563` before this claim took the range out
// of it.  `TEditorId` is one `uint` (`include/MetroidPrime/TGameTypes.hpp:38-39`), which is why
// the store is a single word, and `fn_801834C8` is therefore
// `rstl::vector<TEditorId>::vector(CInputStream&)`.
//
// `fn_80183574`'s element is 0x8 bytes (`slwi ...,3` twice: the allocation and both pointer
// walks), which is a measurement; *which* 8-byte type is not, and is left unasserted here.  Its
// two callers, `fn_80183308` (0x80183308, calls it at 0x80183360) and `fn_8018340C` (0x8018340C,
// at 0x80183450), both then walk a second vector's `mCount`/`mItems` at the same 0x8 stride, which
// is a vector-to-vector copy.  The neighbours here are `CWorldSaveGameInfo`'s members
// (`src/MetroidPrime/CWorldSaveGameInfo.cpp:34-73`), whose only 8-byte member is
// `SLayerState { TAreaId mArea; int mLayer; }` - a plausible reading of the bytes, not a claim.
//
// **Its one callee that this file does not define is `fn_8005F508`, and it is this unit's own
// shape.**  0x8005F508, `symbols.txt:1886`, 0xA4 = 164 bytes, unclaimed and still in dtk's
// `auto_03_8005xxxx`; disassembled out of `build/G2ME01/main.elf` it is the *same* `reserve` over
// a 4-byte element (`slwi r3,r30,2 / bl allocate__Q24rstl17rmemory_allocatorFi`, the same four
// home-slot stores at +0x8/+0xC/+0x10/+0x14, the same `Free` and the same two tail stores), with
// the copy loop lacking the `cmplwi` null test because a 4-byte element is copied as one word.
// So `fn_801834C8`'s `mr r4,r31 / bl fn_8005F508` is that vector's own
// `reserve(count)` - the same call its matched CColor twin makes, at
// `CColorElement.s:932` - and it is declared here, never defined, because it sits in an
// unclaimed `auto_*` range that this claim may not span.  The port link cannot spell that name
// either, so `stub_carve801834c8_0` in `src/MetroidPrime/PortLinkStubs.cpp` stands in for it.
//
// The other two callees resolve in both builds without a stand-in:
// `__ct__9TEditorIdFR12CInputStream` is the `TEditorId(CInputStream&)` declared at
// `include/MetroidPrime/TGameTypes.hpp:42` and defined for the host at
// `src/MetroidPrime/CWorldSaveGameInfo.cpp:24`, while the DOL gets it from dtk's own object
// (`nm build/G2ME01/main.elf`: `800e9bdc T __ct__9TEditorIdFR12CInputStream`); and
// `rstl::rmemory_allocator::allocate` is called through its own header, which spells it
// `allocate__Q24rstl17rmemory_allocatorFi` under mwcceppc and supplies retail's name to the host
// separately (`stub_179`, the arrangement `src/MetroidPrime/Carve801EF84C.cpp:70-76` uses).
// `Free__7CMemoryFPCv` (0x802CE388) is claimed by `Kyoto/Alloc/CMemory.cpp` in the DOL and
// defined for the host at `src/Kyoto/Alloc/PortMwccNew.cpp:39`.
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object `.text` verbatim, so an
// ascending file is a permuted `.text` - 100.00% per function and a broken DOL.  Only
// `tools/flip_test.sh` catches that; `python3 tools/check_decl_order.py --unit
// MetroidPrime/Carve801834C8.cpp`.
//
// Its own unit because a claim may not span an unclaimed gap.  The claim immediately below is
// `MetroidPrime/CRainSplashGenerator.cpp` (`.text` 0x80181750..0x80182830) and the one immediately
// above is `MetroidPrime/Player/CPlayerDynamics.cpp` (0x80183648..0x80189FD4), so
// 0x801834C8..0x80183648 is exactly these two functions and nothing else: `fn_80182EC8` starts at
// 0x80182EC8, and `EndGravityBoost__7CPlayerFR13CStateManager` (0x80183648, `symbols.txt:6433`)
// begins where the claim stops.  What is left of the run below is `auto_03_80182830_text`, now
// 0x80182830..0x801834C8.
//
// Retail names neither function: `symbols.txt` carries the `fn_<addr>` placeholder and this file
// reproduces those symbols verbatim, which is why the definitions are wrapped in `extern "C"` -
// a C++ one would mangle to `_Z<len>fn_801834C8<len>...` and objdiff would pair nothing.
//
// The directory is retail's own, taken from the nearest claimed ranges: the claim below is
// `MetroidPrime/CRainSplashGenerator.cpp` and the one above is `MetroidPrime/Player/CPlayerDynamics.cpp`,
// so this address sits in the `MetroidPrime/` neighbourhood.
//
// **The unit is a `.cpp` and not a `.c` because the bodies are written against the real headers** -
// `rstl::vector`'s own member names, `CInputStream::ReadInt32` and `TEditorId` - which is the same
// reason `src/MetroidPrime/ScriptObjects/Carve801FD52C.cpp` is one.  Nothing is instantiated that
// emits a symbol: `tools/unit_fit.sh MetroidPrime/Carve801834C8.cpp` reports `.text claimed 384
// ours 384 retail 384 fits` and `no extra functions: our object defines only what the retail unit
// object does`, and `build/report.json` has this unit `complete` at **2 / 2 functions, 100.00%**.
//
// Measured on this tree: `nm -S --defined-only` gives exactly `00000000 000000ac T fn_801834C8`
// and `000000ac 000000d4 T fn_80183574`, and a word-by-word comparison against the disc
// (`python3 tools/dol_read.py 0x801834C8 0x180`, **not** `build/G2ME01/main.elf`, which holds our
// own bytes once this unit is in the link) leaves **4 differing words of 96, and all four are the
// `bl` displacements** at 0x80183510, 0x80183524, 0x801835A4 and 0x80183620, which are
// relocations in an unlinked object.  `main.dol` is `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`,
// unchanged.

#include "Kyoto/Streams/CInputStream.hpp"
#include "MetroidPrime/TGameTypes.hpp"

#include "rstl/construct.hpp"
#include "rstl/pointer_iterator.hpp"
#include "rstl/rmemory_allocator.hpp"

extern "C" {

/** 0x8005F508, `symbols.txt:1886`, 0xA4 = 164 bytes: this vector's own
 *  `rstl::vector<TEditorId>::reserve(int)`, which `fn_801834C8` calls with the count it has just
 *  read (`mr r4,r31 / bl fn_8005F508` at 0x8018350C-0x80183510).  Unclaimed, in dtk's own
 *  `auto_03_8005xxxx` object, so the DOL link resolves it there.  Declared, never defined here -
 *  this claim may not span an unclaimed range - and stood in for on the host by
 *  `stub_carve801834c8_0`. */
void fn_8005F508(rstl::vector< TEditorId >* self, int count);

/** 0x802CE388, `symbols.txt:12992`, 0x64: `CMemory::Free(void const*)`, which retail's
 *  `rstl::vector::reserve` reaches as `mAllocator.deallocate(mItems)`
 *  (`include/rstl/rmemory_allocator.hpp:44-53`).  Claimed by `Kyoto/Alloc/CMemory.cpp` in the DOL;
 *  `src/Kyoto/Alloc/PortMwccNew.cpp:39` defines it for the host.  Declared under retail's own
 *  emitted spelling so the call needs no header. */
void Free__7CMemoryFPCv(const void* ptr);

/** The element of `fn_80183574`'s vector: 0x8 bytes, fixed by the `slwi r3,r31,3` that sizes its
 *  allocation at 0x801835A0 and by the `addi r5,r5,0x8 / addi r4,r4,0x8` stride of both of its
 *  walks.  Nothing here reads its fields and nothing is asserted about its class.  **The
 *  constructors are load-bearing**: a POD declaration of the same two words compiles to the same
 *  0xD4 bytes but with every live value in the copy loop one register too high - source `r6`
 *  where retail has `r5`, destination `r5` where retail has `r4`, end `r4` where retail has
 *  `r3` - and 15 of 53 instructions differ.  Declaring the two-word element with constructors
 *  gives it a non-trivial copy constructor, which is what retail's own element has and what
 *  `uninitialized_copy`'s placement `new (dest) T(src)` then lowers to retail's
 *  `cmplwi r4,0 / beq / lwz / stw / lwz / stw`; measured both ways, this run. */
struct SCarve801834C8Elem8 {
  int x00;
  int x04;
  SCarve801834C8Elem8() : x00(0), x04(0) {}
  SCarve801834C8Elem8(int a, int b) : x00(a), x04(b) {}
};

typedef rstl::vector< SCarve801834C8Elem8, rstl::rmemory_allocator > SCarve801834C8Vector8;

void fn_80183574(SCarve801834C8Vector8* self, int newSize);
void* fn_801834C8(rstl::vector< TEditorId >* self, CInputStream& in);

void fn_80183574(SCarve801834C8Vector8* self, int newSize) {
  if (newSize <= self->mCapacity) {
    return;
  }
  SCarve801834C8Elem8* newData;
  rstl::rmemory_allocator::allocate(newData, newSize);
  rstl::uninitialized_copy(self->begin(), self->end(), newData);
  rstl::destroy(self->mItems, self->mItems + self->mCount);
  Free__7CMemoryFPCv(self->mItems);
  self->mItems = newData;
  self->mCapacity = newSize;
}

void* fn_801834C8(rstl::vector< TEditorId >* self, CInputStream& in) {
  self->mCount = 0;
  self->mCapacity = 0;
  self->mItems = nullptr;

  const int count = in.ReadInt32();
  fn_8005F508(self, count);
  for (int i = 0; i < count; ++i) {
    TEditorId tmp(in);
    self->mItems[self->mCount++] = tmp;
  }

  return self;
}

} // extern "C"