#ifndef _CFBSTREAMEDCOMPRESSION
#define _CFBSTREAMEDCOMPRESSION

#include "types.h"

#include "Kyoto/Animation/CSegId.hpp"
#include "Kyoto/Animation/IAnimReader.hpp"
#include "Kyoto/Streams/CInputStream.hpp"
#include "rstl/auto_ptr.hpp"
#include "rstl/pair.hpp"
#include "rstl/single_ptr.hpp"

#include <string.h>

// `config/G2ME01/symbols.txt` called this unit's functions `fn_802B0xxx`, so objdiff could not
// pair them against the ones we compile and scored all nine 0.00% however good the code was.
// They carry their real names now. A pairing is proved either by the target and our object
// holding identical bytes (`=` below), or by a call site that sits at the same offset in both
// objects and lands on the same callee: `GetRotationsAndOffsets` matches 100%, and so do the two
// 48-byte thunks 0x802B0A20 and 0x802B0B24, which is what identifies the two base constructors.
// Sizes are retail / ours, from `powerpc-eabi-nm -S` on
// `build/G2ME01/{obj,src}/Kyoto/Animation/CFBStreamedCompression.o`; the mangled spelling of each
// is the one `symbols.txt` now holds at that address.
//
//   0x802B071C CFBStreamedCompression::GetNumKeyframes                 48 /  48  =
//   0x802B0980 CFBStreamedPerChannelHeaderList::GetSumOfBitCounts      160 /  96
//   0x802B0A20 CFBStreamedPerChannelHeaderList ctor                      48 /  48  =
//   0x802B0A50 TVectorOfVaryingLengthItems<Ui,CFBStreamedPerChannelHeader> ctor  212 / 152
//   0x802B0B24 CFBStreamedCompressionTimeHeader ctor                     48 /  48  =
//   0x802B0B54 CFBKeyFrameReductionPerChannel_HeaderForAll ctor          324 /  96
//   0x802B0C98 CStandardMultiFormatHeader ctor                           224 / 224 =
//   0x802B0E68 CFBBitCompressedDataChannelHeader<4,100000,0> ctor        160 / 160 =
//   0x802B0F08 CFBBitCompressedDataChannelHeader<3,100000,100000> ctor  212 / 212 =
//
// 0x802B0D78 (128) and 0x802B0DF8 (112) stay `fn_`-named for now. Retail calls them on the offset,
// scale and rotation headers respectively, so they are the two
// `CFBBitCompressedDataChannelHeader::GetSumOfBitCounts` instantiations; we emit neither, because
// MWCC inlines ours into one 272-byte `CFBStreamedPerChannelHeader::GetSumOfBitCounts`, so there is
// nothing to pair them with and naming them would only record a guess.

class IObjectStore;

class CStandardMultiFormatHeader {
public:
  explicit CStandardMultiFormatHeader(CInputStream& in)
  : x0_(in.ReadInt8())
  , mMaxTime(in.ReadFloat())
  , mStandardInterval(in.ReadFloat())
  , mRootBoneId(in.ReadInt32())
  , mLooping(in.ReadInt32())
  , mRotationValueForOne(in.ReadInt32())
  , mOffsetResolution(in.ReadFloat())
  , mScaleResolution(in.ReadFloat())
  , mBoneChannelCount(in.ReadInt32())
  , x24_(in.ReadInt32()) {}

  const void* AfterEnd() const { return this + 1; }
  CCharAnimTime GetMaxTime() const { return CCharAnimTime(mMaxTime); }
  CCharAnimTime GetStandardInterval() const { return CCharAnimTime(mStandardInterval); }
  bool IsLooping() const { return mLooping != 0; }
  uint GetRotationValueForOne() const { return mRotationValueForOne; }
  float GetOffsetResolution() const { return mOffsetResolution; }
  float GetScaleResolution() const { return mScaleResolution; }

private:
  uchar x0_;
  float mMaxTime;
  float mStandardInterval;
  uint mRootBoneId;
  uint mLooping;
  uint mRotationValueForOne;
  float mOffsetResolution;
  float mScaleResolution;
  uint mBoneChannelCount;
  uint x24_;
};
CHECK_SIZEOF(CStandardMultiFormatHeader, 0x28)

// Channel records are packed and their scalar fields need not be aligned.
template < typename T >
class TLoadedVal {
public:
  TLoadedVal() {}
  TLoadedVal(T value) { Write(mValue, value); }
  T operator*() const { return Read(mValue); }

  static T Read(const void* data) {
#ifdef __MWERKS__
    return *static_cast< const T* >(data);
#else
    T value;
    memcpy(&value, data, sizeof(value));
    return value;
#endif
  }
  static void Write(void* data, T value) {
#ifdef __MWERKS__
    *static_cast< T* >(data) = value;
#else
    memcpy(data, &value, sizeof(value));
#endif
  }

private:
  uchar mValue[sizeof(T)];
};

// Variable-length payloads follow these headers in the resource buffer.
template < uint Components, uint ConstantComponent, uint SignComponent >
class CFBBitCompressedDataChannelHeader {
public:
  explicit CFBBitCompressedDataChannelHeader(CInputStream& in) {
    ushort width = in.ReadUint16();
    TLoadedVal< ushort >::Write(this, width);
    uchar* data = reinterpret_cast< uchar* >(this) + sizeof(ushort);
    if (width != 0) {
      for (uint i = 0; i < Components; ++i) {
        if (i != SignComponent) {
          TLoadedVal< short >::Write(data, in.ReadInt16());
          data[2] = in.ReadInt8();
          data += 3;
        }
      }
    }
  }

  uint GetWidth() const { return *mWidth; }
  short GetInitialValue(uint component) const {
    if (component == SignComponent) {
      return 0;
    }
    uint index = component;
    if (SignComponent < Components) {
      --index;
    }
    return TLoadedVal< short >::Read(reinterpret_cast< const uchar* >(this) + sizeof(ushort) +
                                     index * 3);
  }
  uint GetBitCount(uint component) const {
    if (SignComponent < Components && component == SignComponent) {
      return 1;
    }
    return reinterpret_cast< const uchar* >(
        this)[sizeof(ushort) + component * 3 + 2 - 3 * (SignComponent < Components)];
  }
  const uchar* AfterEnd() const {
    uint bytes = sizeof(ushort);
    if (GetWidth() != 0) {
      bytes += 3 * (Components - (SignComponent < Components));
    }
    return reinterpret_cast< const uchar* >(this) + bytes;
  }
  uint GetSumOfBitCounts() const {
    if (GetWidth() == 0) {
      return 0;
    }
    uint sum = 0;
    for (uint i = 0; i < Components; ++i) {
      sum += GetBitCount(i);
    }
    return sum;
  }

private:
  TLoadedVal< ushort > mWidth;
};

class CFBStreamedPerChannelHeader {
public:
  typedef CFBBitCompressedDataChannelHeader< 4, 100000, 0 > RotationHeader;
  typedef CFBBitCompressedDataChannelHeader< 3, 100000, 100000 > OffsetHeader;
  typedef CFBBitCompressedDataChannelHeader< 3, 100000, 100000 > ScaleHeader;

  explicit CFBStreamedPerChannelHeader(CInputStream& in) : mSegId(in.ReadInt8()) {
    new (const_cast< RotationHeader* >(&GetRotationBitStorage())) RotationHeader(in);
    new (const_cast< OffsetHeader* >(&GetOffsetBitStorage())) OffsetHeader(in);
    new (const_cast< ScaleHeader* >(&GetScaleBitStorage())) ScaleHeader(in);
  }
  CSegId GetSegId() const { return mSegId; }
  const RotationHeader& GetRotationBitStorage() const {
    return *reinterpret_cast< const RotationHeader* >(this + 1);
  }
  const OffsetHeader& GetOffsetBitStorage() const {
    return *reinterpret_cast< const OffsetHeader* >(GetRotationBitStorage().AfterEnd());
  }
  const ScaleHeader& GetScaleBitStorage() const {
    return *reinterpret_cast< const ScaleHeader* >(GetOffsetBitStorage().AfterEnd());
  }
  const CFBStreamedPerChannelHeader* AfterEnd() const {
    return reinterpret_cast< const CFBStreamedPerChannelHeader* >(GetScaleBitStorage().AfterEnd());
  }
  uint GetSumOfBitCounts() const {
    return GetRotationBitStorage().GetSumOfBitCounts() + GetOffsetBitStorage().GetSumOfBitCounts() +
           GetScaleBitStorage().GetSumOfBitCounts();
  }

private:
  CSegId mSegId;
};
CHECK_SIZEOF(CFBStreamedPerChannelHeader, 0x1)

class TLoadedContainerBase {
protected:
  static void LoadSize(uint& size, CInputStream& in) { size = in.ReadInt32(); }
};

template < typename Size, typename T >
class TArrayInPlaceBase : public TLoadedContainerBase {
public:
  int size() const { return mSize; }
  const uchar* GetFirstAddress() const { return reinterpret_cast< const uchar* >(&mSize + 1); }

protected:
  Size mSize;
};

template < typename Size, typename T >
class TVectorOfVaryingLengthItems : public TArrayInPlaceBase< Size, T > {
public:
  class const_iterator {
  public:
    const_iterator(const T* ptr, int count) : mPtr(ptr), mCount(count) {}
    const_iterator& operator++() {
      --mCount;
      mPtr = mPtr->AfterEnd();
      return *this;
    }
    const T& operator*() const { return *mPtr; }
    const T* operator->() const { return mPtr; }
    bool operator==(const const_iterator& other) const { return mCount == other.mCount; }
    bool operator!=(const const_iterator& other) const { return !(*this == other); }

  private:
    const T* mPtr;
    int mCount;
  };

  explicit TVectorOfVaryingLengthItems(CInputStream& in) {
    this->LoadSize(this->mSize, in);
    const T* ptr = reinterpret_cast< const T* >(this->GetFirstAddress());
    for (int i = 0; i < this->size(); ++i) {
      new (const_cast< T* >(ptr)) T(in);
      ptr = ptr->AfterEnd();
    }
  }
  const uchar* AfterEnd() const {
    const T* ptr = reinterpret_cast< const T* >(this->GetFirstAddress());
    for (int i = 0; i < this->size(); ++i) {
      ptr = ptr->AfterEnd();
    }
    return reinterpret_cast< const uchar* >(ptr);
  }
  const_iterator begin() const {
    return const_iterator(reinterpret_cast< const T* >(this->GetFirstAddress()), this->size());
  }
  const_iterator end() const { return const_iterator(nullptr, 0); }
};

class CFBStreamedPerChannelHeaderList
: public TVectorOfVaryingLengthItems< uint, CFBStreamedPerChannelHeader > {
public:
  explicit CFBStreamedPerChannelHeaderList(CInputStream& in)
  : TVectorOfVaryingLengthItems< uint, CFBStreamedPerChannelHeader >(in) {}
  uint GetSumOfBitCounts() const {
    uint sum = 0;
    for (const_iterator it = begin(); it != end(); ++it) {
      sum += it->GetSumOfBitCounts();
    }
    return sum;
  }
  // Guessed names.
  bool HasOffsetData() const;
  bool HasScaleData() const;
};
CHECK_SIZEOF(CFBStreamedPerChannelHeaderList, 0x4)

class CFBKeyFrameReductionPerChannel_HeaderForAll {
public:
  typedef rstl::pair< const uint*, uint > FrameIterator;

  explicit CFBKeyFrameReductionPerChannel_HeaderForAll(CInputStream& in)
  : mBitCount(in.Get< uint >()) {
    uint* data = &mBitCount + 1;
    for (uint i = 0; i < Uint32sForBitCount(mBitCount); ++i) {
      data[i] = in.Get< uint >();
    }
  }
  // Retail branches here (`clrlwi.` / `addi` / `bne` / `mr`), it is not branchless, and the
  // order of the arms decides which: `bits % 32 ? bits / 32 + 1 : bits / 32` compiles to
  // `beq`+`addi` and misses; this form reproduces retail's `bne`.
  static uint Uint32sForBitCount(uint bits) {
    return bits % 32 == 0 ? bits / 32 : bits / 32 + 1;
  }
  uint FrameAfter(uint frame) const {
    FrameIterator it(reinterpret_cast< const uint* >(this + 1) + frame / 32, 1u << (frame % 32));
    do {
      ++frame;
      Advance(it);
    } while (!FrameAt(it));
    return frame;
  }
  static bool FrameAt(const FrameIterator& it) { return (*it.first & it.second) != 0; }
  static void Advance(FrameIterator& it) {
    it.second <<= 1;
    if (it.second == 0) {
      it.second = 1;
      ++it.first;
    }
  }
  const void* AfterEnd() const {
    return reinterpret_cast< const uint* >(this + 1) + Uint32sForBitCount(mBitCount);
  }

private:
  uint mBitCount;
};
CHECK_SIZEOF(CFBKeyFrameReductionPerChannel_HeaderForAll, 0x4)

class CFBStreamedCompressionTimeHeader : public CFBKeyFrameReductionPerChannel_HeaderForAll {
public:
  explicit CFBStreamedCompressionTimeHeader(CInputStream& in)
  : CFBKeyFrameReductionPerChannel_HeaderForAll(in) {}
};
CHECK_SIZEOF(CFBStreamedCompressionTimeHeader, 0x4)

class CFBStreamedCompression {
public:
  CFBStreamedCompression(CInputStream& in, IObjectStore& store);
  ~CFBStreamedCompression();

  CCharAnimTime GetAnimationDuration() const { return MainHeader().GetMaxTime(); }
  float GetAverageVelocity() const { return mAverageVelocity; }
  bool HasScaleData() const {
    return GetPerChannelHeaderList(TimeHeader(MainHeader())).HasScaleData();
  }
  CSteadyStateAnimInfo GetSteadyStateAnimInfo() const {
    return CSteadyStateAnimInfo(MainHeader().IsLooping(), GetAnimationDuration(), mRootOffset);
  }
  CCharAnimTime FinestSample() const { return MainHeader().GetStandardInterval(); }
  const CStandardMultiFormatHeader& MainHeader() const {
    return *reinterpret_cast< const CStandardMultiFormatHeader* >(mRotsAndOffs.get());
  }
  const CFBStreamedCompressionTimeHeader&
  TimeHeader(const CStandardMultiFormatHeader& header) const {
    return *static_cast< const CFBStreamedCompressionTimeHeader* >(header.AfterEnd());
  }
  const CFBStreamedPerChannelHeaderList&
  GetPerChannelHeaderList(const CFBStreamedCompressionTimeHeader& header) const {
    return *static_cast< const CFBStreamedPerChannelHeaderList* >(header.AfterEnd());
  }
  const uint* GetBytes(const CFBStreamedPerChannelHeaderList& header) const {
    return reinterpret_cast< const uint* >(header.AfterEnd());
  }
  uint GetNumKeyframes() const {
    return GetPerChannelHeaderList(TimeHeader(MainHeader()))
        .begin()
        ->GetRotationBitStorage()
        .GetWidth();
  }

private:
  static rstl::auto_ptr< uint > GetRotationsAndOffsets(uint words, CInputStream& in);

  uint mScratchSize;
  uchar x4_;
  rstl::single_ptr< uint > mRotsAndOffs;
  float mAverageVelocity;
  CVector3f mRootOffset;
};

#endif // _CFBSTREAMEDCOMPRESSION
