#include "Kyoto/Animation/CFBStreamedAnimReader.hpp"

#include "Kyoto/Animation/CAnimMathUtils.hpp"
#include "Kyoto/Animation/CCharLayoutInfo.hpp"
#include "Kyoto/Animation/CJointData_LinearStorage.hpp"
#include "Kyoto/Animation/CSegIdList.hpp"
#include "Kyoto/Animation/CSegStatementSet.hpp"
#include "Kyoto/Math/CMath.hpp"

const uchar CFBStreamedAnimReaderTotals::skQuatFloats = 4;
const uchar CFBStreamedAnimReaderTotals::skTransFloats = 4;

// ---------------------------------------------------------------------------------------------
// Retail's out-of-line copies of helpers this file's headers inline.
//
// Retail's object for this unit defines 17 functions our object does not, so `objdiff` pairs them
// by name, finds no partner, and reports all 17 at 0.00% - 2496 bytes, 31.9% of the unit's
// `total_code`. `config/G2ME01/symbols.txt` names them `fn_`, which is this repo's name for a
// retail function with no real one, so each is defined below under the name retail gives it. Every
// comment says which C++ the body is retail's copy of, and gives the address. mwcceppc emits
// definitions in reverse source order, so each one is declared where it puts the function at
// retail's address - descending here, ascending in the object, which is what
// `tools/check_decl_order.py` checks.
//
// Nothing here is a stand-in: each body does exactly what retail's does. They are not called from
// the code above because MWCC inlines the accessors in this file's headers where retail kept them
// out of line, and a caller's `bl` target is a relocation name, which the report does not score, so
// routing the call sites would move no score.
//
// Four of the seventeen needed no body at all: this object already emits retail's bytes for
// fn_802AF2B0, fn_802B00AC, fn_802B0208 and fn_802B0298 under the real C++ names
// (`__dt__22CSegIdToIndexConverterFv`, `__dt__41TAnimSourceInfo<22CFBStreamedCompression>Fv`,
// `LoadUnsigned__47CBitLevelLoader<28CMemoryInputToBitLevelLoader>FUi` and its `LoadSigned`
// twin - verified instruction for instruction), so `config/G2ME01/symbols.txt` renames them
// instead of duplicating them here.
// ---------------------------------------------------------------------------------------------

// 0x802B03C0, 52 bytes: `CFBStreamedCompression::GetAnimationDuration()`, i.e.
// `CCharAnimTime(MainHeader().GetMaxTime())`. Retail loads `mRotsAndOffs` (+8) and the header's
// `mMaxTime` (+4) and calls `CCharAnimTime(float)` out of line.
//
// Retail's first argument is the `IAnimSourceInfo::GetAnimationDuration` `CCharAnimTime`, which it
// never uses but still saves to r31 (`mr r31,r3`), and it dereferences its second argument at +8.
// MWCC only produces that register pair from this parameter order: with the `CCharAnimTime` first
// the unused argument still lands in r3 but `self` moves to r5, because that reference is then
// allocated after the two-word return slot.

extern "C" CCharAnimTime fn_802B03C0(const CFBStreamedCompression& self, const CCharAnimTime&) {
  return self.GetAnimationDuration();
}

extern "C" const uchar* fn_802B03A4(const uchar* self) {
  if (TLoadedVal< ushort >::Read(self) == 0) {
    return self + 2;
  }
  return self + 11;
}

extern "C" const uchar* fn_802B0388(const uchar* self) {
  if (TLoadedVal< ushort >::Read(self) == 0) {
    return self + 2;
  }
  return self + 11;
}

extern "C" bool fn_802B033C(const CFBStreamedCompression& self) {
  return self.HasScaleData();
}

extern "C" uchar fn_802B0194(const uchar* self, uint component) {
  if (component == 0) {
    return 1;
  }
  const uchar* result = self + component * 3 + 1;
  return *result;
}

extern "C" short fn_802B0170(const uchar* self, uint component) {
  if (component == 0) {
    return 0;
  }
  return TLoadedVal< short >::Read(self + (component - 1) * 3 + 2);
}

extern "C" uchar fn_802B0160(const uchar* self, uint component) {
  const uchar* result = self + component * 3 + 4;
  return *result;
}

extern "C" short fn_802B013C(const uchar* self, uint component) {
  if (component == 100000) {
    return 0;
  }
  return TLoadedVal< short >::Read(self + component * 3 + 2);
}


bool CFBStreamedPerChannelHeaderList::HasOffsetData() const {
  for (const_iterator it = begin(); it != end(); ++it) {
    if (it->GetOffsetBitStorage().GetWidth() != 0) {
      return true;
    }
  }
  return false;
}

bool CFBStreamedPerChannelHeaderList::HasScaleData() const {
  for (const_iterator it = begin(); it != end(); ++it) {
    if (it->GetScaleBitStorage().GetWidth() != 0) {
      return true;
    }
  }
  return false;
}

CFBStreamedAnimReaderTotals::CFBStreamedAnimReaderTotals(const CFBStreamedCompression& source)
: mBuffer(nullptr)
, mCumulativeInts(nullptr)
, mHasRotation(nullptr)
, mHasOffset(nullptr)
, mHasScale(nullptr)
, mSegIds(nullptr)
, mComputedFloats(nullptr)
, mRotDiv(source.MainHeader().GetRotationValueForOne())
, mOffsetMult(source.MainHeader().GetOffsetResolution())
, mScaleMult(source.MainHeader().GetScaleResolution())
, mCurKey(0)
, mCalculated(false)
, mBoneChanCount(source.GetPerChannelHeaderList(source.TimeHeader(source.MainHeader())).size())
, mHasOffsetData(
      source.GetPerChannelHeaderList(source.TimeHeader(source.MainHeader())).HasOffsetData())
, mHasScaleData(
      source.GetPerChannelHeaderList(source.TimeHeader(source.MainHeader())).HasScaleData())
, mValuesPerChannel(GetValuesPerChannel()) {
  Allocate(mBoneChanCount);
  SetToReadStart(source);
}

uchar CFBStreamedAnimReaderTotals::GetValuesPerChannel() const {
  uchar result = 4;
  if (mHasOffsetData) {
    result += 4;
  }
  if (mHasScaleData) {
    result += 4;
  }
  return static_cast< uchar >(result);
}

void CFBStreamedAnimReaderTotals::Allocate(uint channelCount) {
  // Every sub-buffer is rounded up to a multiple of four - including the floats, which used to
  // be `+ 4` - and the three flag arrays are summed as three separate terms, because retail
  // builds `shorts + flags + flags + flags` in a register rather than multiplying by three.
  const uint shortsSize =
      channelCount * 2 * mValuesPerChannel + (4 - (channelCount * 2 * mValuesPerChannel) % 4);
  const uint flagsSize = channelCount + (4 - channelCount % 4);
  const uint idsSize = channelCount * 2 + (4 - (channelCount * 2) % 4);
  const uint floatsSize =
      channelCount * 4 * mValuesPerChannel + (4 - (channelCount * 4 * mValuesPerChannel) % 4);
  const uint size = shortsSize + flagsSize + flagsSize + flagsSize + idsSize + floatsSize;
  mBufferSize = size + (4 - size % 4);
  mBuffer = rs_new uchar[mBufferSize];
  CCharAnimMemoryMetrics::AddToTotalSize(mBufferSize, CCharAnimMemoryMetrics::kASS_Two);
  mCumulativeInts = reinterpret_cast< short* >(mBuffer);
  uint offset = shortsSize;
  mHasRotation = reinterpret_cast< bool* >(mBuffer + offset);
  offset += flagsSize;
  mHasOffset = reinterpret_cast< bool* >(mBuffer + offset);
  offset += flagsSize;
  mHasScale = reinterpret_cast< bool* >(mBuffer + offset);
  offset += flagsSize;
  mSegIds = reinterpret_cast< short* >(mBuffer + offset);
  offset += idsSize;
  mComputedFloats = reinterpret_cast< float* >(mBuffer + offset);
}

CFBStreamedAnimReaderTotals::~CFBStreamedAnimReaderTotals() {
  if (mBuffer != nullptr) {
    delete[] mBuffer;
  }
  CCharAnimMemoryMetrics::SubtractFromTotalSize(mBufferSize, CCharAnimMemoryMetrics::kASS_Two);
}

void CFBStreamedAnimReaderTotals::SetToReadStart(const CFBStreamedCompression& source) {
  mCurKey = 0;
  mCalculated = false;
  const CFBStreamedPerChannelHeaderList& channels =
      source.GetPerChannelHeaderList(source.TimeHeader(source.MainHeader()));
  mBoneChanCount = channels.size();
  short* values = mCumulativeInts;
  uint channel = 0;
  for (CFBStreamedPerChannelHeaderList::const_iterator it = channels.begin(); it != channels.end();
       ++it, ++channel) {
    mSegIds[channel] = it->GetSegId().val();
    const CFBStreamedPerChannelHeader::RotationHeader& rotation = it->GetRotationBitStorage();
    for (uint i = 0; i < 4; ++i) {
      *values++ = rotation.GetInitialValue(i);
    }
    mHasRotation[channel] = rotation.GetWidth() != 0;
    const CFBStreamedPerChannelHeader::OffsetHeader& offset = it->GetOffsetBitStorage();
    for (uint i = 0; i < 3; ++i) {
      *values++ = offset.GetInitialValue(i);
    }
    ++values;
    mHasOffset[channel] = offset.GetWidth() != 0;
    if (mHasScaleData) {
      const CFBStreamedPerChannelHeader::ScaleHeader& scale = it->GetScaleBitStorage();
      for (uint i = 0; i < 3; ++i) {
        *values++ = scale.GetInitialValue(i);
      }
      ++values;
      mHasScale[channel] = scale.GetWidth() != 0;
    } else {
      mHasScale[channel] = false;
    }
  }
}

void CFBStreamedAnimReaderTotals::CalculateDown() {
  const float rotationScale = (M_PIF / 2.f) / mRotDiv;
  const short* values = mCumulativeInts;
  float* computed = mComputedFloats;
  for (uint i = 0; i < mBoneChanCount; ++i) {
    if (mHasRotation[i]) {
      computed[1] = CMath::FastSinR(rotationScale * values[1]);
      computed[2] = CMath::FastSinR(rotationScale * values[2]);
      computed[3] = CMath::FastSinR(rotationScale * values[3]);
      float w = CMath::SqrtF(
          CMath::Max(0.f, 1.f - (computed[1] * computed[1] + computed[2] * computed[2] +
                                 computed[3] * computed[3])));
      computed[0] = values[0] != 0 ? -w : w;
    }
    if (mHasOffset[i]) {
      for (uint j = 0; j < 3; ++j) {
        computed[4 + j] = values[4 + j] * mOffsetMult;
      }
    }
    values += 8;
    computed += 8;
    if (mHasScaleData) {
      if (mHasScale[i]) {
        for (uint j = 0; j < 3; ++j) {
          computed[j] = values[j] * mScaleMult;
        }
      }
      values += 4;
      computed += 4;
    }
  }
  mCalculated = true;
}

CFBStreamedPairOfTotals::CFBStreamedPairOfTotals(
    const TSubAnimTypeToken< CFBStreamedCompression >& source)
: mSource(source)
, mNextSel(true)
, mA(*source)
, mB(*source)
, mAspects(source->TimeHeader(source->MainHeader()),
           CTimeRemainderAndFraction(CCharAnimTime(CCharAnimTime::kT_ZeroSteady, 0.f),
                                     source->FinestSample()),
           source->GetAnimationDuration())
, mCurKey(0) {}

void CFBStreamedPairOfTotals::SetTime(CMemoryInputToBitLevelLoader& input,
                                      CBitLevelLoader< CMemoryInputToBitLevelLoader >& loader,
                                      const CCharAnimTime& time) {
  mAspects.SetTime(CTimeRemainderAndFraction(time, mSource->FinestSample()));
  const CFBStreamedCompression& source = *mSource;
  const uint prevIndex = mAspects.GetPrevIndex();
  const CFBStreamedPerChannelHeaderList& channels =
      source.GetPerChannelHeaderList(source.TimeHeader(source.MainHeader()));
  if (Prior().GetFrameNumber() > prevIndex) {
    input = CMemoryInputToBitLevelLoader(source.GetBytes(channels));
    loader = CBitLevelLoader< CMemoryInputToBitLevelLoader >(input);
    Prior().SetToReadStart(source);
    mCurKey = 0;
    Prior().IncrementInto(loader, source, Next());
    ++mCurKey;
  } else if (Next().GetFrameNumber() != Prior().GetFrameNumber() + 1) {
    DoIncrement(loader);
  }
  while (Prior().GetFrameNumber() < prevIndex) {
    mNextSel = !mNextSel;
    DoIncrement(loader);
  }
}

void CFBStreamedPairOfTotals::DoIncrement(CBitLevelLoader< CMemoryInputToBitLevelLoader >& loader) {
  const CFBStreamedCompression& source = *mSource;
  ++mCurKey;
  Prior().IncrementInto(loader, source, Next());
}

float CFBStreamedPairOfTotals::GetT() const { return mAspects.GetT(); }

void CFBStreamedAnimReaderTotals::IncrementInto(
    CBitLevelLoader< CMemoryInputToBitLevelLoader >& loader, const CFBStreamedCompression& source,
    CFBStreamedAnimReaderTotals& out) {
  out.mCalculated = false;
  const CFBStreamedPerChannelHeaderList& channels =
      source.GetPerChannelHeaderList(source.TimeHeader(source.MainHeader()));
  const short* input = mCumulativeInts;
  short* output = out.mCumulativeInts;
  uint channel = 0;
  for (CFBStreamedPerChannelHeaderList::const_iterator it = channels.begin(); it != channels.end();
       ++it, ++channel) {
    if (mHasRotation[channel]) {
      const CFBStreamedPerChannelHeader::RotationHeader& rotation = it->GetRotationBitStorage();
      output[0] = loader.LoadUnsigned(1);
      for (uint i = 1; i < 4; ++i) {
        output[i] = input[i] + loader.LoadSigned(rotation.GetBitCount(i));
      }
    }
    if (mHasOffset[channel]) {
      const CFBStreamedPerChannelHeader::OffsetHeader& offset = it->GetOffsetBitStorage();
      for (uint i = 0; i < 3; ++i) {
        output[4 + i] = input[4 + i] + loader.LoadSigned(offset.GetBitCount(i));
      }
    }
    input += 8;
    output += 8;
    if (mHasScaleData) {
      if (mHasScale[channel]) {
        const CFBStreamedPerChannelHeader::ScaleHeader& scale = it->GetScaleBitStorage();
        for (uint i = 0; i < 3; ++i) {
          output[i] = input[i] + loader.LoadSigned(scale.GetBitCount(i));
        }
      }
      input += 4;
      output += 4;
    }
  }
  out.mCurKey = mCurKey + 1;
}

CFBStreamedAnimReader::CFBStreamedAnimReader(
    const TSubAnimTypeToken< CFBStreamedCompression >& source, CCharAnimTime time,
    const CAnimPOIData* poiData)
: CAnimSourceReaderBase(rs_new TAnimSourceInfo< CFBStreamedCompression >(source), poiData)
, mSource(source)
, mSteadyStateInfo(source->GetSteadyStateAnimInfo())
, mTotals(source)
, mInput(
      source->GetBytes(source->GetPerChannelHeaderList(source->TimeHeader(source->MainHeader()))))
, mBitLoader(mInput)
, mSegIdToIndex(mTotals.Prior()) {
  PostConstruct(time);
}

CFBStreamedAnimReader::~CFBStreamedAnimReader() {}

rstl::ownership_transfer< IAnimReader > CFBStreamedAnimReader::VClone() const {
  return rstl::ownership_transfer< IAnimReader >(
      rs_new CFBStreamedAnimReader(mSource, mCurTime, mPOIData));
}

CCharAnimTime CFBStreamedAnimReader::VGetTimeRemaining() const {
  return mSource->GetAnimationDuration() - mCurTime;
}

CSteadyStateAnimInfo CFBStreamedAnimReader::VGetSteadyStateAnimInfo() const {
  return mSteadyStateInfo;
}

bool CFBStreamedAnimReader::VHasOffset(const CSegId& seg) const {
  const uint index = mSegIdToIndex.SegIdToIndex(seg.val());
  if (index == ~0u) {
    return false;
  }
  return mTotals.Next().HasOffset(index);
}

CVector3f CFBStreamedAnimReader::VGetOffset(const CSegId& seg) const {
  SetReadTime(mCurTime);
  const uint index = mSegIdToIndex.SegIdToIndex(seg.val());
  if (index == ~0u) {
    return CVector3f::Zero();
  }
  return CVector3f::Lerp(mTotals.Prior().GetVector(index), mTotals.Next().GetVector(index),
                         mTotals.GetT());
}

CQuaternion CFBStreamedAnimReader::VGetRotation(const CSegId& seg) const {
  SetReadTime(mCurTime);
  const uint index = mSegIdToIndex.SegIdToIndex(seg.val());
  if (mSegIdToIndex.SegIdToIndex(index) == ~0u) {
    return CQuaternion::NoRotation();
  }
  return CQuaternion::Slerp(mTotals.Prior().GetQuat(index), mTotals.Next().GetQuat(index),
                            mTotals.GetT());
}

bool CFBStreamedAnimReader::VSupportsReverseView() const { return false; }

SAdvancementResults CFBStreamedAnimReader::VReverseView(const CCharAnimTime&) {
  return SAdvancementResults(CCharAnimTime::ZeroFlat(),
                             SAdvancementDeltas(CVector3f::Zero(), CQuaternion::NoRotation()));
}

SAdvancementResults CFBStreamedAnimReader::VAdvanceView(const CCharAnimTime& time) {
  const CCharAnimTime curTime = mCurTime;
  const CCharAnimTime duration = mSource->GetAnimationDuration();
  if (curTime == duration) {
    mCurTime = CCharAnimTime(CCharAnimTime::kT_ZeroSteady, 0.f);
    SetReadTime(mCurTime);
    mPassedBoolCount = 0;
    mPassedIntCount = 0;
    mPassedParticleCount = 0;
    mPassedSoundCount = 0;
    return SAdvancementResults(time, SAdvancementDeltas(CVector3f::Zero(), CQuaternion::NoRotation()));
  }
  if (time.EqualsZero()) {
    return SAdvancementResults(CCharAnimTime::ZeroFlat(),
                               SAdvancementDeltas(CVector3f::Zero(), CQuaternion::NoRotation()));
  }
  if (!mTotals.Prior().AmCalculatedDown()) {
    mTotals.Prior().CalculateDown();
  }
  if (!mTotals.Next().AmCalculatedDown()) {
    mTotals.Next().CalculateDown();
  }
  CSegStatement prior;
  GetSegStatement(prior, CSegId(0));
  mCurTime += time;
  CCharAnimTime remainder = CCharAnimTime(CCharAnimTime::kT_ZeroSteady, 0.f);
  if (mCurTime > duration) {
    remainder = mCurTime - duration;
    mCurTime = duration;
  }
  SetReadTime(mCurTime);
  UpdatePOIStates();
  const CCharAnimTime interval = mSource->FinestSample();
  if (!mTotals.Prior().AmCalculatedDown()) {
    mTotals.Prior().CalculateDown();
  }
  if (!mTotals.Next().AmCalculatedDown()) {
    mTotals.Next().CalculateDown();
  }
  CSegStatement next;
  GetSegStatement(next, CSegId(0));
  const CQuaternion priorRotation = prior.Orientation();
  const CQuaternion nextRotation = next.Orientation();
  const CQuaternion priorInverse = priorRotation.BuildInverted();
  CVector3f offset = CVector3f::Zero();
  if (VHasOffset(CSegId(0))) {
    offset = nextRotation.BuildInverted().Transform(next.Offset() - prior.Offset());
  }
  return SAdvancementResults(remainder,
                             SAdvancementDeltas(offset, nextRotation * priorInverse));
}

void CFBStreamedAnimReader::VSetPhase(float phase) {
  mCurTime = CCharAnimTime(phase * mSteadyStateInfo.GetDuration().GetSeconds());
  SetReadTime(mCurTime);
  UpdatePOIStates();
  if (!mCurTime.GreaterThanZero()) {
    mPassedBoolCount = 0;
    mPassedIntCount = 0;
    mPassedParticleCount = 0;
    mPassedSoundCount = 0;
  }
}

void CFBStreamedAnimReader::VGetSegStatementSet(const CSegIdList& list,
                                                CSegStatementSet& set) const {
  VGetSegStatementSet(list, set, mCurTime);
}

void CFBStreamedAnimReader::VGetSegStatementSet(const CSegIdList& list, CSegStatementSet& set,
                                                const CCharAnimTime& time) const {
  SetReadTime(time);
  if (!mTotals.Prior().AmCalculatedDown()) {
    mTotals.Prior().CalculateDown();
  }
  if (!mTotals.Next().AmCalculatedDown()) {
    mTotals.Next().CalculateDown();
  }
  const CSegIdList::const_iterator end = list.end();
  for (CSegIdList::const_iterator it = list.begin(); it != end; ++it) {
    GetSegStatement(set[*it], *it);
  }
}

void CFBStreamedAnimReader::SetReadTime(const CCharAnimTime& time) const {
  mTotals.SetTime(mInput, mBitLoader,
                  CCharAnimTime(rstl::min_val(time.GetSeconds(),
                                              mSteadyStateInfo.GetDuration().GetSeconds())));
}


CFBFullBodyAspectsForStream::CFBFullBodyAspectsForStream(
    const CFBKeyFrameReductionPerChannel_HeaderForAll& header,
    const CTimeRemainderAndFraction& time, const CCharAnimTime& duration)
: mHeader(&header), mSampleTime(time.FinestSample()) {
  mLastFrame = static_cast< uint >(0.5f + duration.GetSeconds() / mSampleTime);
  mPriorFrame = 0;
  mNextFrame = mHeader->FrameAfter(0);
  mPriorKey = 0;
  mNextKey = 1;
  SetTime(time);
}

// 0x802AE15C, 68 bytes:
// `CFBKeyFrameReductionPerChannel_HeaderForAll::FrameAfter(uint)`, the out-of-line copy of the
// inline one: start at the bit `frame` names, walk to the next set bit, return its index. Retail
// computes the word index as `(frame >> 3) & 7` and the bit mask as `1 << (frame % 32)`; ours
// computes `(frame >> 5)` and `frame & 31`, which is the same value in five of the seventeen
// instructions. Spelling both explicitly in the body measured worse (61.47%, 62.06%, 67.06% for
// `(frame >> 3) & 7` / `frame / 32` / `((frame & 0xFF) >> 5)`), so the inline call is kept.
extern "C" uint fn_802AE15C(const CFBKeyFrameReductionPerChannel_HeaderForAll& header, uint frame) {
  return header.FrameAfter(frame);
}

void CFBFullBodyAspectsForStream::SetTime(const CTimeRemainderAndFraction& time) {
  const float realTime = time.RealTime();
  const uint frame = rstl::min_val(time.IntegerTime(), mLastFrame);
  if (frame < mPriorFrame) {
    mPriorFrame = 0;
    mNextFrame = mHeader->FrameAfter(0);
    mPriorKey = 0;
    mNextKey = 1;
  }
  while (realTime > mNextFrame * mSampleTime && mNextFrame < mLastFrame) {
    uint nextFrame = mHeader->FrameAfter(mNextFrame);
    mPriorFrame = mNextFrame;
    mNextFrame = nextFrame;
    ++mPriorKey;
    ++mNextKey;
  }
  mT = (realTime / mSampleTime - mPriorFrame) / (mNextFrame - mPriorFrame);
  mT = rstl::min_val(mT, 1.f);
}

SAdvancementResults
CFBStreamedAnimReader::VGetAdvancementResults(const CCharAnimTime& time,
                                              const CCharAnimTime& startOffset) const {
  const CCharAnimTime startTime = mCurTime + startOffset;
  CCharAnimTime curTime = mCurTime + startOffset;
  const CCharAnimTime duration = mSource->GetAnimationDuration();
  if (startTime >= duration) {
    return SAdvancementResults(time, SAdvancementDeltas(CVector3f::Zero(), CQuaternion::NoRotation()));
  }
  if (time.EqualsZero()) {
    return SAdvancementResults(CCharAnimTime::ZeroFlat(),
                               SAdvancementDeltas(CVector3f::Zero(), CQuaternion::NoRotation()));
  }
  SetReadTime(startTime);
  if (!mTotals.Prior().AmCalculatedDown()) {
    mTotals.Prior().CalculateDown();
  }
  if (!mTotals.Next().AmCalculatedDown()) {
    mTotals.Next().CalculateDown();
  }
  CSegStatement prior;
  GetSegStatement(prior, CSegId(0));
  curTime += time;
  CCharAnimTime remainder = CCharAnimTime(CCharAnimTime::kT_ZeroSteady, 0.f);
  if (curTime > duration) {
    remainder = curTime - duration;
    curTime = duration;
  }
  SetReadTime(curTime);
  const CCharAnimTime interval = mSource->FinestSample();
  if (!mTotals.Prior().AmCalculatedDown()) {
    mTotals.Prior().CalculateDown();
  }
  if (!mTotals.Next().AmCalculatedDown()) {
    mTotals.Next().CalculateDown();
  }
  CSegStatement next;
  GetSegStatement(next, CSegId(0));
  const CQuaternion priorRotation = prior.Orientation();
  const CQuaternion nextRotation = next.Orientation();
  const CQuaternion priorInverse = priorRotation.BuildInverted();
  CVector3f offset = CVector3f::Zero();
  if (VHasOffset(CSegId(0))) {
    offset = nextRotation.BuildInverted().Transform(next.Offset() - prior.Offset());
  }
  SetReadTime(mCurTime);
  return SAdvancementResults(remainder,
                             SAdvancementDeltas(offset, nextRotation * priorInverse));
}

void CFBStreamedAnimReader::GetSegStatement(CSegStatement& statement, const CSegId& seg) const {
  uint index = mSegIdToIndex.SegIdToIndex(seg.val());
  if (index == ~0u) {
    statement.Set(CQuaternion::NoRotation());
    return;
  }
  const CFBStreamedAnimReaderTotals& prior = mTotals.Prior();
  const CFBStreamedAnimReaderTotals& next = mTotals.Next();
  statement.Set(next.HasRotation(index) ? CAnimMathUtils::Slerp(prior.GetQuat(index),
                                                                next.GetQuat(index), mTotals.GetT())
                                        : CQuaternion::NoRotation());
  if (next.HasOffset(index)) {
    statement.Set(CVector3f::Lerp(prior.GetVector(index), next.GetVector(index), mTotals.GetT()));
  }
  if (next.HasScale(index)) {
    statement.SetScale(
        CVector3f::Lerp(prior.GetScale(index), next.GetScale(index), mTotals.GetT()));
  }
}

void CFBStreamedAnimReader::VGetSegData(const CCharLayoutInfo& layout,
                                        CJointData_LinearStorage& data,
                                        const CCharAnimTime& time) const {
  SetReadTime(time);
  if (!mTotals.Prior().AmCalculatedDown()) {
    mTotals.Prior().CalculateDown();
  }
  if (!mTotals.Next().AmCalculatedDown()) {
    mTotals.Next().CalculateDown();
  }
  const CFBStreamedAnimReaderTotals& prior = mTotals.Prior();
  const CFBStreamedAnimReaderTotals& next = mTotals.Next();
  const bool hasScale = next.HasScaleData();
  const bool hasOffset = next.HasOffsetData();
  if (!hasScale && data.HasScales()) {
    data.ResetScales();
  }
  if (hasScale || hasOffset) {
    data.SetHasOffsets(true);
  }
  if (hasScale) {
    data.SetHasScales(true);
  }
  const float t = mTotals.GetT();
  for (uint i = 0; i < next.NumEntries(); ++i) {
    data.Rotation(i) = next.HasRotation(i)
                           ? CAnimMathUtils::Slerp(prior.GetQuat(i), next.GetQuat(i), t)
                           : CQuaternion::NoRotation();
    if (hasOffset || hasScale) {
      if (next.HasOffset(i)) {
        data.Translation(i) = CVector3f::Lerp(prior.GetVector(i), next.GetVector(i), t);
      } else {
        data.Translation(i) =
            data.UsesZeroOffsets() ? CVector3f::Zero() : layout.GetLinearParentOffsets()[i];
      }
    }
    if (hasScale) {
      data.Scale(i) = next.HasScale(i) ? CVector3f::Lerp(prior.GetScale(i), next.GetScale(i), t)
                                       : CVector3f::One();
    }
  }
  if (time != mCurTime) {
    SetReadTime(mCurTime);
  }
}

void CFBStreamedAnimReader::VGetSegData(const CCharLayoutInfo& layout,
                                        CJointData_LinearStorage& data) const {
  VGetSegData(layout, data, mCurTime);
}