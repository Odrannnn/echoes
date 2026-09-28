// The three `CGraphics` immediate-mode state setters that touch **only** the vertex descriptor,
// plus the descriptor itself.  Retail addresses and sizes, all read off
// `build/G2ME01/main.elf`:
//
//   CGraphics::StreamColor(uint)          StreamColor__9CGraphicsFUl          0x802C10C4  0x20
//   CGraphics::StreamColor(CColor const&) StreamColor__9CGraphicsFRC6CColor    0x802C10A0  0x24
//   CGraphics::StreamTexcoord(float,float)StreamTexcoord__9CGraphicsFff        0x802C0FBC  0x30
//
// **Why these three and not the rest of the family.** `StreamBegin`, `StreamVertex` and
// `StreamEnd` are also undefined on the port, and their bodies exist verbatim in
// `src/Kyoto/Graphics/DolphinCGraphics.cpp` (a `configure.py` `NonMatching` unit that
// `files.cmake` does not list). They cannot come with this file because each of them writes
// through a GX display-list pointer - `mVertexBuffer = VTX_BUFFER_ADDR`,
// `mpVtxBuffer->x = ...`, `FlushStream()` - and those pointers only exist once
// `StreamBegin` has handed out a real GX region. Bringing `StreamVertex` alone would make
// `CGraphics::UpdateVertexDataStream` a new undefined symbol, so it is net zero, and it would
// be a body that dereferences a null GX pointer. These three are the closed set: they read and
// write the descriptor and nothing else.
//
// ## The descriptor is retail's `lbl_80416EE0`
//
// `lbl_80416EE0 = .bss:0x80416EE0; size:0x30` (config/G2ME01/symbols.txt 19373) is exactly the
// 0x30 bytes the three bodies address, and the offsets fall out of the instructions and agree
// with the member order `DolphinCGraphics.cpp` already declares for the same struct:
//
//   +0x00 Vec   mPosition      +0x18 Vec2  mTexCoord0
//   +0x0C Vec   mNormal        +0x20 Vec2  mTexCoord1
//   +0x28 uint  mColor   (from `stw r3,40(r5)` and `stw r5,40(r4)`)
//   +0x2C ushort mTextureUsed  (from `lhz`/`ori r0,r0,1`/`sth` on 44)
//   +0x2E uchar mStreamFlags   (from the `or`/`stb` on 46 in all three)
//
// It is retail `.bss`, so zero is its own value, and on the host nothing else writes it: the
// only other writer is `DolphinCGraphics.cpp`'s own `vtxDescr`, which is not compiled.
//
// ## The two stream-flag bits are read, not assumed
//
// Each body `or`s in a **byte loaded from `.sdata2`**, not an immediate:
//
//   802c0fc0  lbz  r3,-16048(r2)     ; _SDA2_BASE_ = 0x804223C0 -> 0x8041E510
//   802c10a4  lbz  r0,-16047(r2)     ; -> 0x8041E511
//
// `objdump -s` on those two bytes of the linked ELF reads `04 02`, so `lbl_8041E510` = 4 and
// `lbl_8041E511` = 2 - the same `kHasTexture` / `kHasColor` the Dolphin file spells as literals.
// They are written as literals here because the host has no DOL `.sdata2` to relocate against,
// which is the arrangement `src/Kyoto/Graphics/CGraphicsHostGlobals.cpp` uses for the other
// guest constants.
//
// The `or` rather than an assignment is load-bearing: the descriptor carries one flag byte for
// three independent facts (position/normal/colour/texture present), and each setter adds its
// own bit without clearing the others. `StreamTexcoord` sets two - `kHasTexture` in the flag
// byte and bit 0 of the 16-bit `mTextureUsed`, which is a different field and is what
// `CGraphics::SetLineWidth`-adjacent state reads to decide whether a texture unit is bound.
#include "Kyoto/Graphics/CGraphics.hpp"

#include "Kyoto/Graphics/CColor.hpp"
#include "Kyoto/Math/CVector2f.hpp"

#include "dolphin/mtx/GeoTypes.h"

namespace {
/** `lbl_80416EE0`: retail `.bss`, 0x30 bytes, zero at load. See the offsets in the header. */
struct SVtxDescr {
  Vec mPosition;
  Vec mNormal;
  Vec2 mTexCoord0;
  Vec2 mTexCoord1;
  uint mColor;
  ushort mTextureUsed;
  uchar mStreamFlags;
};
SVtxDescr sVtxDescr;

// `.sdata2` 0x8041E511 and 0x8041E510, read out of the linked ELF - see the header.
const uchar kHasColor = 2;
const uchar kHasTexture = 4;
} // namespace

void CGraphics::StreamColor(uint color) {
  sVtxDescr.mColor = color;
  sVtxDescr.mStreamFlags |= kHasColor;
}

void CGraphics::StreamColor(const CColor& color) {
  sVtxDescr.mColor = color.GetColor_u32();
  sVtxDescr.mStreamFlags |= kHasColor;
}

void CGraphics::StreamTexcoord(float u, float v) {
  sVtxDescr.mTexCoord0.x = u;
  sVtxDescr.mTexCoord0.y = v;
  sVtxDescr.mStreamFlags |= kHasTexture;
  sVtxDescr.mTextureUsed |= 1;
}
