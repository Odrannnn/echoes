#ifndef _CGAMESTATEBLOCKS
#define _CGAMESTATEBLOCKS

#include "types.h"

// The two member shapes `CGameState`'s `+0x110` and `+0x144` blocks are built out of, named so
// that the functions that touch them can be written as C++ instead of as raw offsets.
//
// Every offset and size here is measured out of `build/G2ME01/main.elf`; nothing is recalled.
// The evidence, and what each row is pinned by:
//
//   SGameStateBlock, **16 bytes**: `+0x00` unknown, `+0x04` the element count, `+0x08` the
//   capacity, `+0x0C` the data pointer.
//     * `fn_801466F4` (0x801466F4), called on `CGameState+0x08`, indexes its data as
//       `data + count * 36` and grows it with `allocate__Q24rstl17rmemory_allocatorFi(8(this))`
//       - so the base pointer's `+0x04` is the count, `+0x08` the capacity and `+0x0C` the data.
//     * `fn_8014260C` (0x8014260C), also on `CGameState+0x08`, walks it as
//       `cursor = *(void**)(base+0x0C)` against `*(uint*)(base+0x04) + *(uint*)(base+0x04)*36`.
//     * `fn_80004AA0` (0x80004AA0), the block's own copy, reads `4(src)`, `8(src)` and
//       `12(src)`, writes the same three words on the destination and then memcpy's the bytes -
//       with `4(dst)` as a **byte** size (`srwi. r0,3` for the eight-bytes-at-a-time loop) and
//       `8(dst)` as a **byte** capacity.
//     * `fn_80142DD4` (0x80142DD4) reads `12(this + 0x148 + i*16)` for i = 0, 1, 2, which is the
//       `+0x0C` data pointer of the second block's third element.
//   So `x04_count` is an element count in the 36-byte case and a **byte** size in the byte-buffer
//   case; both are the same word and the difference is in what the code does with it, not in the
//   layout. `x00_unk` is read by nothing and written by nothing in the DOL.
//
//   `SGameStateSlots`, **0x34 = 52 bytes**: `x00_count` then three 16-byte elements.
//     * `fn_80144924` (0x80144924) is its constructor: `stw r4,0(r3)` writes the count and the
//       fill loop strides by 16 (`addi r31,r31,16` at 0x8014499C), so the element is 16 bytes.
//     * `fn_80142DD4` (0x80142DD4) confirms the second instance's extent: `slwi r0,r4,4;
//       add r31,r30,r0; addi r31,r31,328` is `this + 0x148 + i*16`, i = 0, 1, 2 - so the block at
//       `CGameState+0x144` runs `0x144 .. 0x178`, which is 0x34.
//     * `CGameState+0x110 + 0x34 == CGameState+0x144` exactly, so the first instance is the same
//       size without needing a second measurement.
struct SGameStateBlock {
  u32 x00_unk;    //!< +0x00: read and written by nothing in the DOL
  u32 x04_count;  //!< +0x04: the element count, or a byte size in the byte-buffer instance
  u32 x08_cap;    //!< +0x08: the capacity, in the same unit as x04_count
  void* x0c_data; //!< +0x0C: the data block
};
CHECK_SIZEOF(SGameStateBlock, 0x10)

struct SGameStateSlots {
  int x00_count;            //!< +0x00
  SGameStateBlock x04_blk[3]; //!< +0x04, stride 16
};
CHECK_SIZEOF(SGameStateSlots, 0x34)

// `fn_80142A10` - retail .text:0x80142A10, size:0x20 = 32 bytes. The copy constructor of the
// 16-byte element, and the only thing the fill loop calls. It forwards `r3` unchanged to
// `fn_80004D5C` and touches nothing itself, so its own 8 instructions are a frame and a tail call.
//
// `fn_80004D5C` (0x80004D5C, 0x24) is `if (this != nullptr) fn_80004AA0(this, src);` - the null
// test is on the *pointer*, and `fn_80004AA0` is the copy that reads `4/8/12` of the source. So
// the element copy is retail's `if (this)` copy constructor and the shape above is what it copies.
extern "C" void fn_80142A10(SGameStateBlock* self, const SGameStateBlock* src);

#endif // _CGAMESTATEBLOCKS
