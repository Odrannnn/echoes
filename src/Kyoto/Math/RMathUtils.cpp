#define MSL_NO_INLINE_SQRT
#include "Kyoto/Math/CMath.hpp"

#include "Kyoto/Math/CVector3f.hpp"

#ifndef TARGET_PC
extern "C" float fn_8001D658(float x);
extern "C" double lbl_80419A58;
extern "C" const double lbl_8041E750;

double lbl_80419A58;

// Retail's constructor cache for sqrt(3), retained with its address-derived name.
extern "C" void fn_802CDF64() { lbl_80419A58 = CMath::SqrtD(lbl_8041E750); }

// Float swap helper used by the polynomial solvers in this unit.
extern "C" void fn_802CDF50(float* a, float* b) {
  float tmp = *a;
  *a = *b;
  *b = tmp;
}
#endif

#ifdef TARGET_PC
float CMath::SqrtF(const float x) { return sqrtf(x); }
#else
float CMath::SqrtF(const float x) { return fn_8001D658(x); }
#endif

double CMath::SqrtD(const double x) { return sqrt(x); }

#ifdef TARGET_PC
float CMath::InvSqrtF(float x) { return 1.f / sqrtf(x); }
#else
float CMath::InvSqrtF(float x) { return 1.f / fn_8001D658(x); }
#endif

float CMath::CeilingF(float x) {
  float tmp = floor(x);
  if (tmp == x) {
    return x;
  }
  return tmp + 1.f;
}

CVector3f CMath::GetCatmullRomSplinePoint(const CVector3f& a, const CVector3f& b,
                                          const CVector3f& c, const CVector3f& d, float t) {
  if (t <= 0.0f)
    return b;
  if (t >= 1.0f)
    return c;

  return (
      a * (-0.5f * t * t * t + t * t - 0.5f * t) + b * (1.5f * t * t * t + -2.5f * t * t + 1.0f) +
      c * (-1.5f * t * t * t + 2.0f * t * t + 0.5f * t) + d * (0.5f * t * t * t - 0.5f * t * t));
}

float CMath::GetCatmullRomSplinePoint(float a, float b, float c, float d, float t) {
  if (t <= 0.0f)
    return b;
  if (t >= 1.0f)
    return c;

  return (
      a * (-0.5f * t * t * t + t * t - 0.5f * t) + b * (1.5f * t * t * t + -2.5f * t * t + 1.0f) +
      c * (-1.5f * t * t * t + 2.0f * t * t + 0.5f * t) + d * (0.5f * t * t * t - 0.5f * t * t));
}

CVector3f CMath::GetBezierPoint(const CVector3f& a, const CVector3f& b, const CVector3f& c,
                                const CVector3f& d, float t) {
  CVector3f ab = CVector3f::Lerp(a, b, t);
  CVector3f bc = CVector3f::Lerp(b, c, t);
  CVector3f cd = CVector3f::Lerp(c, d, t);

  return CVector3f::Lerp(CVector3f::Lerp(ab, bc, t), CVector3f::Lerp(bc, cd, t), t);
}

// The quintic fade the noise functions below weight their lattice corners with:
// x^3 * (6x^2 - 15x + 10), the C2-continuous smootherstep. Its three coefficients are
// 6, 15 and 10 in this order in retail's .sdata2.
extern "C" float fn_802CCE28(float x) {
  const float x2 = x * x;
  const float x3 = x * x2;
  const float linear = 6.f * x - 15.f;
  return x3 * (x * linear + 10.f);
}

// Linear interpolation, t*(b - a) + a. The noise functions use it to mix the eight
// corner values once the fades are applied.
extern "C" float fn_802CCE1C(float t, float a, float b) { return t * (b - a) + a; }

#ifndef TARGET_PC
// Retail's permutation table for the noise functions, 512 bytes of .data at
// lbl_803BA0B8: the 256-entry permutation repeated twice, so the +1 lookups need no
// wrap. It belongs to another unit's claim, hence the external reference.
extern "C" unsigned char lbl_803BA0B8[];

// The gradient picker the noise body calls once per lattice corner: it takes the
// permutation byte and the three fractional offsets measured from that corner, and
// returns the dot product of the corner's gradient with them.
extern "C" float fn_802CCDA4(int hash, float x, float y, float z);

// Its four-dimensional counterpart, with four offsets.
extern "C" float fn_802CCD28(int hash, float x, float y, float z, float w);

// The two Perlin noise bodies, declared here rather than in CMath.hpp because they are
// local to this unit and the wrappers below are the only things that name them. The
// wrappers are guarded for the same reason as the helpers above - the port has no caller
// for them (CREPerlinNoise lives in CRealElement.cpp, which is not in files.cmake), so
// defining them there would only add two undefined symbols to the link.
extern "C" float fn_802CCA38(float x, float y, float z);
extern "C" float fn_802CC4E4(float x, float y, float z, float w);

// Perlin's improved noise in three dimensions: split the point into its integer
// lattice cell and the fraction inside it, fade the fractions, gather the eight corner
// gradients through the permutation table and mix them with the fades, x fastest.
extern "C" float fn_802CCA38(float x, float y, float z) {
  const float flx = floor(x);
  const float fly = floor(y);
  const float flz = floor(z);
  const int mx = static_cast< int >(flx) & 0xFF;
  const int my = static_cast< int >(fly) & 0xFF;
  const int mz = static_cast< int >(flz) & 0xFF;
  x -= flx;
  y -= fly;
  z -= flz;
  const float ux = fn_802CCE28(x);
  const float uy = fn_802CCE28(y);
  const float uz = fn_802CCE28(z);
  const int a = lbl_803BA0B8[mx] + my;
  const int b = lbl_803BA0B8[mx + 1] + my;
  const int aa = lbl_803BA0B8[a] + mz;
  const int ab = lbl_803BA0B8[a + 1] + mz;
  const int ba = lbl_803BA0B8[b] + mz;
  const int bb = lbl_803BA0B8[b + 1] + mz;
  // The two z halves are written z first and z-1 second, and the outer lerp's arguments
  // are in the same order: mwcceppc evaluates a call's arguments right to left, so the
  // z-1 corners are the ones gathered first, which is the order retail calls them in.
  return fn_802CCE1C(uz, fn_802CCE1C(uy, fn_802CCE1C(ux,
                                                      fn_802CCDA4(lbl_803BA0B8[aa], x, y, z),
                                                      fn_802CCDA4(lbl_803BA0B8[ba], x - 1, y, z)),
                            fn_802CCE1C(ux, fn_802CCDA4(lbl_803BA0B8[ab], x, y - 1, z),
                                       fn_802CCDA4(lbl_803BA0B8[bb], x - 1, y - 1, z))),
         fn_802CCE1C(uy, fn_802CCE1C(ux,
                                     fn_802CCDA4(lbl_803BA0B8[aa + 1], x, y, z - 1),
                                     fn_802CCDA4(lbl_803BA0B8[ba + 1], x - 1, y, z - 1)),
                     fn_802CCE1C(ux, fn_802CCDA4(lbl_803BA0B8[ab + 1], x, y - 1, z - 1),
                                fn_802CCDA4(lbl_803BA0B8[bb + 1], x - 1, y - 1, z - 1))));
}

// Perlin's improved noise in four dimensions: as the three-dimensional body above, with a
// fourth lattice axis, so sixteen corner gradients mixed by fifteen lerps. Each of the
// eight `x?y?z?` names is the lattice index after three permutation lookups; the fourth
// axis is the +1 on the index, which is why the w halves are written w first and w-1
// second: mwcceppc evaluates a call's arguments right to left, so the w-1 corners are
// gathered first, which is the order retail calls them in.
extern "C" float fn_802CC4E4(float x, float y, float z, float w) {
  const float flx = floor(x);
  const float fly = floor(y);
  const float flz = floor(z);
  const float flw = floor(w);
  const int mx = static_cast< int >(flx) & 0xFF;
  const int my = static_cast< int >(fly) & 0xFF;
  const int mz = static_cast< int >(flz) & 0xFF;
  const int mw = static_cast< int >(flw) & 0xFF;
  x -= flx;
  y -= fly;
  z -= flz;
  w -= flw;
  const float ux = fn_802CCE28(x);
  const float uy = fn_802CCE28(y);
  const float uz = fn_802CCE28(z);
  const float uw = fn_802CCE28(w);
  const int a = lbl_803BA0B8[mx] + my;
  const int aa = lbl_803BA0B8[a] + mz;
  const int ab = lbl_803BA0B8[a + 1] + mz;
  const int b = lbl_803BA0B8[mx + 1] + my;
  const int ba = lbl_803BA0B8[b] + mz;
  const int bb = lbl_803BA0B8[b + 1] + mz;
  const int aaa = lbl_803BA0B8[aa] + mw;
  const int aab = lbl_803BA0B8[ab] + mw;
  const int aba = lbl_803BA0B8[aa + 1] + mw;
  const int abb = lbl_803BA0B8[ab + 1] + mw;
  const int baa = lbl_803BA0B8[ba] + mw;
  const int bab = lbl_803BA0B8[ba + 1] + mw;
  const int bba = lbl_803BA0B8[bb] + mw;
  const int bbb = lbl_803BA0B8[bb + 1] + mw;
  return fn_802CCE1C(uw,
    fn_802CCE1C(uz,
        fn_802CCE1C(uy,
            fn_802CCE1C(ux,
                fn_802CCD28(lbl_803BA0B8[aaa], x, y, z, w),
                fn_802CCD28(lbl_803BA0B8[baa], x - 1, y, z, w)),
            fn_802CCE1C(ux,
                fn_802CCD28(lbl_803BA0B8[aba], x, y - 1, z, w),
                fn_802CCD28(lbl_803BA0B8[bba], x - 1, y - 1, z, w))),
        fn_802CCE1C(uy,
            fn_802CCE1C(ux,
                fn_802CCD28(lbl_803BA0B8[aab], x, y, z - 1, w),
                fn_802CCD28(lbl_803BA0B8[bab], x - 1, y, z - 1, w)),
            fn_802CCE1C(ux,
                fn_802CCD28(lbl_803BA0B8[abb], x, y - 1, z - 1, w),
                fn_802CCD28(lbl_803BA0B8[bbb], x - 1, y - 1, z - 1, w)))),
    fn_802CCE1C(uz,
        fn_802CCE1C(uy,
            fn_802CCE1C(ux,
                fn_802CCD28(lbl_803BA0B8[aaa + 1], x, y, z, w - 1),
                fn_802CCD28(lbl_803BA0B8[baa + 1], x - 1, y, z, w - 1)),
            fn_802CCE1C(ux,
                fn_802CCD28(lbl_803BA0B8[aba + 1], x, y - 1, z, w - 1),
                fn_802CCD28(lbl_803BA0B8[bba + 1], x - 1, y - 1, z, w - 1))),
        fn_802CCE1C(uy,
            fn_802CCE1C(ux,
                fn_802CCD28(lbl_803BA0B8[aab + 1], x, y, z - 1, w - 1),
                fn_802CCD28(lbl_803BA0B8[bab + 1], x - 1, y, z - 1, w - 1)),
            fn_802CCE1C(ux,
                fn_802CCD28(lbl_803BA0B8[abb + 1], x, y - 1, z - 1, w - 1),
                fn_802CCD28(lbl_803BA0B8[bbb + 1], x - 1, y - 1, z - 1, w - 1)))));
}

float CMath::Noise1d(float x) { return fn_802CCA38(x, 0.f, 0.f); }
float CMath::Noise2d(float x, float y) { return fn_802CCA38(x, y, 0.f); }
float CMath::Noise3d(float x, float y, float z) { return fn_802CCA38(x, y, z); }
float CMath::Noise4d(float x, float y, float z, float w) { return fn_802CC4E4(x, y, z, w); }
#endif

CVector3f CMath::BaryToWorld(const CVector3f& p0, const CVector3f& p1, const CVector3f& p2,
                             const CVector3f& bary) {
  return bary.GetX() * p0 + bary.GetY() * p1 + bary.GetZ() * p2;
}

static const uint skSinX1 = 0x3f7ff347;
static const uint skSinX2 = 0xbe2a34ae;
static const uint skSinX3 = 0x3c047fca;
static const uint skSinX4 = 0xb9206873;

float CMath::FastSinR(float x) {
  if (fabsf(x) > M_PIF) {
    x = WrapPi(x);
  }

  float x2 = x * x;
  float acc = x;
  acc *= reinterpret_cast< const float& >(skSinX1);
  x *= x2;
  acc += x * reinterpret_cast< const float& >(skSinX2);
  x *= x2;
  acc += x * reinterpret_cast< const float& >(skSinX3);
  x *= x2;
  acc += x * reinterpret_cast< const float& >(skSinX4);
  return acc;
}

static const uint skCosX1 = 0x3f800000;
static const uint skCosX2 = 0xbefffd62;
static const uint skCosX3 = 0x3d2a7a18;
static const uint skCosX4 = 0xbab2bb2b;
static const uint skCosX5 = 0x37a93188;

float CMath::FastCosR(float x) {
  if (fabsf(x) > M_PIF) {
    x = WrapPi(x);
  }

  float x2 = x * x;
  float acc = reinterpret_cast< const float& >(skCosX1);
  acc += x2 * reinterpret_cast< const float& >(skCosX2);
  float xn = x2 * x2;
  acc += xn * reinterpret_cast< const float& >(skCosX3);
  xn *= x2;
  acc += xn * reinterpret_cast< const float& >(skCosX4);
  xn *= x2;
  acc += xn * reinterpret_cast< const float& >(skCosX5);
  return acc;
}

static const uint skArcCosX1 = 0x3fc90fdb;
static const uint skArcCosX2 = 0xbf7f8bd1;
static const uint skArcCosX3 = 0xbe52ce8c;
static const uint skArcCosX4 = 0x3de9fe20;
static const uint skArcCosX5 = 0xbe980d88;

float CMath::FastArcCosR(float x) {
  if (fabsf(x) > 0.925f) {
    return acosf(x);
  }

  float x2 = x * x;
  float acc = reinterpret_cast< const float& >(skArcCosX1);
  acc += x * reinterpret_cast< const float& >(skArcCosX2);
  float xn = x * x2;
  acc += xn * reinterpret_cast< const float& >(skArcCosX3);
  xn *= x2;
  acc += xn * reinterpret_cast< const float& >(skArcCosX4);
  xn *= x2;
  acc += xn * reinterpret_cast< const float& >(skArcCosX5);
  return acc;
}

int CMath::FloorPowerOfTwo(int v) {
  if (v == 0) {
    return 0;
  }
  const uint s1 = (0xffffU - v) >> 0x1b & 0x10;
  const uint sb1 = static_cast< uint >(v) >> s1 & 0xffff;
  const uint s2 = (0xff - sb1) >> 0x1c & 8;
  const uint sb2 = sb1 >> s2 & 0xff;
  const uint s3 = ((0xf - sb2) >> 0x1d) & 4;
  const uint sb3 = (sb2 >> s3) & 0xf;
  const uint s4 = (3 - sb3) >> 0x1e & 2;
  const uint totalShift = s1 + s2 + s3 + s4;
  const uint finalSig = sb3 >> s4 & 3;
  const uint finalShift = ((1 - finalSig) >> 0x1f) + totalShift;
  return 1 << finalShift;
}

// The same bit scan as FloorPowerOfTwo, returning the shift rather than 1 << shift, so the
// answer is the position of v's highest set bit. Retail's parameter is signed: the object
// compares it with `cmpwi` and then shifts it as unsigned, which is what `srw` after a
// `static_cast<uint>` spells.
extern "C" int fn_802CC120(int v) {
  if (v == 0) {
    return 0;
  }
  const uint s1 = (0xffffU - v) >> 0x1b & 0x10;
  const uint sb1 = static_cast< uint >(v) >> s1 & 0xffff;
  const uint s2 = (0xff - sb1) >> 0x1c & 8;
  const uint sb2 = sb1 >> s2 & 0xff;
  const uint s3 = ((0xf - sb2) >> 0x1d) & 4;
  const uint sb3 = (sb2 >> s3) & 0xf;
  const uint s4 = (3 - sb3) >> 0x1e & 2;
  const uint totalShift = s1 + s2 + s3 + s4;
  const uint finalSig = sb3 >> s4 & 3;
  return ((1 - finalSig) >> 0x1f) + totalShift;
}

#ifndef TARGET_PC
#pragma section ".ctors$10"
__declspec(section ".ctors$10") extern void* const fn_802CDF64_reference = fn_802CDF64;
#endif

bool CMath::SolveQuadratic(float a, float b, float c, float& plus, float& minus) {
  const float discriminant = b * b - (4.f * a) * c;
  if (discriminant < FLT_EPSILON || fabsf(a) < FLT_EPSILON) {
    return false;
  }
  const float root = SqrtF(discriminant);
  plus = (-b + root) / (2.f * a);
  minus = (-b - root) / (2.f * a);
  return true;
}

float CMath::PhongBlob(float t, float exponent) {
  t = Clamp(0.f, t, 1.f);
  return pow(0.5f * (1.f + cosf(M_PIF * t)), exponent);
}

float CMath::EaseInOut(float t, EEaseTypes ease, float easeIn, float easeOut, float low, float high,
                       float scale) {
  const float minimum = FastMin(low, high);
  const float maximum = FastMax(low, high);
  const float range = maximum - minimum;
  t = Clamp(0.f, t, 1.f);
  easeIn = Clamp(0.f, easeIn, 1.f);
  easeOut = Clamp(0.f, easeOut, 1.f);

  switch (ease) {
  case kET_Sinusoidal: {
    const float easeInWeight = 2.f * easeIn / M_PIF;
    // Upstream declares these in the other order and both `const`; the order is what MWCC's
    // register allocator sees, and retail only matches with the non-const, easeOut-first pair.
    float easeOutWeight, middle;
    middle = easeOut + easeInWeight - easeIn;
    easeOutWeight = 2.f * (1.f - easeOut) / M_PIF;
    const float total = middle + easeOutWeight;
    if (t <= easeIn) {
      t = easeInWeight * (1.f + sinf(M_PIF * 0.5f * (t / easeIn) - M_PIF * 0.5f)) / total;
    } else if (t >= easeOut) {
      t = (easeOutWeight * sinf(M_PIF * 0.5f * ((t - easeOut) / (1.f - easeOut))) + middle) / total;
    } else {
      t = (t + easeInWeight - easeIn) / total;
    }
    break;
  }
  case kET_Quadratic:
    if (t <= easeIn) {
      t = scale * (t * t / (2.f * easeIn));
    } else if (t >= easeOut) {
      const float delta = t - easeOut;
      const float base = 0.5f * (scale * easeIn) + scale * (easeOut - easeIn);
      const float falloff = -(0.5f * (scale * (delta / (1.f - easeOut))) - scale);
      const float falloffTerm = falloff * delta;
      t = base + falloffTerm;
    } else {
      t = scale * (0.5f * easeIn) + scale * (t - easeIn);
    }
    break;
  default:
    break;
  }

  return range * Clamp(0.f, t, 1.f) + minimum;
}
