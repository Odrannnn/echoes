/**
 * `fn_80142CF8` - retail `.text:0x80142CF8`, `size:0x80` = 128 bytes, 0x80142CF8..0x80142D78.
 * The next symbol, `fn_80142D78` (0x80142D78, 0x5C), starts there, so that is the exact end of
 * the range this unit claims.
 *
 * `CGameState::CGameState()` calls it last, at 0x80144C84, on `this`. It is retail's
 * "serialise the current game options into a scratch buffer" step, and it is the same body as
 * `fn_80142DD4` (`CGameStateSlotDefaults.cpp`) minus the index - see that file for the register
 * facts, which are the same here.
 *
 *     80142d00  li    r4,32           the byte count
 *     80142d04  addi  r5,r13,-31137   0x8041FD80 - 31137 = 0x804183DF = `lbl_804183DF`
 *     80142d14  addi  r3,r31,376      `this + 0x178`
 *     80142d18  bl    0x80142ba4      fill the block with 32 copies of that byte
 *     80142d1c  lwz  r4,388(r31)      `this + 0x184` = the block's data pointer
 *     80142d30  bl   0x80300070       CMemoryStreamOut(local, data, 32, kOS_NotOwned, 4096)
 *     80142d3c  bl   0x80342d80       CBitStreamWriter(local, stream)
 *     80142d48  bl   0x801615ec       CGameOptions::PutTo(this + 0x80, writer)
 *     80142d54  bl   0x80342d30       ~CBitStreamWriter(local, -1)
 *     80142d60  bl   0x802fff4        ~CMemoryStreamOut(local, -1)
 *
 * ## The `.sdata` symbol must be declared **non-const**, or the address is wrong
 *
 * `lbl_804183DF` is a one-byte `.sdata` object, and it is its **address** that is used - the
 * relocation is `R_PPC_EMB_SDA21 lbl_804183DF`, read out of
 * `build/G2ME01/obj/auto_03_80142A30_text.o`, so retail materialises it as `addi r5,r13,-31137`
 * and not as a `lis`/`addi` pair. Declared `const`, mwcceppc puts the object in the *read-only*
 * small-data area instead, decides the address needs the general form, and emits
 * `lis r4,0 ; addi r5,r4,0` with `R_PPC_ADDR16_HA`/`R_PPC_ADDR16_LO` - four instructions where
 * retail has one, and the unit drops from 100% to 88.44%. **Measured both ways.** This is the
 * same finding `CGameStateMemcardCtor.cpp` records for the two globals it *reads*: there the
 * missing `const` is what forces the per-iteration reload, and here the missing `const` is what
 * keeps the address in `r13`'s small-data window. Same root cause, opposite symptom, so the rule
 * to carry is that a retail `.sdata` global's **C++ type has to put it back in `.sdata`** -
 * non-const - whatever the source does with it.
 *
 * The 32 default bytes it points at are all zero (the 0x804183D8..0x804183E0 run of single-byte
 * `.sdata` objects is all zero and four other functions take offsets into it:
 * `fn_80142D78` takes `lbl_804183DE`, `fn_80142DD4` takes `lbl_804183DD`, and so on).
 *
 * ## The two temporaries are stack objects, and the stream is declared first
 *
 * Retail builds `CMemoryStreamOut` at 20(r1) and `CBitStreamWriter` at 8(r1) out of a 160-byte
 * frame; `CBitStreamWriter` is 0xC bytes (`CHECK_SIZEOF` in its own header), so 8..19 is exactly
 * the writer and 20.. is the stream. Their **destruction order is scope order**, so the writer
 * has to be the inner one - which the source already is, because its constructor takes a
 * reference to the stream.
 *
 * `CGameState+0x178` is the `SGameStateBlock` that `include/MetroidPrime/Player/CGameState.hpp`
 * pins: `+0x184` is its `x0c_data`, four bytes below the `+0x188` that the constructor zeroes.
 *
 * **Not in `files.cmake`, measured** - see this file's entry in `tools/check_files_cmake.py`.
 */
#include "types.h"

#include "Kyoto/Streams/CMemoryStreamOut.hpp"
#include "Kyoto/Streams/CBitStreamWriter.hpp"

#include "MetroidPrime/Player/CGameState.hpp"
#include "MetroidPrime/Player/CGameOptions.hpp"

// `lbl_804183DF`, `.sdata:0x804183DF`, `size:0x1 data:byte`. **Not `const`** - see the header
// comment; `const` is worth 12 bytes of instruction stream here.
extern "C" unsigned char lbl_804183DF;

// `fn_80142BA4`, retail 0x80142ba4: `fn_80142914(self)` (reset), `fn_801465EC(self, count)`
// (reserve) and then `count` single-byte `push_back`s out of `src`. Its second argument is in
// `r4` and its third in `r5`, which is the order below.
extern "C" void fn_80142BA4(SGameStateBlock* self, int count, const unsigned char* src);

extern "C" {
void fn_80142CF8(CGameState* self) {
  fn_80142BA4(&self->x178, 32, &lbl_804183DF);

  CMemoryStreamOut stream(self->x178.x0c_data, 32, CMemoryStreamOut::kOS_NotOwned, 4096);
  CBitStreamWriter writer(stream);
  self->gameOptions.PutTo(writer);
}
} // extern "C"
