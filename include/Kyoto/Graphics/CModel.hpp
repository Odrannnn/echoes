#ifndef _CMODEL
#define _CMODEL

#include "types.h"

#include "Kyoto/CFactoryMgr.hpp"
#include "Kyoto/Graphics/CTexture.hpp"
#include "Kyoto/TToken.hpp"

#include "rstl/auto_ptr.hpp"
#include "rstl/single_ptr.hpp"
#include "rstl/vector.hpp"

class CAABox;
class CCubeModel;
class CModelFlags;
class IObjectStore;

class CModel {
  struct SShader;
  friend struct SShader;

  // Echoes tracks texture timeouts per material set instead of per model.
  struct SShader {
    rstl::vector< TCachedToken< CTexture > > mTextures;
    uchar* mData;
    CModel* mOwner;
    SShader* mPrev;
    SShader* mNext;

    SShader(uchar* data, CModel* owner);
    SShader(const SShader& other);
    ~SShader();

    void UnlockTextures();
    void RemoveFromList();
    void MoveToThisFrameList();
  };

  static uint sTotalMemory;
  static SShader* sThisFrameList;
  static SShader* sOneFrameList;
  static SShader* sTwoFrameList;

public:
  enum EDrawFlatFlags {
    kDF_Unknown0,
  };

  CModel(const rstl::auto_ptr< uchar >& data, int length, IObjectStore& store);
#ifdef TARGET_PC
  // Port: an empty model for `port::pool::CreateStandInObject` (src/MetroidPrime/PortPoolStandIns.cpp).
  CModel() : mDataLen(0), mLastFrame(0), mCurrentMatxIdx(0), x30_16_(0), mHasSkinMatrices(0) {}
#endif
  ~CModel();
  void Touch(int) const;
  void Draw(const CModelFlags&) const;
  void Draw(u64 mask, const CModelFlags& flags) const;
  void DrawUnsortedParts(const CModelFlags& flags) const;
  void DrawSortedParts(const CModelFlags& flags) const;
  void DolphinDrawFlat(EDrawFlatFlags flags) const;
  void PreDrawModel(const CModelFlags& flags) const;
  bool IsLoaded(int matIdx) const;
  const CAABox& GetAABB() const;
  const float* GetPositions() const;
  const float* GetNormals() const;
  const CCubeModel* GetModelInstance() const { return mModelInstance.get(); }
  void UpdateLastFrame() const;
  void VerifyCurrentShader(int shader) const;
  // Retail buffer relocation methods; names are inferred from their implementations.
  rstl::auto_ptr< uchar > GetData();
  uint GetDataSize() const;
  void RemapData(uchar* data);

  static void DisableTextureTimeout();
  static void EnableTextureTimeout();
  static void FrameDone();
  static void AddToTotal(uint amt) { sTotalMemory += amt; }
  static void RemoveFromTotal(uint amt) { sTotalMemory -= amt; }
  static uint GetTotalMemory() { return sTotalMemory; }

#ifdef TARGET_PC
  /**
   * The number of material sets, i.e. the number of `Touch` steps retail's touch-everything
   * loops (`fn_80027AE8`, and `fn_80027B44` for one index) walk. Pre-upstream this header
   * modelled retail offset 0x1C as `int x1c_numParts`; upstream recovered `CModel` and that
   * word is `mMatSets`'s count. `rstl::vector` is `{ rmemory_allocator, int mCount, int
   * mCapacity, T* mItems }` and `rstl::single_ptr` is one pointer, so in the 32-bit layout
   * `mSurfaces` spans 0x08..0x17, `mMatSets` starts at 0x18 and its `mCount` lands on 0x1C -
   * and `CHECK_SIZEOF(CModel, 0x34)` below only closes if every one of those sizes is the one
   * claimed. The port's touch loops still need the count
   * (src/MetroidPrime/CModelTouchParts.cpp, src/MetroidPrime/Player/CGunEffectTouchAll.cpp), so
   * it is exposed here rather than by making `mMatSets` public.
   */
  int GetMatSetCount() const { return mMatSets.mCount; }
#endif

private:
  void* SetupSkinMatrices() const;

  rstl::single_ptr< uchar > mData;
  uint mDataLen;
  rstl::vector< void* > mSurfaces;
  mutable rstl::vector< SShader > mMatSets;
  rstl::single_ptr< CCubeModel > mModelInstance;
  mutable uint mLastFrame;
  mutable uint mCurrentMatxIdx : 16;
  uint x30_16_ : 1;
  uint mHasSkinMatrices : 1;
};
CHECK_SIZEOF(CModel, 0x34)

const CFactoryFnReturn FModelFactory(const SObjectTag& tag, const rstl::auto_ptr< uchar >& ptr,
                                     int len, const CVParamTransfer& xfer);

#endif // _CMODEL
