#ifndef _CPATTERNED
#define _CPATTERNED

#include "types.h"

#include "MetroidPrime/Enemies/CAi.hpp"

#include "Kyoto/Animation/CharacterCommon.hpp"
#include "Kyoto/Graphics/CColor.hpp"
#include "Kyoto/Math/CQuaternion.hpp"

class CPatternedInfo;

// Retail passes 0x20 for Metaree; no other values are identified yet.
enum EPatternedAI {
  kPAI_Metaree = 0x20,
};

// Echoes' CPatterned: constructor at 0x80079BE4, vtable at 0x803B2458 (82 slots), size 0x7c0
// (the smallest first-member offset among ~40 creature modules; Metaree's start at 0x7c0).
//
// Status: the constructor's signature, the size and the vtable's shape are established; the member
// list below is decoded from the constructor's stores only as far as 0x4a0. Like CAi, the
// constructor's translation unit has no range in config/G2ME01/splits.txt, so nothing here can be
// paired by objdiff yet.
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
  virtual void VSlot50(); // fn_80073C64
  virtual void VSlot51(); // fn_80076088
  virtual void VSlot52(); // fn_80075FDC
  virtual void VSlot53(); // fn_80078394
  virtual void VSlot54(); // fn_80078238
  virtual void VSlot55(); // fn_80073C6C
  virtual void VSlot56(); // fn_80073C74
  virtual void VSlot57(); // fn_80073C7C
  virtual void VSlot58(); // fn_80075CEC
  virtual void VSlot59(); // fn_80077814
  virtual void VSlot60(); // fn_8007989C
  virtual void VSlot61(); // fn_800358E0
  virtual void VSlot62(); // fn_8007477C
  virtual void VSlot63(); // fn_80074774
  virtual void VSlot64(); // fn_8007457C
  virtual void VSlot65(); // fn_80074480
  virtual void VSlot66(); // fn_8015180C
  virtual void VSlot67(); // fn_80073C84
  virtual void VSlot68(); // fn_80073C90
  virtual void VSlot69(); // fn_80073C9C
  virtual void VSlot70(); // fn_80073D0C
  virtual void VSlot71(); // fn_80073CD4
  virtual void VSlot72(); // fn_80073CD0
  virtual void VSlot73(); // fn_80073CA4
  virtual void VSlot74(); // fn_80075EC8
  virtual void VSlot75(); // fn_80073F50
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
  bool x34c_24_ : 1;              // false
  bool x34c_25_flyer : 1;         // moveType == kMT_Flyer
  bool x34c_26_ : 1;              // false
  bool x34c_27_ : 1;              // false
  bool x34c_28_notFlyer : 1;      // moveType != kMT_Flyer; Metaree reads it (fn_42_36C)
  bool x34c_29_ : 1;              // false
  bool x34c_30_ : 1;              // true
  bool x34c_31_ : 1;              // false
  bool x34d_24_ : 1;              // true
  bool x34d_25_ : 1;              // false
  bool x34d_26_ : 1;              // true
  void* x350_;                    // new'd: 0x54 bytes if pInfo+0xdc is valid, else 0x40
  EPatternedAI x354_patternedAI;
  uint x358_;                     // pInfo+0x2a0
  float x35c_;
  float x360_;
  float x364_;
  float x368_;
  CDamageVulnerability x36c_damageVulnerability; // built from pInfo+0x140
  int x39c_;
  CVector3f x3a0_;
  CVector3f x3ac_;
  uint x3b8_;                     // pInfo+0xd4
  CVector3f x3bc_;
  float x3c8_pInfo[4];            // pInfo+0x4..0x10
  float x3d8_cosAngle;            // cos(pInfo+0x14 * constant)
  float x3dc_pInfo[7];            // pInfo+0x18..0x30
  float x3f8_;
  CVector3f x3fc_;                // pInfo+0xbc
  float x408_[5];
  EFlavorType x41c_flavor;
  uint x420_flags;                // 26 one-bit fields (plus one two-bit), set bit by bit
  uchar x424_contactDamage[0x1c]; // pInfo+0x34..0x4e: word, four floats, three halves, a byte
  float x440_;
  float x444_;                    // pInfo+0x50
  float x448_;                    // Metaree's fn_42_324 resets it
  CColor x44c_color;              // Metaree's fn_42_334 reads its alpha byte
  uint x450_;
  CVector3f x454_;
  CQuaternion x460_;
  uchar x470_token[0xc];          // token from the model's animation data (+0x110), locked
  uchar x47c_pad[0xc];
  bool x488_;
  int x48c_;
  uint x490_;                     // pInfo+0xc8
  uint x494_[3];                  // pInfo+0x114..0x11c
  // Not decoded. Sub-objects are constructed at 0x4a0 (from pInfo+0x120), 0x61c, 0x620
  // (pInfo+0x29c), 0x6b4, 0x6b8 and 0x6f4; 0x71c is a zero CVector3f; Metaree takes the address
  // of 0x754 (fn_42_384).
  uchar x4a0_undecoded[0x7c0 - 0x4a0];
};
CHECK_SIZEOF(CPatterned, 0x7c0)

#endif // _CPATTERNED
