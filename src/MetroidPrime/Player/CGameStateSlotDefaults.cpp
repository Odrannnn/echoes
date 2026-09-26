/**
 * `fn_80142DD4` - retail `.text:0x80142DD4`, `size:0x94` = 148 bytes, 0x80142DD4..0x80142E68.
 * The next symbol starts at 0x80142E68, so that is the exact end of the range this unit claims.
 *
 * `CGameState::CGameState()` calls it three times, at 0x80144C64/0x80144C70/0x80144C7C, as
 * `fn_80142DD4(this, i)`. It is the same body as `fn_80142CF8` (`CGameStateSysOptsPutTo.cpp`) with
 * an index in front of it: write 32 default bytes into the *i*-th 16-byte block of the `x144`
 * member, then serialise `gameOptions` into it through a `CMemoryStreamOut` and a
 * `CBitStreamWriter`.
 *
 *     80142ddc  addi  r5,r13,-31139   0x8041FD80 - 31139 = 0x804183DD = `lbl_804183DD`
 *     80142de4  slwi  r0,r4,4          the index, times 16
 *     80142de8  li    r4,32            the byte count
 *     80142df8  add   r31,r30,r0
 *     80142dfc  addi  r31,r31,328      `this + 0x148 + i*16`
 *     80142e04  bl    0x80142ba4       fill it with 32 copies of that byte
 *     80142e08  lwz   r4,12(r31)       the block's data pointer
 *     80142e1c  bl    0x80300070        CMemoryStreamOut(local, data, 32, kOS_NotOwned, 4096)
 *     80142e28  bl    0x80342d80        CBitStreamWriter(local, stream)
 *     80142e34  bl    0x801615ec        CGameOptions::PutTo(this + 0x80, writer)
 *     80142e40  bl    0x80342d30        ~CBitStreamWriter(local, -1)
 *     80142e4c  bl    0x802fff4         ~CMemoryStreamOut(local, -1)
 *
 * ## `this + 0x148 + i*16` is a *named* member, not an offset
 *
 * `SGameStateSlots` in `include/MetroidPrime/Player/CGameStateBlocks.hpp` is
 * `{ int x00_count; SGameStateBlock x04_blk[3]; }`, and `CGameState::x144` is one of them, so
 * `&self->x144.x04_blk[idx]` is that address with no offset arithmetic left in the source - which
 * is what keeps the `slwi`/`add`/`addi` a plain array index. The header's comment on `x144` is
 * the measurement that fixes the extent: `0x144 + 0x34 == 0x178`, and the constructor's three
 * calls are the reason the stride is 16 and the base is 0x148 and not 0x144.
 *
 * Two more things are not logic, and both are in `CGameStateSysOptsPutTo.cpp`'s header at more
 * length: `lbl_804183DD` is a one-byte `.sdata` object used **by address** (the relocation is
 * `R_PPC_EMB_SDA21 lbl_804183DD` in `build/G2ME01/obj/auto_03_80142A30_text.o`) whose 32 default
 * bytes are all zero, and the two temporaries are stack objects whose destruction order is scope
 * order - the writer must be the inner one, which it is because its constructor takes a reference
 * to the stream.
 *
 * ## The register allocation
 *
 * `self` is in `r30` and the block pointer in `r31`, both callee-saved, because `self` is needed
 * again at 0x80142E2C (`addi r3,r30,128`) after two calls while the block pointer is not. The
 * epilogue restores `r30` before `r31`, which is mwcceppc's own order and the reason the frame is
 * `-160(r1)`: 160 bytes is the 0x84-byte `CMemoryStreamOut` at 20(r1) plus slack.
 *
 * **Not in `files.cmake`, measured** - see this file's entry in `tools/check_files_cmake.py`.
 */
#include "types.h"

#include "Kyoto/Streams/CMemoryStreamOut.hpp"
#include "Kyoto/Streams/CBitStreamWriter.hpp"

#include "MetroidPrime/Player/CGameState.hpp"
#include "MetroidPrime/Player/CGameOptions.hpp"
#include "MetroidPrime/Player/CGameStateBlocks.hpp"

// `lbl_804183DD`, `.sdata:0x804183DD`, `size:0x1 data:byte`, used by address. **Not `const`** -
// see `CGameStateSysOptsPutTo.cpp`'s header for the measurement: a `const` declaration puts the
// object in the read-only small-data area and the address comes out as a `lis`/`addi` pair with
// `R_PPC_ADDR16_HA`/`R_PPC_ADDR16_LO` instead of the single `R_PPC_EMB_SDA21` retail has.
extern "C" unsigned char lbl_804183DD;

// `fn_80142BA4`, retail 0x80142ba4. Same function `CGameStateSysOptsPutTo.cpp` calls, with the
// default-byte object one lower in `.sdata`.
extern "C" void fn_80142BA4(SGameStateBlock* self, int count, const unsigned char* src);

extern "C" {
void fn_80142DD4(CGameState* self, int idx) {
  // **The array base goes into its own local, and that is what fixes the register allocation.**
  // Written as one expression - `&self->x144.x04_blk[idx]` - mwcceppc folds the constant first
  // and emits `addi r31,r4,328 ; add r31,r30,r31`, i.e. `self + (idx*16 + 0x148)`. Retail emits
  // `slwi r0,r4,4 ; add r31,r30,r0 ; addi r31,r31,328`, i.e. `(self + idx*16) + 0x148`, and it
  // puts the scaled index in `r0` - the register `mflr` has just vacated - rather than
  // overwriting the incoming argument in place. Taking the base first makes the strength
  // reduction come out the other way round. Measured both ways: 83.05% -> 100.00%, and the
  // object is 0x94 bytes either way, so nothing but the allocator differs.
  SGameStateBlock* blocks = self->x144.x04_blk;
  SGameStateBlock* block = blocks + idx;
  fn_80142BA4(block, 32, &lbl_804183DD);

  CMemoryStreamOut stream(block->x0c_data, 32, CMemoryStreamOut::kOS_NotOwned, 4096);
  CBitStreamWriter writer(stream);
  self->gameOptions.PutTo(writer);
}
} // extern "C"
