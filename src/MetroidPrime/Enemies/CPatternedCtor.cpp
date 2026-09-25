// CPatterned's constructor, 0x80079BE4, 0xb58 (2904) bytes - the largest single unmatched
// function in the DOL, and the last thing standing between the Enemy hierarchy and the 75 creature
// modules. It is a *separate* unit from MetroidPrime/Enemies/CPatterned.cpp because it lives at
// 0x80079BE4 while that unit claims 0x80073C58..0x80073CB4, and one unit may claim only one range
// per section in config/G2ME01/splits.txt. See docs/research/CPatterned_layout.txt for the
// byte-by-byte split of what is below, and for what is still not written.
//
// Nothing here is claimed in splits.txt yet, so this object is not in the link: the DOL and all 86
// RELs still reproduce retail with the retail bytes at 0x80079BE4. The point of the file is that
// the signature, the member list and the member-init list are fixed by measurement rather than
// guessed, and that the body compiles at all.
#include "MetroidPrime/Enemies/CPatterned.hpp"

#include <math.h>

#include "MetroidPrime/CActorParameters.hpp"
#include "MetroidPrime/CEntityInfo.hpp"
#include "MetroidPrime/TGameTypes.hpp"

#include "Kyoto/Math/CAABox.hpp"
#include "Kyoto/Math/CVector3f.hpp"

// Retail's shared .sdata2 pool. Writing the literals recreates the pool entries and moves every
// address above them, which breaks the modules - the same trap as kCAiSplashDenom. The values were
// read out of the retail split objects, not recalled:
//
//   lbl_8041AAC0 0.0f    lbl_8041AAC8 1.0f    lbl_8041AAD0 0.5f    lbl_8041AAD4 -1.0f
//   lbl_8041AB0C 0.8f    lbl_8041AB20 pi/180 lbl_8041AB24 3.0f
//   lbl_8041AAB8 0.33f   lbl_8041B758 24.525f
extern const float lbl_8041AAC0; // 0.0f
extern const float lbl_8041AAC8; // 1.0f
extern const float lbl_8041AAD0; // 0.5f
extern const float lbl_8041AAD4; // -1.0f
extern const float lbl_8041AB0C; // 0.8f
extern const float lbl_8041AB20; // pi/180
extern const float lbl_8041AB24; // 3.0f

extern CVector3f sZeroVector__9CVector3f;
extern CVector3f sForwardVector__9CVector3f;
extern CQuaternion sNoRotation__11CQuaternion;

// Every offset the constructor reads out of its CPatternedInfo argument, measured from the loads in
// its body. The three CVector3f-shaped groups are copied as three scalars, not as a vector, which
// is why they are three floats each. Not one of the field names is retail's; the suffixes are
// offsets so that a later rename is mechanical.
class CPatternedInfo {
public:
  uchar pad00[4];
  float x4, x8, xc, x10;          // -> x3c8_pInfo[4]
  float x14;                      // degrees; cos(x14 * pi/180) -> x3d8_cosAngle
  float x18, x1c, x20, x24, x28, x2c, x30; // -> x3dc_pInfo[7]
  uint x34;                       // -> x424_contactDamage[0x0]
  float x38, x3c, x40, x44;       // -> x424_contactDamage[0x4..0x10]
  ushort x48, x4a, x4c;           // -> x424_contactDamage[0x14, 0x16, 0x18]
  uchar x4e;                      // -> x424_contactDamage[0x1a]
  float x50;                      // -> x444_
  uchar pad54[0x20];
  void* x74;                      // a CAi constructor stack argument
  uchar pad78[0x44];
  float xbc, xc0, xc4;            // -> x3fc_ (one CVector3f)
  uint xc8;                       // -> x490_
  uchar padcc[8];
  uint xd4;                       // -> x3b8_
  uint xd8, xdc;                  // two more CAi stack arguments; xdc != 0xffff selects the
                                  // 0x54-byte flavour of the x350_ allocation
  float xe0, xe4, xe8;            // -> x72c_ (one CVector3f)
  float xf0, xf4, xf8;            // -> x748_ (one CVector3f)
  TUniqueId xfc;                  // != 0xffff -> the first anim token (this+0x754)
  TUniqueId x100;                 // != 0xffff -> the second anim token (this+0x764)
  float x104, x108, x10c;         // -> x774_ (one CVector3f)
  TUniqueId x110;                 // != 0xffff -> the third anim token (this+0x780)
  float x114, x118, x11c;         // -> x494_[3] and the CUnitVector3f's source vector
  uchar x120[0x17c];              // the sub-object at this+0x4a0
  uchar pad29c[0x200];
  uint x29c;                      // handed to the sub-object at this+0x620
  uint x2a0;                      // -> x358_
  uchar x2a4;                     // bit 7 selects the CAABox pass
};

// The three anim tokens and the model's own anim token go through retail's helpers, none of which
// is written yet. Declared here so the shape of the code is right; the bodies are the blocker,
// not the declarations.
extern "C" {
// Six unnamed sub-object constructors. Sizes and arguments are measured; nothing names them.
void fn_8007A73C(uchar* self, const CPatternedInfo* pInfo);  // 0x4a0, 0x17c bytes, from pInfo+0x120
void fn_800FA748(uchar* self);                                 // 0x61c, 0x04 bytes
void fn_801BD654(uchar* self, const CPatternedInfo* pInfo);  // 0x620, 0x94 bytes, from pInfo+0x29c
void fn_801B7420(uchar* self);                                 // 0x6b4, 0x04 bytes
void fn_801B7E48(uchar* self);                                 // 0x6b8, 0x3c bytes
void fn_801B8C88(uchar* self);                                 // 0x6f4, 0x28 bytes
void fn_800747A4(CPatterned* self, uint a, uint b);           // (this, x4a4, x4a8)
void fn_80079904(CPatterned* self, EBodyType bodyType);
void fn_8004A284(CPatterned* self, int one, CAABox box, const CPatternedInfo* pInfo);
void fn_8023ACFC(CDamageVulnerability* out, const CPatternedInfo* pInfo, float f);
void fn_8002BE04(uchar* out, const CModelData* model, const rstl::string& name);
}

CPatterned::CPatterned(EPatternedAI ai, TUniqueId uid, const rstl::string& name, EFlavorType flavor,
                       const CEntityInfo& info, const CTransform4f& xf, const CModelData& mData,
                       const CPatternedInfo& pInfo, EMovementType moveType,
                       EColliderType colliderType, EBodyType bodyType,
                       const CActorParameters& actParams)
  : CAi(uid, name, info, 4, xf, mData, /*bounds*/ CAABox(sZeroVector__9CVector3f, sZeroVector__9CVector3f), /*mass*/ lbl_8041AAC0,
        /*hInfo*/ CHealthInfo(lbl_8041AAC0, lbl_8041AAC0), /*dVuln*/ CDamageVulnerability(CDamageVulnerability::NormalVulnerabilty()), /*matList*/ CMaterialList(),
        /*stateMachine*/ 0, /*stateMachine2*/ 0, actParams, lbl_8041AAC0, lbl_8041AAC0),
    x330_destObj(kInvalidUniqueId),
    x334_destPos(sZeroVector__9CVector3f),
    x340_(sZeroVector__9CVector3f),
    x34c_24_(false),
    x34c_25_flyer(moveType == kMT_Flyer),
    x34c_26_(false),
    x34c_27_(false),
    x34c_28_(false),
    x34c_29_(false),
    x34c_30_(true),
    x34c_31_(false),
    x34d_24_(true),
    x34d_25_(false),
    x34d_26_(true),
    x350_(nullptr),
    x354_patternedAI(ai),
    x358_(pInfo.x2a0),
    x35c_(lbl_8041AAC0),
    x360_(lbl_8041AAC0),
    x364_(lbl_8041AAC0),
    x368_(lbl_8041AAD0),
    x36c_damageVulnerability(CDamageVulnerability::NormalVulnerabilty()),
    x39c_(nullptr),
    x3a0_(sZeroVector__9CVector3f),
    x3ac_(sZeroVector__9CVector3f),
    x3b8_(pInfo.xd4),
    x3bc_(sZeroVector__9CVector3f),
    x3c8_pInfo(),
    x3d8_cosAngle(0.0f),
    x3dc_pInfo(),
    x3f8_(lbl_8041AAC0),
    x3fc_(CVector3f(pInfo.xbc, pInfo.xc0, pInfo.xc4)),
    x408_(),
    x41c_flavor(flavor),
    x420_flags(0),
    x424_(pInfo.x34),
    x428_(pInfo.x38),
    x42c_(pInfo.x3c),
    x430_(pInfo.x40),
    x434_(pInfo.x44),
    x438_(pInfo.x48),
    x43a_(pInfo.x4a),
    x43c_(pInfo.x4c),
    x43e_(pInfo.x4e),
    x440_(lbl_8041AAC0),
    x444_(pInfo.x50),
    x448_(lbl_8041AAD4),
    x44c_color(),
    x450_(0),
    x454_(sZeroVector__9CVector3f),
    x460_(sNoRotation__11CQuaternion),
    x470_animToken(),
    x488_(false),
    x48c_(0),
    x490_(pInfo.xc8),
    x494_(),
    x4a0_sub0(),
    x61c_sub1(),
    x620_sub2(),
    x6b4_sub3(),
    x6b8_sub4(),
    x6f4_sub5(),
    x71c_(sZeroVector__9CVector3f),
    x728_(lbl_8041AAC0),
    x72c_(CVector3f(pInfo.xe0, pInfo.xe4, pInfo.xe8)),
    x738_(lbl_8041AAC0),
    x73c_(lbl_8041AAC0),
    x740_colliderType(colliderType),
    x744_(lbl_8041AB24),
    x748_(CVector3f(pInfo.xf0, pInfo.xf4, pInfo.xf8)),
    x754_(),
    x760_hasAnimToken(false),
    x764_(),
    x770_hasToken2(false),
    x774_(CVector3f(pInfo.x104, pInfo.x108, pInfo.x10c)),
    x780_(),
    x78c_hasToken3(false),
    x790_(CVector3f(lbl_8041AAC8, lbl_8041AAC8, lbl_8041AAC8)),
    x79c_(CUnitVector3f(sForwardVector__9CVector3f)),
    x7a8_(0.0f),
    x7ac_(sZeroVector__9CVector3f),
    x7b8_(255),
    x7b9_pad() {
  // Everything below this line is retail's, in retail's order. Each block names the callee it
  // forwards to, so the next lane starts from a list rather than from 2904 bytes of assembly.
  x3c8_pInfo[0] = pInfo.x4;
  x3c8_pInfo[1] = pInfo.x8;
  x3c8_pInfo[2] = pInfo.xc;
  x3c8_pInfo[3] = pInfo.x10;
  x3d8_cosAngle = cosf(lbl_8041AB20 * pInfo.x14);
  x3dc_pInfo[0] = pInfo.x18;
  x3dc_pInfo[1] = pInfo.x1c;
  x3dc_pInfo[2] = pInfo.x20;
  x3dc_pInfo[3] = pInfo.x24;
  x3dc_pInfo[4] = pInfo.x28;
  x3dc_pInfo[5] = pInfo.x2c;
  x3dc_pInfo[6] = pInfo.x30;
  x408_[0] = lbl_8041AAC0;
  x408_[1] = lbl_8041AAC0;
  x408_[2] = lbl_8041AAC0;
  x408_[3] = lbl_8041AAC0;
  x408_[4] = lbl_8041AAC0;
  x494_[0] = pInfo.x114;
  x494_[1] = pInfo.x118;
  x494_[2] = pInfo.x11c;
  fn_8023ACFC(&x36c_damageVulnerability, &pInfo, lbl_8041AB0C);
  // this+0x60 is CActor::m_modelData; its +0x10 member's +0x110 is the anim token CPatterned
  // locks. Both indirections are retail's; the class of the intermediate is not identified.
  x470_animToken = CToken(*reinterpret_cast< CToken* >(
      *reinterpret_cast< uchar* const* >(
          *reinterpret_cast< uchar* const* const* >(&mData) + 0x10) + 0x110));
  x470_animToken.Lock();
  fn_8007A73C(x4a0_sub0, &pInfo);
  fn_800FA748(x61c_sub1);
  fn_801BD654(x620_sub2, &pInfo);
  fn_801B7420(x6b4_sub3);
  fn_801B7E48(x6b8_sub4);
  fn_801B8C88(x6f4_sub5);
  fn_800747A4(this, *reinterpret_cast< uint* >(x4a0_sub0 + 4),
              *reinterpret_cast< uint* >(x4a0_sub0 + 8));
  if (x430_ > lbl_8041AAC0) {
    x430_ = lbl_8041AAC0;
  }
}
