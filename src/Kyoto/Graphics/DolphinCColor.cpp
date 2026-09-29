#include "Kyoto/Graphics/CColor.hpp"

#include "Kyoto/Streams/CInputStream.hpp"
#include "rstl/math.hpp"

CColor::CColor(CInputStream& in) {
  float r = in.ReadFloat();
  float g = in.ReadFloat();
  float b = in.ReadFloat();
  Set(r, g, b, in.ReadFloat());
}

CColor::CColor(const float r, const float g, const float b, const float a) {
  mR = r * 255.f;
  mG = g * 255.f;
  mB = b * 255.f;
  mA = a * 255.f;
}

void CColor::Set(float r, float g, float b, float a) {
  mR = CCast::ToUint8(r * 255.f);
  mG = CCast::ToUint8(g * 255.f);
  mB = CCast::ToUint8(b * 255.f);
  mA = CCast::ToUint8(a * 255.f);
}

void CColor::Get(float& r, float& g, float& b, float& a) const {
  r = CCast::ToReal32(mR) * (1.f / 255.f);
  g = CCast::ToReal32(mG) * (1.f / 255.f);
  b = CCast::ToReal32(mB) * (1.f / 255.f);
  a = CCast::ToReal32(mA) * (1.f / 255.f);
}

void CColor::Get(float& r, float& g, float& b) const {
  r = CCast::ToReal32(mR) * (1.f / 255.f);
  g = CCast::ToReal32(mG) * (1.f / 255.f);
  b = CCast::ToReal32(mB) * (1.f / 255.f);
}

CColor CColor::Lerp(const CColor& a, const CColor& b, float t) {
  const float omt = 1.f - t;
  return CColor(omt * a.GetRed() + t * b.GetRed(), omt * a.GetGreen() + t * b.GetGreen(),
                omt * a.GetBlue() + t * b.GetBlue(), omt * a.GetAlpha() + t * b.GetAlpha());
}

uint CColor::Lerp(uint a, uint b, float t) {
  uint alpha = t * 256.f;
  uint dstrb = a & 0xff00ff;
  uint dstag = a >> 8 & 0xff00ff;
  uint srcrb = b & 0xff00ff;
  uint srcag = b >> 8 & 0xff00ff;
  uint drb = srcrb - dstrb;
  uint dag = srcag - dstag;
  drb *= alpha;
  dag *= alpha;
  drb >>= 8;
  dag >>= 8;
  const uint rb = (drb + dstrb) & 0x00ff00ff;
  const uint ag = (dag + dstag) << 8 & 0xff00ff00;
  return rb | ag;
}

CColor CColor::Modulate(const CColor& a, const CColor& b) {
  return CColor((uchar)((a.GetRedu8() * b.GetRedu8()) / (uchar)255),
                (a.GetGreenu8() * b.GetGreenu8()) / (uchar)255,
                (a.GetBlueu8() * b.GetBlueu8()) / (uchar)255,
                (a.GetAlphau8() * b.GetAlphau8()) / (uchar)255);
}

CColor CColor::Add(const CColor& a, const CColor& b) {
  const uchar alpha = rstl::min_val< uint >(255, a.GetAlphau8() + b.GetAlphau8());
  const uchar blue = rstl::min_val< uint >(255, a.GetBlueu8() + b.GetBlueu8());
  const uchar green = rstl::min_val< uint >(255, a.GetGreenu8() + b.GetGreenu8());
  const uchar red = rstl::min_val< uint >(255, a.GetRedu8() + b.GetRedu8());
  return CColor(red, green, blue, alpha);
}

ushort CColor::ToRGB5A3() const {
  const uchar r = GetRedu8();
  const uchar g = GetGreenu8();
  const uchar b = GetBlueu8();
  const uchar a = GetAlphau8();
  ushort ret = (1 << 15) | ((b & 0xf8) >> 3) | ((g & 0xf8) << 2) | ((r & 0xf8) << 7);
  if (a != 0xff)
    ret = ((a & 0xe0) << 7) | ((b & 0xf0) >> 4) | (g & 0xf0) | ((r & 0xf0) << 4);
  return ret;
}

// Guessed name: target routine at 0x80320420 packs RGB565.
ushort CColor::ToRGB565() const {
  return ((mB & 0xf8) >> 3) | ((mG & 0xfc) << 3) | ((mR & 0xf8) << 8);
}

// Guessed name: target routine at 0x80320440 expands RGB5A3.
CColor CColor::FromRGB5A3(ushort value) {
  // Upstream's body has three real faults the asm rules out: in the alpha branch it assigns the
  // red expression to mB as well, it sets mA from the bitfield instead of leaving it at 0xff, and
  // it writes straight into the members instead of building the retail GXColor staging word.
  if (value & 0x8000) {
    GXColor color = {0, 0, 0, 0xff};
    color.r = ((value & 0x7c00) >> 7) | ((value & 0x7000) >> 13);
    color.g = ((value & 0x3e0) >> 2) | ((value & 0x380) >> 7);
    color.b = (value & 0x1f) << 3 | (value & 0x1f) >> 2;
    return CColor(color.r, color.g, color.b, color.a);
  } else {
    GXColor color = {0, 0, 0, 0};
    color.r = ((value & 0xf00) >> 4) | ((value & 0xf00) >> 8);
    color.g = ((value & 0xf0) >> 4) | (value & 0xf0);
    color.b = (value & 0xf) << 4 | (value & 0xf);
    color.a = ((value & 0x7000) >> 7) | ((value & 0x7000) >> 10);
    return CColor(color.r, color.g, color.b, color.a);
  }
}
