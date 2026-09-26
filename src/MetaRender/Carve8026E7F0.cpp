// Eleven `CCubeRenderer` methods out of dtk's `auto_03_8026258C_text` range, carved as one
// `Matching` unit. Retail .text 0x8026E7E4..0x8026E9B8, 0x1D4 = 468 bytes:
//
//   8026e7e4 <SetDrawableCallback__13CCubeRendererFPFPCvPCvi_vPCv>:        0x0C =  12
//   8026e7f0 <GetFPS__13CCubeRendererFv>:                                 0x20 =  32
//   8026e810 <SetBlendMode_AdditiveDestColor__13CCubeRendererFv>:       0x30 =  48
//   8026e840 <SetBlendMode_Replace__13CCubeRendererFv>:                  0x30 =  48
//   8026e870 <SetBlendMode_InvertSrc__13CCubeRendererFv>:                0x30 =  48
//   8026e8a0 <SetBlendMode_InvertDst__13CCubeRendererFv>:                0x30 =  48
//   8026e8d0 <SetBlendMode_ColorMultiply__13CCubeRendererFv>:            0x30 =  48
//   8026e900 <SetBlendMode_NoColorWrite__13CCubeRendererFv>:             0x30 =  48
//   8026e930 <SetBlendMode_AlphaBlended__13CCubeRendererFv>:             0x30 =  48
//   8026e960 <SetBlendMode_AdditiveAlpha__13CCubeRendererFv>:            0x30 =  48
//   8026e990 <SetDepthReadWrite__13CCubeRendererFbb>:                    0x28 =  40
//
// Eleven methods, 468 bytes, one unit, because they are one contiguous run: `SetDrawableCallback`
// is the function immediately above `GetFPS` with nothing between. Eleven in one claim rather
// than eleven claims - `linked` counts functions, so a contiguous run is one unit worth
// eleven, and eleven claims would be eleven chances at the range clash for the same eleven
// functions.
//
// `SetDrawableCallback` is the 12-byte one and needs no frame at all:
//
//     stw r4,152(r3) ; stw r5,156(r3) ; blr
//
// i.e. two pointer stores at +0x98 and +0x9C and a return. The mangled name is
// `FPFPCvPCvi_vPCv`, which is `TDrawableCallback` followed by `PCv` - and
// `IRenderer::TDrawableCallback` is
// `void (*)(const void*, const void*, int)`, so it mangles to `PFPCvPCvi_v` and the header's
// typedef already produces retail's exact name. **No typedef change was needed here**, which
// is worth recording because it looks like one: the header's `SetDrawableCallback` takes two
// parameters and the mangled name looks like four. Two member names were needed
// (`include/MetaRender/CCubeRenderer.hpp`), because the body writes +0x98 and +0x9C and both
// were inside a `uchar pad[]`.
//
// **The eight blend modes are one shape with four constants each.** Retail's
// `SetBlendMode_AdditiveDestColor` is
//
//     stwu r1,-16(r1) ; mflr r0 ; li r3,1 ; li r4,2 ; stw r0,20(r1)
//     li r5,1 ; li r6,0 ; bl 802c15e8
//     lwz r0,20(r1) ; mtlr r0 ; addi r1,r1,16 ; blr
//
// and the other seven differ only in the four `li`s. The callee is `fn_802C15E8`, which is
// a two-instruction tail call to `CGX::SetBlendMode(mode, srcFac, dstFac, op)` - so the
// three-argument set is passed through a wrapper retail compiled, and the source below calls
// that wrapper rather than `CGX::SetBlendMode` directly. Calling `CGX::SetBlendMode` from
// here would emit a call to a different address and reproduce nothing: **the call target is
// part of the bytes**, and the wrapper exists in retail's link at 0x802C15E8 with its own
// name, so the object below has to name it.
//
// The four constants, resolved against `include/dolphin/gx/GXEnum.h`:
//
//   method                          mode srcFac dstFac op   (r3, r4, r5, r6)
//   AdditiveDestColor              1    2      1      0    BLEND, SRCCLR, ONE, CLEAR
//   Replace                         1    1      0      0    BLEND, ONE,    ZERO, CLEAR
//   InvertSrc                       2    1      0      12   LOGIC, ONE,    ZERO, INVCOPY
//   InvertDst                       1    3      0      0    BLEND, INVSRCCLR, ZERO
//   ColorMultiply                   1    0      2      0    BLEND, ZERO,   SRCCLR
//   NoColorWrite                    1    0      1      0    BLEND, ZERO,   ONE
//   AlphaBlended                    1    4      5      0    BLEND, SRCALPHA, INVSRCALPHA
//   AdditiveAlpha                   1    4      1      0    BLEND, SRCALPHA, ONE
//
// `SetDepthReadWrite(bool, bool)` forwards **`read`**, as a `bool`, with no conversion:
// retail does `mr r3,r4 ; li r4,3 ; bl fn_802C162C`. Getting this wrong is a one-instruction
// miss that reads as a codegen difference and is not one. Two wrong spellings each cost the
// same 4% and each looks equally reasonable: passing `update` gives `clrlwi r3,r5,24` (the
// register says the source operand is `read`), and declaring the wrapper's first parameter
// `GXBool` - a distinct one-byte type from `bool` - gives `clrlwi r3,r4,24`, because the
// argument is then a type conversion rather than a copy. **It has to be `bool`**: a `bool`
// argument passed to a `bool` parameter needs no widening, so no mask is emitted. That is the
// whole of this function's shape.
//
// `fn_802C162C` is retail's `SetZMode` wrapper: it normalises *its own* two booleans with
// `clrlwi ...,24`, stores the second to a `.sdata` word, and calls
// `CGX::SetZMode(compareEnable, func, updateEnable)` with the stored word as `func`. So
// `fn_802C162C(read, 3)` is "Z-test enabled iff `read`, always LEQUAL", which is retail's
// one-line body and not a guess: the three is the immediate in `li r4,3`.
//
// `GetFPS` is a leaf call to `fn_802BEC6C` (0x802BEC6C, 0x50), which is the unnamed CGraphics
// method `SetUseVideoFilter` calls too. It is a real named symbol in retail's ELF, so the
// object can call it.
//
// **Ten of the eleven need no header change at all.** Each is already declared in
// `include/MetaRender/CCubeRenderer.hpp` with the signature retail mangles: the eight blend
// modes and `GetFPS` as no-argument, and `SetDepthReadWrite(bool read, bool update)`. Only
// `SetDrawableCallback` needed the two member names. The header's *other* `CCubeRenderer`
// entries are not all right - `DrawSpaceWarp`, `SetWorldFog`, `SetWireframeFlags`,
// `SetWorldViewpoint` and `BeginPrimitive` all have signatures retail does not mangle that
// way - which is why this is a claim about these eleven and not about the class.
//
// Declared descending by retail offset: mwcceppc emits in reverse source order, and an
// ascending file is a permuted `.text` that still scores 100.00% per function and still
// breaks the DOL.
//
// `src/MetaRender/CCubeRenderer.cpp` does not exist, so nothing else in the tree defines any
// of these symbols; the four carve units in this directory are the only CCubeRenderer
// definitions anywhere, which is also why none of them emits the class's vtable as an extra
// (there is no key function anywhere in the tree, and `unit_fit.sh` confirms no extra
// functions in any of the five).

#include "MetaRender/CCubeRenderer.hpp"

// `fn_802C15E8` and `fn_802C162C` are retail's real symbol names (see
// `config/G2ME01/symbols.txt`), and both are defined in the DOL by an `auto_*` range, so a
// relocation against them resolves in the matching build.
extern "C" {
void fn_802C15E8(GXBlendMode, GXBlendFactor, GXBlendFactor, GXLogicOp);
void fn_802C162C(bool, GXCompare);
void fn_802BEC6C();
}

void CCubeRenderer::SetDepthReadWrite(bool read, bool update) {
  fn_802C162C(read, GX_LEQUAL);
}

void CCubeRenderer::SetBlendMode_AdditiveAlpha() {
  fn_802C15E8(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_ONE, GX_LO_CLEAR);
}

void CCubeRenderer::SetBlendMode_AlphaBlended() {
  fn_802C15E8(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_CLEAR);
}

void CCubeRenderer::SetBlendMode_NoColorWrite() {
  fn_802C15E8(GX_BM_BLEND, GX_BL_ZERO, GX_BL_ONE, GX_LO_CLEAR);
}

void CCubeRenderer::SetBlendMode_ColorMultiply() {
  fn_802C15E8(GX_BM_BLEND, GX_BL_ZERO, GX_BL_SRCCLR, GX_LO_CLEAR);
}

void CCubeRenderer::SetBlendMode_InvertDst() {
  fn_802C15E8(GX_BM_BLEND, GX_BL_INVSRCCLR, GX_BL_ZERO, GX_LO_CLEAR);
}

void CCubeRenderer::SetBlendMode_InvertSrc() {
  fn_802C15E8(GX_BM_LOGIC, GX_BL_ONE, GX_BL_ZERO, GX_LO_INVCOPY);
}

void CCubeRenderer::SetBlendMode_Replace() {
  fn_802C15E8(GX_BM_BLEND, GX_BL_ONE, GX_BL_ZERO, GX_LO_CLEAR);
}

void CCubeRenderer::SetBlendMode_AdditiveDestColor() {
  fn_802C15E8(GX_BM_BLEND, GX_BL_SRCCLR, GX_BL_ONE, GX_LO_CLEAR);
}

void CCubeRenderer::GetFPS() { fn_802BEC6C(); }

void CCubeRenderer::SetDrawableCallback(TDrawableCallback cb, const void* ctx) {
  x98_drawableCallback = cb;
  x9c_drawableContext = ctx;
}
