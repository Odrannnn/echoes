#include "Kyoto/Animation/CFBStreamedCompression.hpp"

#include "Kyoto/Animation/CFBStreamedAnimReader.hpp"
#include "Kyoto/Math/CloseEnough.hpp"

rstl::auto_ptr< uint > CFBStreamedCompression::GetRotationsAndOffsets(uint words,
                                                                      CInputStream& in) {
  rstl::auto_ptr< uint > data(rs_new uint[words]);
  CStandardMultiFormatHeader* header = reinterpret_cast< CStandardMultiFormatHeader* >(data.get());
  new (header) CStandardMultiFormatHeader(in);
  CFBStreamedCompressionTimeHeader* timeHeader =
      static_cast< CFBStreamedCompressionTimeHeader* >(const_cast< void* >(header->AfterEnd()));
  new (timeHeader) CFBStreamedCompressionTimeHeader(in);
  CFBStreamedPerChannelHeaderList* channels =
      static_cast< CFBStreamedPerChannelHeaderList* >(const_cast< void* >(timeHeader->AfterEnd()));
  new (channels) CFBStreamedPerChannelHeaderList(in);
  // Retail evaluates `AfterEnd()` first: it is the statement before the count, and swapping the
  // two costs the one instruction this function is short of retail by.
  uchar* cursor = const_cast< uchar* >(channels->AfterEnd());
  const uint wordCount = static_cast< uint >(
      static_cast< float >(channels->GetSumOfBitCounts() *
                               channels->begin()->GetRotationBitStorage().GetWidth() +
                           31) /
      32.f);
  for (uint i = 0; i < wordCount; ++i) {
    TLoadedVal< uint >::Write(cursor, in.ReadInt32());
    cursor += sizeof(uint);
  }
  return data;
}

CFBStreamedCompression::CFBStreamedCompression(CInputStream& in, IObjectStore&)
: mScratchSize(in.ReadInt32())
, x4_(in.ReadInt8())
, mRotsAndOffs(GetRotationsAndOffsets(mScratchSize / 4 + 1, in).release())
, mRootOffset(0.f, 0.f, 0.f) {
  {
    const CFBStreamedPerChannelHeaderList& channels =
        GetPerChannelHeaderList(TimeHeader(MainHeader()));
    const uint* bytes = GetBytes(channels);
    const uint keyframes = GetNumKeyframes();
    CMemoryInputToBitLevelLoader input(bytes);
    CBitLevelLoader< CMemoryInputToBitLevelLoader > loader(input);
    uint rootIndex = 0;
    for (CFBStreamedPerChannelHeaderList::const_iterator it = channels.begin();
         it != channels.end(); ++it) {
      if (it->GetSegId() == CSegId(0)) {
        break;
      }
      ++rootIndex;
    }
    CFBStreamedAnimReaderTotals totals(*this);
    totals.CalculateDown();
    CVector3f previous = totals.GetVector(rootIndex);
    float distance = 0.f;
    for (uint i = 0; i < keyframes; ++i) {
      totals.IncrementInto(loader, *this, totals);
      totals.CalculateDown();
      const CVector3f current = totals.GetVector(rootIndex);
      CVector3f difference = current - previous;
      previous = current;
      const float delta = difference.Magnitude();
      if (!close_enough(delta, 0.f)) {
        distance += delta;
      }
    }
    mAverageVelocity = distance / GetAnimationDuration().GetSeconds();
  }
  CCharAnimMemoryMetrics::AddToTotalSize(mScratchSize, CCharAnimMemoryMetrics::kASS_Two);
}

CCharAnimTime CFBStreamedCompression::GetAnimationDuration() const {
  return MainHeader().GetMaxTime();
}

CFBStreamedCompression::~CFBStreamedCompression() {
  CCharAnimMemoryMetrics::SubtractFromTotalSize(mScratchSize, CCharAnimMemoryMetrics::kASS_Two);
}
