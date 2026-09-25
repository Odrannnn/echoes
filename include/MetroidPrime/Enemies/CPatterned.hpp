#ifndef _CPATTERNED
#define _CPATTERNED

#include "types.h"

#include "MetroidPrime/Enemies/CAi.hpp"

#include "Kyoto/Animation/CharacterCommon.hpp"
#include "Kyoto/CToken.hpp"
#include "Kyoto/Graphics/CColor.hpp"
#include "Kyoto/Math/CQuaternion.hpp"
#include "Kyoto/Math/CUnitVector3f.hpp"

class CPatternedInfo;
// The sub-object vtable slot 73 hands back by reference (0x754). Its real type is not identified;
// only its address matters to anything written so far. Its first member *is* now known: a
// CToken built from the anim token of pInfo+0xfc, guarded by a byte at 0x760.
class CPatternedAnimEvent;

// Retail passes 0x20 for Metaree; no other values are identified yet.
enum EPatternedAI {
  kPAI_Metaree = 0x20,
};

// Echoes' CPatterned: constructor at 0x80079BE4, vtable at 0x803B2458 (82 slots), size 0x7c0
// (the smallest first-member offset among ~40 creature modules; Metaree's start at 0x7c0).
//
// Status: the whole member list below is read out of the constructor's stores
// (0x80079BE4, 0xb58 bytes) - see docs/research/CPatterned_layout.txt. What is still missing is
// the constructor's *body*; ten of the class's own accessors, at 0x80073C58..0x80073CB4, are
// written in MetroidPrime/Enemies/CPatterned.cpp and that unit is Matching. The other virtuals in
// this range's neighbourhood (GetTouchBounds, GetOrigin, slots 70-72, 75) are not, and neither is
// the destructor at 0x80073978.
class CPatterned : public CAi {
public:
  enum EFlavorType {
    kFT_Zero,
    kFT_One,
  };
  enum EMovementType {
    kMT_Ground,
    kMT_Flyer, // selects the second of two static material lists in the constructor
  };
  enum EColliderType {
    kCT_Zero,
    kCT_One,
  };

  // Trilogy: __ct__10CPatternedF12EPatternedAI9TUniqueIdRC...basic_string...
  //          Q210CPatterned11EFlavorTypeRC11CEntityInfoRC12CTransform4fRC10CModelData
  //          RC14CPatternedInfoQ210CPatterned13EMovementTypeQ210CPatterned13EColliderType
  //          9EBodyTypeRC16CActorParameters
  CPatterned(EPatternedAI ai, TUniqueId uid, const rstl::string& name, EFlavorType flavor,
             const CEntityInfo& info, const CTransform4f& xf, const CModelData& mData,
             const CPatternedInfo& pInfo, EMovementType moveType, EColliderType colliderType,
             EBodyType bodyType, const CActorParameters& actParams);

  // Overrides, by retail vtable slot. The retail function is given where it has no label yet.
  ~CPatterned() override;                                                 // 2  fn_80073978
  CEntity* TypesMatch(int typeId) const override;                         // 3
  void PreThink(float dt, CStateManager& mgr) override;                   // 4  fn_80074018
  void Think(float dt, CStateManager& mgr) override;                      // 5  fn_80076D1C
  void AcceptScriptMsg(CStateManager& mgr, const CScriptMsg& msg) override; // 6 fn_80079568
  void PreRender(CStateManager& mgr, const CFrustumPlanes& planes) override; // 9 fn_800753AC
  void AddToRenderer(const CStateManager& mgr) const override;            // 10 fn_80073F64
  void Render(const CStateManager& mgr) const override;                   // 11 fn_80074F70
  bool CanRenderUnsorted(const CStateManager& mgr) const override;        // 12 fn_80075370
  void CalculateRenderBounds(CStateManager& mgr) override;                // 13 fn_80075224
  const CDamageVulnerability* GetDamageVulnerability() const override;    // 16 fn_800742F8
  const CDamageVulnerability* GetDamageVulnerability(const CVector3f&, const CVector3f&,
                                                     const CDamageInfo&) const override; // 17
  rstl::optional_object< CAABox > GetTouchBounds() const override;        // 18 fn_80073BF0
  void Touch(CActor& other, CStateManager& mgr) override;                 // 19 fn_80076B40
  CVector3f GetOrbitPosition(const CStateManager& mgr) const override;    // 20 fn_80075910
  CVector3f GetAimPosition(const CStateManager& mgr, float dt) const override; // 21 fn_8007594C
  EWeaponCollisionResponseTypes GetCollisionResponseType(const CVector3f&, const CVector3f&,
                                                         const CWeaponMode&,
                                                         int) const override; // 24 fn_8007403C
  void DoUserAnimEvent(CStateManager& mgr, const CInt32POINode& node, EUserEventType type,
                       float dt) override;                                // 28 fn_80076134
  CScannableObjectInfo* GetScannableObjectInfo() const override;          // 29 fn_8007427C
  void CollidedWith(const TUniqueId& id, const CCollisionInfoList& list,
                    CStateManager& mgr) override;                         // 33 fn_800766D4
  int PhysicsUnkVirtual() override;                                       // 36 fn_80073F58
  void Death(CStateManager& mgr, const CVector3f& direction,
             EScriptObjectState state) override;                          // 38 fn_80078DBC
  void KnockBack(CStateManager& mgr, const CKnockBackInfo& info) override; // 39 fn_800781A4
  void TakeDamage(const CVector3f& direction, float magnitude) override;  // 41 fn_80073C58

  // Slots 46-81 are CPatterned's own virtuals. Trilogy names ~110 CPatterned functions, but its
  // translation unit was built differently (its constructor is 0xf0c bytes against 0xb58 here), so
  // they cannot be assigned to these slots by position; each needs its body compared.
  virtual void VSlot46(); // fn_80074EF8
  virtual void VSlot47(); // fn_80074EB8
  virtual void VSlot48(); // fn_80075EFC
  virtual void VSlot49(); // fn_800765A8
  virtual uchar VSlot50(); // fn_80073C64
  virtual void VSlot51(); // fn_80076088
  virtual void VSlot52(); // fn_80075FDC
  virtual void VSlot53(); // fn_80078394
  virtual void VSlot54(); // fn_80078238
  virtual int VSlot55(); // fn_80073C6C
  virtual int VSlot56(); // fn_80073C74
  virtual int VSlot57(); // fn_80073C7C
  virtual void VSlot58(); // fn_80075CEC
  virtual void VSlot59(); // fn_80077814
  virtual void VSlot60(); // fn_8007989C
  virtual void VSlot61(); // fn_800358E0
  virtual void VSlot62(); // fn_8007477C
  virtual void VSlot63(); // fn_80074774
  virtual void VSlot64(); // fn_8007457C
  virtual void VSlot65(); // fn_80074480
  virtual void VSlot66(); // fn_8015180C
  virtual TUniqueId VSlot67(); // fn_80073C84
  virtual bool VSlot68(); // fn_80073C90
  virtual float VSlot69(); // fn_80073C9C
  virtual int VSlot70(); // fn_80073D0C
  virtual CAABox VSlot71(); // fn_80073CD4
  virtual void VSlot72(); // fn_80073CD0
  virtual CPatternedAnimEvent& VSlot73(); // fn_80073CA4
  virtual void VSlot74(); // fn_80075EC8
  virtual int VSlot75(); // fn_80073F50
  virtual void VSlot76(); // fn_80078A64
  virtual void VSlot77(); // fn_80152818
  virtual void VSlot78(); // fn_80074BD8
  virtual void VSlot79(); // fn_80074B00
  virtual void VSlot80(); // fn_80074AF8
  virtual void VSlot81(); // fn_80074BF8

private:
  // Offsets are absolute (CAi ends at 0x330). "pInfo+N" is the CPatternedInfo field copied in.
  TUniqueId x330_destObj;         // kInvalidUniqueId
  CVector3f x334_destPos;         // zero
  CVector3f x340_;                // zero
  // 0x34c and 0x34d hold eleven one-bit fields, written one at a time by the constructor
  // (three instructions each: lbz / rlwimi / stb). The values in order of emission are
  // false, (moveType == kMT_Flyer), false, false, kInvalidUniqueId&1, false, true, false,
  // true, false, true - measured from the source registers, see the file header.
  bool x34c_24_ : 1;              // false
  bool x34c_25_flyer : 1;         // moveType == kMT_Flyer
  bool x34c_26_ : 1;              // false
  bool x34c_27_ : 1;              // false
  bool x34c_28_ : 1;              // kInvalidUniqueId & 1; Metaree reads it (fn_42_36C)
  bool x34c_29_ : 1;              // false
  bool x34c_30_ : 1;              // true
  bool x34c_31_ : 1;              // false
  bool x34d_24_ : 1;              // true
  bool x34d_25_ : 1;              // false
  bool x34d_26_ : 1;              // true
  uchar x34e_pad[2];              // never written
  void* x350_;                    // new'd: 0x54 bytes if pInfo+0xdc is valid, else 0x40
  EPatternedAI x354_patternedAI;
  uint x358_;                     // pInfo+0x2a0
  float x35c_;                    // 0.0f
  float x360_;                    // 0.0f
  float x364_;                    // 0.0f
  float x368_;                    // 0.5f
  CDamageVulnerability x36c_damageVulnerability; // built from pInfo+0x140 with 0.8f
  CToken* x39c_;                  // heap: 12 bytes, from this+0x4ac's uid via a "SCAN" string.
                                  // 0 at first, then replaced at the end of the constructor with
                                  // the old one deleted. NOT an int.
  CVector3f x3a0_;                // zero
  CVector3f x3ac_;                // zero
  uint x3b8_;                     // pInfo+0xd4
  CVector3f x3bc_;                // zero
  float x3c8_pInfo[4];            // pInfo+0x4..0x10
  float x3d8_cosAngle;            // cos(pInfo+0x14 * (pi/180))
  float x3dc_pInfo[7];            // pInfo+0x18..0x30
  float x3f8_;                    // 0.0f
  CVector3f x3fc_;                // pInfo+0xbc
  float x408_[5];                 // 0.0f each
  EFlavorType x41c_flavor;
  // 26 one-bit fields, each written separately (4 instructions each: lwz / rlwimi / stw). Only
  // bits 1, 15, 16 and 17 are set; every other source is 0 or a mask whose bit 0 is clear, so
  // the word ends up 0x00018002.
  uint x420_flags;
  // pInfo+0x34..0x4e, copied field by field: a word, four floats, three halves and a byte. x430_
  // is the only one the constructor touches again, clamping it to 0 near the end.
  uint x424_;                      // pInfo+0x34
  float x428_;                     // pInfo+0x38
  float x42c_;                     // pInfo+0x3c
  float x430_;                     // pInfo+0x40, clamped to 0
  float x434_;                     // pInfo+0x44
  ushort x438_;                    // pInfo+0x48
  ushort x43a_;                    // pInfo+0x4a
  ushort x43c_;                    // pInfo+0x4c
  uchar x43e_;                     // pInfo+0x4e
  float x440_;                    // 0.0f
  float x444_;                    // pInfo+0x50
  float x448_;                    // -1.0f; Metaree's fn_42_324 resets it
  CColor x44c_color;              // Metaree's fn_42_334 reads its alpha byte
  uint x450_;
  CVector3f x454_;                // zero
  CQuaternion x460_;              // sNoRotation
  CToken x470_animToken;          // copy of the model's anim token, then Lock()
  uchar x478_pad[0x10];           // 0x478..0x488: never written
  bool x488_;                     // 0
  int x48c_;                      // 0
  uint x490_;                     // pInfo+0xc8
  uint x494_[3];                  // pInfo+0x114..0x11c
  // Six sub-objects, each built by its own constructor and nothing else. Sizes and the argument
  // each one takes are in docs/research/CPatterned_layout.txt; none of the six has a name yet.
  uchar x4a0_sub0[0x17c];         // 0x4a0..0x61b, from pInfo+0x120; has a TUniqueId at 0x4ac
  uchar x61c_sub1[0x04];          // 0x61c..0x61f, no argument
  uchar x620_sub2[0x94];          // 0x620..0x6b3, from pInfo+0x29c
  uchar x6b4_sub3[0x04];          // 0x6b4..0x6b7, no argument
  uchar x6b8_sub4[0x3c];          // 0x6b8..0x6f3, no argument
  uchar x6f4_sub5[0x28];          // 0x6f4..0x71b, no argument
  CVector3f x71c_;                // zero
  float x728_;                    // 0.0f
  CVector3f x72c_;                // pInfo+0xe0..0xe8
  float x738_;                    // 0.0f
  float x73c_;                    // 0.0f
  EColliderType x740_colliderType; // the colliderType argument, stored a second time
  float x744_;                    // 3.0f (slot 77 hands this back)
  CVector3f x748_;                // pInfo+0xf0..0xf8
  CToken x754_;                   // pInfo+0xfc through gpSimplePool, then Lock()
  uchar x75c_pad[4];              // never written
  uchar x760_hasAnimToken;        // 0, then 1 once x754_ is filled
  CToken x764_;                   // pInfo+0x100, same shape
  uchar x76c_pad[4];              // never written
  uchar x770_hasToken2;
  CVector3f x774_;                // pInfo+0x104..0x10c
  CToken x780_;                   // pInfo+0x110, same shape
  uchar x788_pad[4];              // never written
  uchar x78c_hasToken3;
  CVector3f x790_;                // 1.0f each
  CUnitVector3f x79c_;            // CUnitVector3f(sForwardVector)
  float x7a8_;                    // dot product of the zero vector with x79c_, i.e. 0.0f
  CVector3f x7ac_;                // zero
  uchar x7b8_;                    // 255, overwritten with a byte from the model when there is one
  uchar x7b9_pad[7];              // 0x7b9..0x7c0: never written
};
CHECK_SIZEOF(CPatterned, 0x7c0)

#endif // _CPATTERNED
