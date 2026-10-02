// Retail installs the SIndexLoad handle through the out-of-line
// `single_ptr<CMoviePlayer::SIndexLoad>::operator=(T* const)` (0x8031E9D8, 72 bytes, called at
// 0x8031CD0) while inlining that same member for every other single_ptr in the TU. The opt-in is
// per translation unit, so take it and write the other nine call sites out by hand - see
// `mp_assign` below. See include/rstl/single_ptr.hpp.
#define RSTL_SINGLE_PTR_ASSIGN_OUT_OF_LINE 1
#include "Kyoto/Basics/CBasics.hpp"
#include "Kyoto/Alloc/LockedCache.hpp"
#include "Kyoto/Math/CVector3f.hpp"

#include <Kyoto/Graphics/CMoviePlayer.hpp>

#include "Kyoto/Audio/CAudioSys.hpp"
#include "Kyoto/Audio/CStaticAudioPlayer.hpp"
#include "Kyoto/Graphics/CGraphics.hpp"
#include "Kyoto/Math/CMath.hpp"
#include "dolphin/PPCArch.h"
#include "dolphin/ai.h"
#include "dolphin/gx/GXVert.h"
#include "dolphin/os.h"
#include "dolphin/thp/THPDec.h"
#include <Kyoto/Graphics/CGX.hpp>
#include <Kyoto/Graphics/CTexture.hpp>
#include <math.h>
#include <rstl/math.hpp>
#include <string.h>

#include "dolphin/gx/GXGeometry.h"
#include "dolphin/gx/GXTev.h"
#include "dolphin/gx/GXTexture.h"

extern bool lbl_804199CC;
extern bool lbl_80419B9D;

static rstl::string SelectMoviePath(const char* path) {
  rstl::string name(path);
  if (lbl_804199CC) {
    rstl::string palName = name + "_pal";
    if (CDvdFile::FileExists(palName.data())) {
      return palName;
    }
  }
  return name;
}

static bool ShouldEnableLockedCache() {
  return LCGetBase() != GetLockedCacheAllocationBase();
}

class CInterruptGuard {
  bool mEnabled;

public:
  CInterruptGuard() : mEnabled(OSDisableInterrupts()) {}
  ~CInterruptGuard() { OSRestoreInterrupts(mEnabled); }
};

static int sNumReferences = 0;
static CMoviePlayer* sAudioPlayer;
static const short* curAudioBuffer;
static int soundBufferIndex;
static short soundBuffer[2][320] ATTRIBUTE_ALIGN(32);
static bool sAudioEnabled = true;
static uchar sSfxVolume = 127;

struct CMoviePlayer::SIndexLoad {
  rstl::single_ptr< CDvdRequest > mHeaderRequest;
  rstl::single_ptr< CDvdRequest > mVideoRequest;
  rstl::single_ptr< CDvdRequest > mAudioRequest;
  rstl::single_ptr< uchar > mBuffer;
  int mState;

  ~SIndexLoad();

  SIndexLoad()
  : mBuffer(static_cast< uchar* >(CMemory::Alloc(64, IAllocator::kHI_RoundUpLen)))
  , mState(0) {}
};
CMoviePlayer::SIndexLoad::~SIndexLoad() {}

CMoviePlayer::CTHPTextureSet::CTHPTextureSet(void* y, void* u, void* v, void* audio)
: mY(static_cast< uchar* >(y))
, mU(static_cast< uchar* >(u))
, mV(static_cast< uchar* >(v))
, mAudio(static_cast< uchar* >(audio))
, mAudioSamples(0)
, mAudioSamplesConsumed(0) {}

CMoviePlayer::CTHPTextureSet::CTHPTextureSet(const CTHPTextureSet& other)
: mY(other.mY)
, mU(other.mU)
, mV(other.mV)
, mAudio(other.mAudio)
, mAudioSamples(other.mAudioSamples)
, mAudioSamplesConsumed(other.mAudioSamplesConsumed) {}

CMoviePlayer::CTHPTextureSet::~CTHPTextureSet() {}

// Hand-rolled instantiations. Retail carries these as unnamed weak symbols
// (fn_*), emitted from rstl::vector/construct templates that our headers do not
// reproduce exactly. Writing them out under retail's names matches the bytes; the
// proper fix is shaping the templates (see include/rstl/construct.hpp for the same
// problem already solved that way) and this block should go when that happens.
// Names are retail's, the bodies are reconstructions.
extern "C" void fn_803193F8(CMoviePlayer::CTHPTextureSet* dst,
                            const CMoviePlayer::CTHPTextureSet* src) {
  rstl::construct(dst, *src);
}

extern "C" void fn_803193D8(CMoviePlayer::CTHPTextureSet* dst,
                            const CMoviePlayer::CTHPTextureSet* src) {
  fn_803193F8(dst, src);
}

extern "C" void fn_803193A0(
    rstl::vector< CMoviePlayer::CTHPTextureSet >* textures,
    const CMoviePlayer::CTHPTextureSet* texture) {
  fn_803193D8(textures->mItems + textures->mCount++, texture);
}

typedef CMoviePlayer::CTHPTextureSet CMovieTexture;
typedef rstl::vector< CMovieTexture > CMovieTextureVector;
struct CMovieTextureIterator {
  CMovieTexture* x0_owner;
  CMovieTexture* x4_current;

  explicit CMovieTextureIterator(CMovieTexture* current)
  : x0_owner(current), x4_current(current) {}
};

// The two bounds travel to fn_80317FF8 as one local: retail's `clear`, `~vector` and `reserve`
// all hold them at consecutive frame slots with the **end** first (`r1+8`, `r1+12`, then the
// first element at `r1+16`, `r1+20`), and pass `&last.x4_current` as the second argument. Two
// separate locals get their slots the other way round - mwcceppc hands them out in reverse
// declaration order - which is what put the copy before the destroy arguments in the wrong
// order; a struct's members are laid out in declaration order, which is retail's. `x0_owner`
// no longer needs to be `volatile` to get its store emitted: with both bounds in one object
// all four stores appear anyway (measured: clear 78.33 -> 78.83, reserve 75.44 -> 91.26,
// ~vector 90.06 -> 90.30).
struct CMovieTextureRange {
  CMovieTextureIterator last;
  CMovieTextureIterator first;

  CMovieTextureRange(CMovieTexture* items, int count)
  : last(items + count), first(items) {}
};

extern "C" void fn_803180A0(CMovieTexture* texture) { texture->~CMovieTexture(); }

extern "C" void fn_80318080(CMovieTexture* texture) { fn_803180A0(texture); }

extern "C" void fn_80318030(CMovieTexture** first, CMovieTexture** last) {
  CMovieTexture* current = *first;
  CMovieTexture** const end = last;
  while (current != *end) {
    fn_80318080(current);
    ++current;
  }
}

// The two bounds are one 8-byte local here too (end at `r1+8`, begin at `r1+12`), not two
// separate locals: that is what puts the `addi` for the second argument ahead of the first
// (measured 71.00 -> 77.00; the remaining 2 instructions are the load of `*last` into r5 that
// retail schedules before saving LR).
extern "C" void fn_80317FF8(CMovieTexture** first, CMovieTexture** last) {
  struct {
    CMovieTexture* end;
    CMovieTexture* begin;
  } it;
  it.end = *last;
  it.begin = *first;
  fn_80318030(&it.begin, &it.end);
}

// Hoisting the loop cursor into a local (rather than walking the parameter) is what puts it
// in r31 and the bound in r30, as retail has it; walking the parameter swaps the two.
extern "C" void fn_8031A8AC(CMovieTexture* first, CMovieTexture* last) {
  CMovieTexture* it = first;
  while (it != last) {
    fn_80318080(it);
    ++it;
  }
}

extern "C" void fn_8031A88C(CMovieTexture* first, CMovieTexture* last) {
  fn_8031A8AC(first, last);
}

// The rstl::vector< rstl::auto_ptr< uchar > > element loops, as retail emits them: the
// element destructor inlined (so nothing calls __dt__Q24rstl12auto_ptr<Uc>Fv), and the
// copy that hands ownership over, which also clears the source's owner flag.
// ~vector calls the first, reserve the second.
typedef rstl::auto_ptr< uchar > CMovieBuffer;
typedef rstl::vector< CMovieBuffer > CMovieBufferVector;
typedef CMovieBufferVector::iterator CMovieBufferIterator;

// Retail's `fn_80319CB4` is `rstl::destroy` with the range's iterators taken **by value**, and
// that is load-bearing: the MWCC ABI hands a non-POD class parameter by address, so the copies
// land in the callee's frame (the two dead `stw`s retail has) and the bound is then held in a
// register for the whole loop. Passing the iterators by pointer, or calling `rstl::destroy`
// directly, leaves the bound in memory and re-reads it every iteration instead.
template < typename It >
static inline void mp_destroy_val(It begin, It end) {
  It cur = begin;
  for (; cur != end; ++cur) {
    rstl::destroy(&*cur);
  }
}

extern "C" void fn_80319CB4(CMovieBufferIterator begin, CMovieBufferIterator end) {
  mp_destroy_val(begin, end);
}

extern "C" CMovieBuffer* fn_8031AA24(CMovieBuffer** first, CMovieBuffer** last,
                                     CMovieBuffer* out) {
  CMovieBuffer* source = *first;
  CMovieBuffer* const end = *last;
  for (; source != end; ++source, ++out) {
    rstl::construct(out, *source);
  }
  return out;
}

extern "C" CMovieTexture* fn_8031A8F8(CMovieTexture** first, CMovieTexture** last,
                                      CMovieTexture* destination) {
  CMovieTexture* source = *first;
  CMovieTexture* output = destination;
  while (source != *last) {
    fn_803193D8(output, source);
    ++source;
    ++output;
  }
  return output;
}

template <>
void CMovieTextureVector::clear() {
  CMovieTextureRange range(mItems, mCount);
  fn_80317FF8(&range.first.x4_current, &range.last.x4_current);
  mCount = 0;
}

template <>
CMovieTextureVector::~vector() {
  CMovieTextureRange range(mItems, mCount);
  fn_80317FF8(&range.first.x4_current, &range.last.x4_current);
  mAllocator.deallocate(mItems);
}

template <>
void CMovieTextureVector::reserve(int newSize) {
  if (newSize <= mCapacity) {
    return;
  }

  CMovieTexture* newData;
  mAllocator.allocate(newData, newSize);
  {
    // The copy takes its bounds by address, so they are the address-taken range; the destroy
    // that follows takes them by value and retail passes them in registers, which it cannot do
    // for the same variables. (Reusing the two locals for both calls is what held the second
    // call's arguments in memory and cost this function 16 bytes of frame.)
    CMovieTextureRange range(mItems, mCount);
    fn_8031A8F8(&range.first.x4_current, &range.last.x4_current, newData);
  }
  fn_8031A88C(mItems, mItems + mCount);
  mAllocator.deallocate(mItems);
  mItems = newData;
  mCapacity = newSize;
}

const unsigned char skInterlacePattern[32] = {
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

static void MyTHPGXRestore() {
  CGX::SetZMode(TRUE, GX_ALWAYS, FALSE);
  CGX::SetBlendMode(GX_BM_NONE, GX_BL_ONE, GX_BL_ZERO, GX_LO_SET);
  CGX::SetNumTexGens(1);
  CGX::SetNumChans(0);
  CGX::SetNumTevStages(1);
  CGX::SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR_NULL);
  CGX::SetAlphaCompare(GX_ALWAYS, 0, GX_AOP_AND, GX_ALWAYS, 0);
}

static void MyTHPGXYuv2RgbSetup(bool field, bool deinterlace) {
  GXVtxDescList attr[3] = {
      {GX_VA_POS, GX_DIRECT},
      {GX_VA_TEX0, GX_DIRECT},
      {GX_VA_NULL, GX_NONE},
  };
  CGX::SetZMode(TRUE, GX_ALWAYS, FALSE);
  CGX::SetBlendMode(GX_BM_NONE, GX_BL_ONE, GX_BL_ZERO, GX_LO_CLEAR);
  CGX::SetNumChans(0);
  CGX::SetTexCoordGen(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY, FALSE, GX_PTIDENTITY);
  CGX::SetTexCoordGen(GX_TEXCOORD1, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY, FALSE, GX_PTIDENTITY);

  if (deinterlace) {
    CGX::SetNumTexGens(2);
    CGX::SetNumTevStages(4);
  } else {
    CGX::SetNumTexGens(3);
    CGX::SetTexCoordGen(GX_TEXCOORD2, GX_TG_MTX2x4, GX_TG_POS, GX_TEXMTX0, FALSE, GX_PTIDENTITY);

    float n = field ? 0.25f : 0.f;
    float mtx[8] = {0.125f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.25f, n};
    GXLoadTexMtxImm(reinterpret_cast< MtxPtr >(mtx), GX_TEXMTX0, GX_MTX2x4);
    GXTexObj obj;
    GXInitTexObj(&obj, skInterlacePattern, 8, 4, GX_TF_I8, GX_REPEAT, GX_REPEAT, FALSE);
    GXInitTexObjLOD(&obj, GX_NEAR, GX_NEAR, 0.f, 0.f, 0.f, FALSE, FALSE, GX_ANISO_1);
    GXLoadTexObj(&obj, GX_TEXMAP3);
    CTexture::InvalidateTexmap(GX_TEXMAP3);
    CGX::SetTevOrder(GX_TEVSTAGE4, GX_TEXCOORD2, GX_TEXMAP3, GX_COLOR_NULL);
    CGX::SetStandardTevColorAlphaOp(GX_TEVSTAGE4);
    CGX::SetTevColorIn(GX_TEVSTAGE4, GX_CC_ZERO, GX_CC_ZERO, GX_CC_ZERO, GX_CC_CPREV);
    CGX::SetTevAlphaIn(GX_TEVSTAGE4, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_TEXA);
    CGX::SetAlphaCompare(GX_LESS, 128, GX_AOP_AND, GX_ALWAYS, 0);
    CGX::SetNumTevStages(5);
  }

  CGX::SetVtxDescv(attr);
  GXSetColorUpdate(TRUE);
  GXSetAlphaUpdate(FALSE);
  GXInvalidateTexAll();
  GXSetVtxAttrFmt(GX_VTXFMT7, GX_VA_POS, GX_CLR_RGBA, GX_F32, 0);
  GXSetVtxAttrFmt(GX_VTXFMT7, GX_VA_TEX0, GX_CLR_RGBA, GX_RGBX8, 0);
  CGX::SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD1, GX_TEXMAP1, GX_COLOR_NULL);
  CGX::SetTevColorIn(GX_TEVSTAGE0, GX_CC_ZERO, GX_CC_TEXC, GX_CC_KONST, GX_CC_C0);
  CGX::SetTevColorOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, false, GX_TEVPREV);
  CGX::SetTevAlphaIn(GX_TEVSTAGE0, GX_CA_ZERO, GX_CA_TEXA, GX_CA_KONST, GX_CA_A0);
  CGX::SetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_SUB, GX_TB_ZERO, GX_CS_SCALE_1, false, GX_TEVPREV);
  CGX::SetTevKColorSel(GX_TEVSTAGE0, GX_TEV_KCSEL_K0);
  CGX::SetTevKAlphaSel(GX_TEVSTAGE0, GX_TEV_KASEL_K0_A);
  CGX::SetTevOrder(GX_TEVSTAGE1, GX_TEXCOORD1, GX_TEXMAP2, GX_COLOR_NULL);
  CGX::SetTevColorIn(GX_TEVSTAGE1, GX_CC_ZERO, GX_CC_TEXC, GX_CC_KONST, GX_CC_CPREV);
  CGX::SetTevColorOp(GX_TEVSTAGE1, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_2, false, GX_TEVPREV);
  CGX::SetTevAlphaIn(GX_TEVSTAGE1, GX_CA_ZERO, GX_CA_TEXA, GX_CA_KONST, GX_CA_APREV);
  CGX::SetTevAlphaOp(GX_TEVSTAGE1, GX_TEV_SUB, GX_TB_ZERO, GX_CS_SCALE_1, false, GX_TEVPREV);
  CGX::SetTevKColorSel(GX_TEVSTAGE1, GX_TEV_KCSEL_K1);
  CGX::SetTevKAlphaSel(GX_TEVSTAGE1, GX_TEV_KASEL_K1_A);
  CGX::SetTevOrder(GX_TEVSTAGE2, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR_NULL);
  CGX::SetTevColorIn(GX_TEVSTAGE2, GX_CC_ZERO, GX_CC_TEXC, GX_CC_ONE, GX_CC_CPREV);
  CGX::SetTevColorOp(GX_TEVSTAGE2, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, true, GX_TEVPREV);
  CGX::SetTevAlphaIn(GX_TEVSTAGE2, GX_CA_TEXA, GX_CA_ZERO, GX_CA_ZERO, GX_CA_APREV);
  CGX::SetTevAlphaOp(GX_TEVSTAGE2, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, true, GX_TEVPREV);
  CGX::SetTevOrder(GX_TEVSTAGE3, GX_TEXCOORD_NULL, GX_TEXMAP_NULL, GX_COLOR_NULL);
  CGX::SetTevColorIn(GX_TEVSTAGE3, GX_CC_APREV, GX_CC_CPREV, GX_CC_KONST, GX_CC_ZERO);
  CGX::SetTevColorOp(GX_TEVSTAGE3, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, true, GX_TEVPREV);
  CGX::SetTevAlphaIn(GX_TEVSTAGE3, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO);
  CGX::SetTevAlphaOp(GX_TEVSTAGE3, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, true, GX_TEVPREV);
  CGX::SetTevKColorSel(GX_TEVSTAGE3, GX_TEV_KCSEL_K2);
  const GXColorS10 color = {-90, 0, -114, 135};
  GXSetTevColorS10(GX_TEVREG0, color);
  const GXColor kColor0 = {0, 0, 226, 88};
  CGX::SetTevKColor(GX_KCOLOR0, kColor0);
  const GXColor kColor1 = {179, 0, 0, 182};
  CGX::SetTevKColor(GX_KCOLOR1, kColor1);
  const GXColor kColor2 = {255, 0, 255, 128};
  CGX::SetTevKColor(GX_KCOLOR2, kColor2);
}

static void MyTHPYuv2RgbTextureSetup(void* y, void* u, void* v, ushort width, ushort height) {
  GXTexObj yTex;
  GXInitTexObj(&yTex, y, width, height, GX_TF_I8, GX_CLAMP, GX_CLAMP, FALSE);
  GXInitTexObjLOD(&yTex, GX_NEAR, GX_NEAR, 0.f, 0.f, 0.f, FALSE, FALSE, GX_ANISO_1);
  GXLoadTexObj(&yTex, GX_TEXMAP0);

  GXTexObj uTex;
  GXInitTexObj(&uTex, u, width / 2, height / 2, GX_TF_I8, GX_CLAMP, GX_CLAMP, FALSE);
  GXInitTexObjLOD(&uTex, GX_NEAR, GX_NEAR, 0.f, 0.f, 0.f, FALSE, FALSE, GX_ANISO_1);
  GXLoadTexObj(&uTex, GX_TEXMAP1);

  GXTexObj vTex;
  GXInitTexObj(&vTex, v, static_cast< s16 >(width / 2), height / 2, GX_TF_I8, GX_CLAMP, GX_CLAMP,
               FALSE);
  GXInitTexObjLOD(&vTex, GX_NEAR, GX_NEAR, 0.f, 0.f, 0.f, FALSE, FALSE, GX_ANISO_1);
  GXLoadTexObj(&vTex, GX_TEXMAP2);

  CTexture::InvalidateTexmap(GX_TEXMAP0);
  CTexture::InvalidateTexmap(GX_TEXMAP1);
  CTexture::InvalidateTexmap(GX_TEXMAP2);
}

// `RSTL_SINGLE_PTR_ASSIGN_OUT_OF_LINE` above moves every `single_ptr<T>::operator=(T* const)` in
// this TU out of line, and retail only calls one of them that way, so the rest are written out
// here: the body is the member's own, so the bytes are the ones mwcceppc produced when it inlined
// it. Every call site that must stay inlined has to go through this - measured, leaving any of
// them as a plain `=` emits a second weak `__as__...` copy and drops that function out of 100%
// (`PostDVDReadRequestIfNeeded` 100 -> 94.42, `PrefetchNextFrame` 100 -> 94.60, `Rewind`
// 100 -> 84.29). With all of them converted the object carries the one weak `__as__` symbol
// retail's does, and nothing else changes.
template < typename T >
static inline void mp_assign(rstl::single_ptr< T >& slot, T* const ptr) {
  delete slot.mPtr;
  slot.mPtr = ptr;
}

CMoviePlayer::CMoviePlayer(const char* path, const float preLoadSeconds, const bool loop,
                           const bool deinterlace)
: mDvdFile(SelectMoviePath(path).data())
, mIndexLoad(rs_new SIndexLoad)
, mNextReadSize(0)
, mNextReadOff(0)
, mReadSizeWrapped(0)
, mReadOffWrapped(0)
, mCurLoadFrame(0)
, mRequestFrameWrapped(0)
, mCurFrame(0)
, mDecodedTexSlot(0)
, mDrawTexSlot(-1)
, mAudioSlot(-1)
, mDecodedTexCount(0)
, mFrameRem(0.f)
, mPrefetchFrame(0)
, mPrefetchOff(0)
, mPrefetchSize(0)
, mPlayMode(kPM_Playing)
, mTotalSeconds(0.f)
, mCurSeconds(0.f)
, mPreLoadSeconds(preLoadSeconds)
, mPreLoadFrames(0)
, mLoop(loop)
, mDeinterlace(deinterlace)
, mIs60Hz(false)
, mHasAudio(false)
, mFieldFlip(false)
, mCachedBytes(0)
, mFieldIndex(0)
, mVolume(127) {
  if (sNumReferences == 0 && ShouldEnableLockedCache()) {
    LCEnable();
  }

  static bool sThpInitialized = false;
  if (!sThpInitialized) {
    sThpInitialized = true;
    THPInit();
  }
  ++sNumReferences;
  VerifyCallbackStatus();
  mp_assign(mIndexLoad->mHeaderRequest, mDvdFile.SyncRead(mIndexLoad->mBuffer.get(), 64));
}

bool CMoviePlayer::ContinueLoading() {
  if (mIndexLoad.null()) {
    return false;
  }
  uchar* const buffer = mIndexLoad->mBuffer.get();
  bool hasVideo;
  bool hasAudio;
  switch (mIndexLoad->mState) {
  case 0:
    if (mIndexLoad->mHeaderRequest->IsComplete()) {
      memcpy(&mHeader, buffer, sizeof(THPHeader));
      mHeader.mVersion = CBasics::SwapBytes(static_cast< uint >(mHeader.mVersion));
      mHeader.mBufferSize = CBasics::SwapBytes(static_cast< uint >(mHeader.mBufferSize));
      mHeader.mAudioMaxSamples =
          CBasics::SwapBytes(static_cast< uint >(mHeader.mAudioMaxSamples));
      mHeader.mFrameRate = CBasics::SwapBytes(mHeader.mFrameRate);
      mHeader.mNumFrames = CBasics::SwapBytes(static_cast< uint >(mHeader.mNumFrames));
      mHeader.mFirstFrameSize =
          CBasics::SwapBytes(static_cast< uint >(mHeader.mFirstFrameSize));
      mHeader.mMovieDataSize =
          CBasics::SwapBytes(static_cast< uint >(mHeader.mMovieDataSize));
      mHeader.mCompInfoDataOffsets =
          CBasics::SwapBytes(static_cast< uint >(mHeader.mCompInfoDataOffsets));
      mHeader.mOffsetDataOffsets =
          CBasics::SwapBytes(static_cast< uint >(mHeader.mOffsetDataOffsets));
      mHeader.mMovieDataOffsets =
          CBasics::SwapBytes(static_cast< uint >(mHeader.mMovieDataOffsets));
      mHeader.mFinalFrameDataOffsets =
          CBasics::SwapBytes(static_cast< uint >(mHeader.mFinalFrameDataOffsets));
      if (fabsf(mHeader.mFrameRate - 29.97f) < 0.00001f) {
        mHeader.mFrameRate = 30.f;
      }
      if (fabsf(mHeader.mFrameRate - 59.94f) < 0.00001f) {
        mHeader.mFrameRate = 60.f;
      }
      if (fabsf(mHeader.mFrameRate - 60.f) < 0.00001f) {
        mIs60Hz = true;
      }
      mp_assign(mIndexLoad->mHeaderRequest,
                mDvdFile.AsyncSeekRead(buffer, 32, kSO_Set, mHeader.mCompInfoDataOffsets));
      ++mIndexLoad->mState;
    } else {
      return true;
    }
  case 1: {
    if (mIndexLoad->mHeaderRequest->IsComplete()) {
      memcpy(&mThpComponents, buffer, sizeof(THPFrameCompInfo));
      mThpComponents.mNumComponents =
          CBasics::SwapBytes(static_cast< uint >(mThpComponents.mNumComponents));
      mp_assign(mIndexLoad->mHeaderRequest, static_cast< CDvdRequest* >(nullptr));
      uchar* audioBuffer = buffer + 32;
      int offset = mHeader.mCompInfoDataOffsets + sizeof(THPFrameCompInfo);
      for (uint i = 0; i < mThpComponents.mNumComponents; ++i) {
        switch (mThpComponents.mFrameComp[i]) {
        case 0:
          mp_assign(mIndexLoad->mVideoRequest, mDvdFile.AsyncSeekRead(buffer, 32, kSO_Set, offset));
          offset += sizeof(THPVideoInfo);
          break;
        case 1:
          mp_assign(mIndexLoad->mAudioRequest,
                    mDvdFile.AsyncSeekRead(audioBuffer, 32, kSO_Set, offset));
          offset += sizeof(THPAudioInfo);
          mHasAudio = true;
          break;
        }
      }
      ++mIndexLoad->mState;
    } else {
      return true;
    }
  }
  case 2: {
    bool complete = true;
    hasVideo = mIndexLoad->mVideoRequest.get() != nullptr;
    hasAudio = mIndexLoad->mAudioRequest.get() != nullptr;
    if (hasVideo && !mIndexLoad->mVideoRequest->IsComplete()) {
      complete = false;
    }
    if (hasAudio && !mIndexLoad->mAudioRequest->IsComplete()) {
      complete = false;
    }
    if (!complete) {
      return true;
    }
    if (hasVideo) {
      memcpy(&mVideoInfo, buffer, sizeof(THPVideoInfo));
      mVideoInfo.mXSize = CBasics::SwapBytes(static_cast< uint >(mVideoInfo.mXSize));
      mVideoInfo.mYSize = CBasics::SwapBytes(static_cast< uint >(mVideoInfo.mYSize));
    }
    if (hasAudio) {
      memcpy(&mAudioInfo, buffer + 32, sizeof(THPAudioInfo));
      mAudioInfo.mSndChannels =
          CBasics::SwapBytes(static_cast< uint >(mAudioInfo.mSndChannels));
      mAudioInfo.mSndFrequency =
          CBasics::SwapBytes(static_cast< uint >(mAudioInfo.mSndFrequency));
      mAudioInfo.mSndNumSamples =
          CBasics::SwapBytes(static_cast< uint >(mAudioInfo.mSndNumSamples));
    }
  }
  }

  mIndexLoad = nullptr;
  mTextures.reserve(3);
  mNextReadOff = mHeader.mMovieDataOffsets;
  mNextReadSize = mHeader.mFirstFrameSize;
  mReadSizeWrapped = mHeader.mFirstFrameSize;
  mReadOffWrapped = mHeader.mMovieDataOffsets;
  mTotalSeconds = mHeader.mNumFrames / mHeader.mFrameRate;
  if (mPreLoadSeconds < 0.f) {
    mPreLoadSeconds = mTotalSeconds;
    mPreLoadFrames = mHeader.mNumFrames;
  } else if (mPreLoadSeconds > 0.f) {
    mPreLoadFrames =
        rstl::min_val(static_cast< uint >(mHeader.mNumFrames),
                      static_cast< uint >(mPreLoadSeconds * mHeader.mFrameRate));
    mPreLoadSeconds = rstl::min_val(mPreLoadSeconds, mTotalSeconds);
  }
  if (mPreLoadFrames > 0) {
    mRequestQueue.reserve(mPreLoadFrames);
  }
  PostDVDReadRequestIfNeeded();
  return false;
}

CMoviePlayer::~CMoviePlayer() {
  CancelReadRequests();
  --sNumReferences;
  VerifyCallbackStatus();
  if (sNumReferences == 0 && ShouldEnableLockedCache()) {
    LCDisable();
  }
  if (sAudioPlayer == this) {
    sAudioPlayer = nullptr;
  }
}

void CMoviePlayer::InitializeTextures() {
  const uint ySize = OSRoundUp32B(mVideoInfo.mXSize * mVideoInfo.mYSize);
  const uint uvSize = OSRoundUp32B(mVideoInfo.mXSize * mVideoInfo.mYSize / 4);
  const uint audioSize = mHeader.mAudioMaxSamples * 4;
  for (int i = 0; i < mTextures.capacity(); ++i) {
    void* y = CMemory::Alloc(ySize, IAllocator::kHI_RoundUpLen);
    void* u = CMemory::Alloc(uvSize, IAllocator::kHI_RoundUpLen);
    void* v = CMemory::Alloc(uvSize, IAllocator::kHI_RoundUpLen);
    void* audio = CMemory::Alloc(audioSize, IAllocator::kHI_RoundUpLen);
    DCFlushRangeNoSync(y, ySize);
    DCFlushRangeNoSync(u, uvSize);
    DCFlushRangeNoSync(v, uvSize);
    DCFlushRangeNoSync(audio, audioSize);
    // Upstream pushes in place, which inlines the count bump; retail calls the out-of-line
    // push helper, so the temporary is named and fn_803193A0 is called.
    const CTHPTextureSet texture(y, u, v, audio);
    fn_803193A0(&mTextures, &texture);
  }

  PPCSync();
  mDecodedTexSlot = 0;
  mDrawTexSlot = -1;
  mAudioSlot = -1;
}

void CMoviePlayer::PostDVDReadRequestIfNeeded() {
  CInterruptGuard interrupts;
  if (mCurLoadFrame < mHeader.mNumFrames) {
    bool usedPrefetch = false;
    if (!mPrefetchRequest.null()) {
      if (mPrefetchOff == mNextReadOff && mNextReadSize == mPrefetchSize &&
          mPrefetchFrame == mCurLoadFrame) {
        mRequestBuffer = rstl::auto_ptr< uchar >(mPrefetchBuffer.release());
        mRequest = mPrefetchRequest;
        if (mRequest->IsComplete()) {
          PrefetchNextFrame();
        }
        usedPrefetch = true;
      } else {
        mp_assign(mPrefetchRequest, static_cast< CRealDvdRequest* >(nullptr));
        mp_assign(mPrefetchBuffer, static_cast< uchar* >(nullptr));
        mPrefetchFrame = mHeader.mNumFrames;
      }
    }
    if (!usedPrefetch) {
      mRequestBuffer = rstl::auto_ptr< uchar >(
          static_cast< uchar* >(CMemory::Alloc(mNextReadSize, IAllocator::kHI_RoundUpLen)));
      rstl::single_ptr< CRealDvdRequest > request(rs_new CRealDvdRequest);
      DVDOpen(const_cast< char* >(mDvdFile.GetFilename().data()), &request->FileInfo());
      request->FileInfo().cb.userData = this;
      DVDReadAsyncPrio(&request->FileInfo(), mRequestBuffer.get(), mNextReadSize,
                       mNextReadOff, DVDCallback, 2);
      mRequest = request;
    }
  }
}

void CMoviePlayer::DVDCallback(s32 result, DVDFileInfo* info) {
  if (result == 10) {
    return;
  }
  DCInvalidateRange(info->cb.addr, info->cb.length);
  lbl_80419B9D = true;
  static_cast< CMoviePlayer* >(info->cb.userData)->HandleDVDInterrupt(info);
}

void CMoviePlayer::HandleDVDInterrupt(DVDFileInfo* info) {
  if (!mPrefetchRequest.null() || mRequest.null() || &mRequest->FileInfo() != info) {
    return;
  }
  PrefetchNextFrame();
}

void CMoviePlayer::PrefetchNextFrame() {
  mPrefetchFrame = mCurLoadFrame + 1;
  if (mPrefetchFrame != mHeader.mNumFrames) {
    mPrefetchSize = *reinterpret_cast< const uint* >(mRequestBuffer.get());
    mPrefetchOff = mNextReadOff + mNextReadSize;
    mp_assign(mPrefetchBuffer,
              static_cast< uchar* >(CMemory::Alloc(mPrefetchSize, IAllocator::kHI_RoundUpLen)));
    rstl::single_ptr< CRealDvdRequest > request(rs_new CRealDvdRequest);
    DVDOpen(const_cast< char* >(mDvdFile.GetFilename().data()), &request->FileInfo());
    request->FileInfo().cb.userData = this;
    DVDReadAsyncPrio(&request->FileInfo(), mPrefetchBuffer.get(), mPrefetchSize,
                     mPrefetchOff, DVDCallback, 2);
    mPrefetchRequest = request;
  }
}

void CMoviePlayer::ReadCompleted() {
  CInterruptGuard interrupts;
  mp_assign(mRequest, static_cast< CRealDvdRequest* >(nullptr));
  if (mCurLoadFrame == mRequestQueue.size() && mPreLoadFrames > mCurLoadFrame) {
    mRequestQueue.push_back_unsafe(mRequestBuffer);
    mCachedBytes += mNextReadSize;
  }
  mNextReadOff += mNextReadSize;
  mNextReadSize = CBasics::SwapBytes(*reinterpret_cast< const uint* >(mRequestBuffer.get()));
  ++mCurLoadFrame;
  if (mCurLoadFrame == mPreLoadFrames) {
    if (mCurLoadFrame == mHeader.mNumFrames) {
      mReadSizeWrapped = mHeader.mFirstFrameSize;
      mReadOffWrapped = mHeader.mMovieDataOffsets;
    } else {
      mReadSizeWrapped = mNextReadSize;
      mReadOffWrapped = mNextReadOff;
    }
  }
  if (mCurLoadFrame >= mHeader.mNumFrames && mLoop) {
    mNextReadOff = mReadOffWrapped;
    mNextReadSize = mReadSizeWrapped;
    mCurLoadFrame = mPreLoadFrames;
  }
}

void CMoviePlayer::DecodeFromRead(const void* ptr) {
  uchar work[4096 + 32];
  void* alignedWork = reinterpret_cast< void* >((reinterpret_cast< uintptr_t >(work) + 31) & ~31);
  if (mTextures.empty()) {
    InitializeTextures();
  }
  CTHPTextureSet& texture = mTextures[mDecodedTexSlot];
  const uint* sizes = static_cast< const uint* >(ptr) + 2;
  const uchar* dataStart =
      static_cast< const uchar* >(ptr) + 8 + mThpComponents.mNumComponents * 4;
  uint offset = 0;
  const uchar* data = dataStart;
  texture.SetAudioSamplesConsumed(0);
  texture.SetAudioSamples(0);
  for (uint i = 0; i < mThpComponents.mNumComponents; ++i) {
    if (mThpComponents.mFrameComp[i] == 0) {
      THPVideoDecode(const_cast< uchar* >(data), texture.Y(), texture.U(), texture.V(),
                     alignedWork);
    } else if (mThpComponents.mFrameComp[i] == 1) {
      const uint samples = THPAudioDecode(static_cast< short* >(texture.Audio()),
                                          const_cast< uchar* >(data), 0);
      const BOOL interrupts = OSDisableInterrupts();
      texture.SetAudioSamples(samples);
      texture.SetAudioSamplesConsumed(0);
      OSRestoreInterrupts(interrupts);
    }
    // Re-forming `data` from the running offset (rather than advancing it directly) is what leaves
    // the loop with the two induction variables retail has: the byte cursor and `offset`.
    offset += CBasics::SwapBytes(*sizes++);
    data = dataStart + offset;
  }
  if (++mDecodedTexSlot == mTextures.size()) {
    mDecodedTexSlot = 0;
  }
}

void CMoviePlayer::Update(float dt) {
  if (mCurLoadFrame < mPreLoadFrames) {
    if (!mRequest.null() && mRequest->IsComplete()) {
      ReadCompleted();
      if (mCurLoadFrame >= mRequestQueue.size() && mCurLoadFrame < mPreLoadFrames &&
          mRequestQueue.size() < mHeader.mNumFrames) {
        PostDVDReadRequestIfNeeded();
      }
    }
  } else if (!mRequest.null()) {
    const bool canDecode = mRequestFrameWrapped >= mRequestQueue.size() &&
                           mCurLoadFrame >= mRequestQueue.size();
    if (mDecodedTexCount < 2 && canDecode && mRequest->IsComplete()) {
      ReadCompleted();
      rstl::auto_ptr< uchar > buffer;
      if (mCurLoadFrame < mHeader.mNumFrames) {
        buffer = mRequestBuffer;
      } else {
        buffer = rstl::auto_ptr< uchar >(mRequestBuffer.get());
        buffer.release();
      }
      PostDVDReadRequestIfNeeded();
      DecodeFromRead(buffer.get());
      ++mDecodedTexCount;
      ++mRequestFrameWrapped;
      if (mRequestFrameWrapped >= mHeader.mNumFrames && mLoop) {
        mRequestFrameWrapped = 0;
      }
    }
  }
  if (mRequest.null() && mPlayMode == kPM_Playing &&
      mRequestQueue.size() < mHeader.mNumFrames) {
    PostDVDReadRequestIfNeeded();
  }
  if (mDecodedTexCount < 2 && mPlayMode == kPM_Playing &&
      mRequestFrameWrapped < mPreLoadFrames) {
    const int frame = rstl::min_val(mRequestFrameWrapped, mRequestQueue.size() - 1);
    if (frame == -1) {
      return;
    }
    DecodeFromRead(mRequestQueue[frame].get());
    ++mDecodedTexCount;
    ++mRequestFrameWrapped;
    if (mRequestFrameWrapped >= mHeader.mNumFrames && mLoop) {
      mRequestFrameWrapped = 0;
    }
  }
  if (mDecodedTexCount > 0 && mPlayMode == kPM_Playing) {
    mCurSeconds += dt;
    if (mLoop) {
      mCurSeconds = CMath::ModF(mCurSeconds, mTotalSeconds);
    } else {
      mCurSeconds = rstl::min_val(mCurSeconds, mTotalSeconds);
    }
    float remainder = mFrameRem - dt;
    const float frameDt = 1.f / mHeader.mFrameRate;
    if (remainder <= 0.f) {
      if (!mFieldFlip) {
        if (++mDrawTexSlot >= mTextures.size()) {
          mDrawTexSlot = 0;
        }
        const BOOL interrupts = OSDisableInterrupts();
        if (mAudioSlot == -1) {
          mAudioSlot = 0;
        }
        OSRestoreInterrupts(interrupts);
        --mDecodedTexCount;
        ++mCurFrame;
        if (mCurFrame == mHeader.mNumFrames && mLoop) {
          mCurFrame = 0;
        }
        remainder += frameDt;
        mFieldIndex = 0;
      } else {
        remainder += dt;
        mFieldFlip = false;
      }
    }
    mFrameRem = remainder;
  }
}

void CMoviePlayer::DrawFrame(const CVector3f& v1, const CVector3f& v2, const CVector3f& v3,
                             const CVector3f& v4) {
  if (mDrawTexSlot == -1) {
    return;
  }
  CGraphics::SetUseVideoFilter(mDeinterlace);
  const BOOL interrupts = OSDisableInterrupts();
  sAudioPlayer = this;
  OSRestoreInterrupts(interrupts);
  CTHPTextureSet& texture = mTextures[mDrawTexSlot];
  const bool field = CGraphics::GetDolphinLastFrameAbove();
  MyTHPGXYuv2RgbSetup(field, mDeinterlace || mIs60Hz);
  MyTHPYuv2RgbTextureSetup(texture.Y(), texture.U(), texture.V(), mVideoInfo.mXSize,
                           mVideoInfo.mYSize);
  CGX::Begin(GX_TRIANGLEFAN, GX_VTXFMT7, 4);
  GXPosition3f32(v1.GetX(), v1.GetY(), v1.GetZ());
  GXTexCoord2u16(0, 0);
  GXPosition3f32(v3.GetX(), v3.GetY(), v3.GetZ());
  GXTexCoord2u16(0, 1);
  GXPosition3f32(v4.GetX(), v4.GetY(), v4.GetZ());
  GXTexCoord2u16(1, 1);
  GXPosition3f32(v2.GetX(), v2.GetY(), v2.GetZ());
  GXTexCoord2u16(1, 0);
  CGX::End();
  MyTHPGXRestore();
  if (mFieldIndex == 0 && !field && !mIs60Hz) {
    mFieldFlip = true;
  }
  ++mFieldIndex;
}

void CMoviePlayer::SetPlayMode(const EPlayMode mode) { mPlayMode = mode; }

float CMoviePlayer::GetTotalSeconds() const { return mTotalSeconds; }

float CMoviePlayer::GetPlayedSeconds() const { return mCurSeconds + mFrameRem; }

bool CMoviePlayer::GetIsFullyCached() const {
  return mRequestQueue.size() >= mPreLoadFrames;
}

bool CMoviePlayer::GetIsMovieFinishedPlaying() const {
  return !mLoop && mCurFrame == mHeader.mNumFrames;
}

void CMoviePlayer::Rewind() {
  CancelReadRequests();
  mRequestBuffer = rstl::auto_ptr< uchar >(nullptr);
  mp_assign(mPrefetchBuffer, static_cast< uchar* >(nullptr));
  mNextReadSize = mHeader.mFirstFrameSize;
  mNextReadOff = mHeader.mMovieDataOffsets;
  mReadSizeWrapped = mHeader.mFirstFrameSize;
  mReadOffWrapped = mHeader.mMovieDataOffsets;
  mCurLoadFrame = 0;
  mRequestFrameWrapped = 0;
  mCurFrame = 0;
  mDecodedTexSlot = 0;
  mDrawTexSlot = -1;
  mAudioSlot = -1;
  mDecodedTexCount = 0;
  mFrameRem = 0.f;
  mCurSeconds = 0.f;
  mTextures.clear();
}

void CMoviePlayer::StaticMyAudioCallback() {
  if (sAudioPlayer != nullptr && sAudioPlayer->mHasAudio) {
    curAudioBuffer = static_cast< const short* >(OSPhysicalToCached(AIGetDMAStartAddr()));
    soundBufferIndex ^= 1;
    short* buffer = soundBuffer[soundBufferIndex];
    AIInitDMA(reinterpret_cast< uintptr_t >(buffer), sizeof(soundBuffer[0]));
    const BOOL interrupts = OSEnableInterrupts();
    if (curAudioBuffer != nullptr) {
      DCInvalidateRange(const_cast< short* >(curAudioBuffer), sizeof(soundBuffer[0]));
    }
    sAudioPlayer->MixAudio(buffer, curAudioBuffer, 160);
    DCFlushRange(buffer, sizeof(soundBuffer[0]));
    OSRestoreInterrupts(interrupts);
  }
}

void CMoviePlayer::MixAudio(short* out, const short* in, unsigned long samples) {
  const short* input = in;
  if (mAudioSlot == -1) {
    if (in != nullptr) {
      memcpy(out, in, samples * 4);
    } else {
      memset(out, 0, samples * 4);
    }
    return;
  }
  const uchar volume = rstl::min_val(127, mVolume * sSfxVolume * 100 >> 14);
  const ushort attenuation =
      sAudioEnabled ? static_cast< ushort >(CAudioSys::GetScaledVolume(volume)) : 0;
  for (int frame = 0; samples != 0 && frame < 3; ++frame) {
    CTHPTextureSet& texture = mTextures[mAudioSlot];
    uint count = texture.GetAudioSamples() - texture.GetAudioSamplesConsumed();
    if (count > samples) {
      count = samples;
    } else {
      if (++mAudioSlot == mTextures.size()) {
        mAudioSlot = 0;
      }
    }
    const uint consumed = texture.GetAudioSamplesConsumed();
    const short* audio = static_cast< short* >(texture.Audio()) + consumed * 2;
    texture.SetAudioSamplesConsumed(count + consumed);
    if (in != nullptr) {
      for (uint i = 0; i < count * 2; ++i) {
        int sample = *input + ((attenuation * *audio) >> 15);
        if (sample < -32768) {
          sample = -32768;
        } else if (sample > 32767) {
          sample = 32767;
        }
        *out = sample;
        ++out;
        ++input;
        ++audio;
      }
    } else {
      for (uint i = 0; i < count * 2; ++i) {
        int sample = (attenuation * *audio) >> 15;
        if (sample < -32768) {
          sample = -32768;
        } else if (sample > 32767) {
          sample = 32767;
        }
        *out = sample;
        ++out;
        ++audio;
      }
    }
    samples -= count;
  }
  if (samples != 0) {
    if (in != nullptr) {
      memcpy(out, input, samples * 4);
    } else {
      memset(out, 0, samples * 4);
    }
  }
}

void CMoviePlayer::VerifyCallbackStatus() {
  if (sNumReferences > 0) {
    CStaticAudioPlayer::RunDMACallback(StaticMyAudioCallback);
  } else {
    CStaticAudioPlayer::CancelDMACallback(StaticMyAudioCallback);
  }
}

uint CMoviePlayer::GetWidth() const { return mVideoInfo.mXSize; }

uint CMoviePlayer::GetHeight() const { return mVideoInfo.mYSize; }

void CMoviePlayer::SetAudioEnabled(bool enabled) { sAudioEnabled = enabled; }

bool CMoviePlayer::GetAudioEnabled() { return sAudioEnabled; }

void CMoviePlayer::SetSfxVolume(uchar volume) { sSfxVolume = rstl::min_val(uchar(127), volume); }

CMoviePlayer::EPlayMode CMoviePlayer::GetPlayMode() const { return mPlayMode; }

void CMoviePlayer::DrawFrame(int left, int right, int bottom, int top) {
  const float l = left;
  const float r = right;
  const float b = bottom;
  const float t = top;
  CVector3f v1(l, 0.f, t);
  CVector3f v2(r, 0.f, t);
  CVector3f v3(l, 0.f, b);
  CVector3f v4(r, 0.f, b);
  DrawFrame(v1, v2, v3, v4);
}

void CMoviePlayer::CancelReadRequests() {
  rstl::single_ptr< CRealDvdRequest > prefetch;
  rstl::single_ptr< CRealDvdRequest > request;
  {
    CInterruptGuard interrupts;
    prefetch = mPrefetchRequest;
    request = mRequest;
  }
  if (!prefetch.null()) {
    prefetch->PostCancelRequest();
    mp_assign(prefetch, static_cast< CRealDvdRequest* >(nullptr));
  }
  if (!request.null()) {
    request->PostCancelRequest();
    mp_assign(request, static_cast< CRealDvdRequest* >(nullptr));
  }
}
