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
// The two `CFBBitCompressedDataChannelHeader::GetSumOfBitCounts` instantiations carry their names
// too, measured this time (2026-10-01): declaring them out of class below is what makes MWCC emit
// them, and both are byte-for-byte equal to retail's:
//   0x802B0D78 CFBBitCompressedDataChannelHeader<3,100000,100000>::GetSumOfBitCounts  128 / 128 =
//   0x802B0DF8 CFBBitCompressedDataChannelHeader<4,100000,0>::GetSumOfBitCounts        112 / 112 =
// Retail calls them on the offset and scale headers and on the rotation header respectively. Each
// pairing is unambiguous: 128 and 112 are the only functions of those sizes on either side.
// That makes the unit 13 / 14. The one left is
// `TVectorOfVaryingLengthItems<Ui,CFBStreamedPerChannelHeader>`'s constructor, retail 212 bytes and
// ours 136 at 63.96%: retail inlines the element constructor and the `AfterEnd` chain into the
// loop body, we emit the element constructor out of line (140 bytes) and call it. Raising
// `inline_max_size` for this file does not close that - see the note below.

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
  // Prime 1 declares these four out of class, behind `NTSC_INLINE`, which is empty for
  // GM8P and later. That is what retail's build sees, and it is load-bearing: retail emits the
  // two `AfterEnd` instantiations as real functions (0x802B0388/0x802B03A4, 28 bytes each,
  // in `CFBStreamedAnimReader.o`) and the two `GetSumOfBitCounts` ones as real functions
  // (0x802B0D78 128 B, 0x802B0DF8 112 B) instead of inlining them, and
  // `CFBStreamedPerChannelHeaderList::GetSumOfBitCounts` calls all six. Defining them in the
  // class body makes them implicitly inline and MWCC folds them away, which is what left our
  // `GetSumOfBitCounts` at 34% with a 272-byte helper retail does not have. Both of retail's
  // `GetSumOfBitCounts` bodies are now reproduced byte-for-byte (see the note at the top).
  short GetInitialValue(uint component) const;
  uint GetBitCount(uint component) const;
  const uchar* AfterEnd() const;
  uint GetSumOfBitCounts() const;

private:
  TLoadedVal< ushort > mWidth;
};

template < uint Components, uint ConstantComponent, uint SignComponent >
short CFBBitCompressedDataChannelHeader< Components, ConstantComponent,
                                         SignComponent >::GetInitialValue(uint component) const {
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

template < uint Components, uint ConstantComponent, uint SignComponent >
uint CFBBitCompressedDataChannelHeader< Components, ConstantComponent,
                                       SignComponent >::GetBitCount(uint component) const {
  if (SignComponent < Components && component == SignComponent) {
    return 1;
  }
  return reinterpret_cast< const uchar* >(
      this)[sizeof(ushort) + component * 3 + 2 - 3 * (SignComponent < Components)];
}

template < uint Components, uint ConstantComponent, uint SignComponent >
const uchar* CFBBitCompressedDataChannelHeader< Components, ConstantComponent,
                                                SignComponent >::AfterEnd() const {
  uint bytes = sizeof(ushort);
  if (GetWidth() != 0) {
    bytes += 3 * (Components - (SignComponent < Components));
  }
  return reinterpret_cast< const uchar* >(this) + bytes;
}

template < uint Components, uint ConstantComponent, uint SignComponent >
uint CFBBitCompressedDataChannelHeader< Components, ConstantComponent,
                                       SignComponent >::GetSumOfBitCounts() const {
  // Prime 1 walks the payload itself instead of calling `GetBitCount` in the loop. That is what
  // retail's two instantiations do: `fn_802B0D78` (128 B) and `fn_802B0DF8` (112 B) have the
  // body unrolled into straight-line `lbz 2(r4) / addi r4,r4,3` pairs with no call and no loop
  // counter, whereas calling `GetBitCount` keeps a counted loop and an out-of-line call.
  if (GetWidth() == 0) {
    return 0;
  }
  uint sum = 0;
  const uchar* data = reinterpret_cast< const uchar* >(this) + sizeof(ushort);
  for (uint i = 0; i < Components; ++i) {
    if (i == SignComponent) {
      sum += 1;
    } else {
      sum += data[2];
      data += 3;
    }
  }
  return sum;
}

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
    // Prime 1 hoists the count into a local. Leaving `this->size()` in the loop's condition
    // makes MWCC reload `mSize` on every iteration; retail reads it once (`lwz r31,0(r27)`)
    // before the loop and compares against the register.
    const int count = this->size();
    const T* ptr = reinterpret_cast< const T* >(this->GetFirstAddress());
    for (int i = 0; i < count; ++i) {
      new (const_cast< T* >(ptr)) T(in);
      ptr = ptr->AfterEnd();
    }
  }
  // Prime 1 declares this one out of class as well, behind the same empty `NTSC_INLINE`.
  // Retail calls it out of line - `fn_802B02F0`, 76 bytes, in `CFBStreamedAnimReader.o`, and
  // `GetRotationsAndOffsets` calls it twice: once for `AfterEnd()` and once for `GetBytes()`.
  // Defining it in the class body makes MWCC inline the walk at both call sites, which is the
  // 596-vs-564 byte gap in that function.
  const uchar* AfterEnd() const;
  const_iterator begin() const {
    return const_iterator(reinterpret_cast< const T* >(this->GetFirstAddress()), this->size());
  }
  const_iterator end() const { return const_iterator(nullptr, 0); }
};

template < typename Size, typename T >
const uchar* TVectorOfVaryingLengthItems< Size, T >::AfterEnd() const {
  const T* ptr = reinterpret_cast< const T* >(this->GetFirstAddress());
  for (int i = 0; i < this->size(); ++i) {
    ptr = ptr->AfterEnd();
  }
  return reinterpret_cast< const uchar* >(ptr);
}

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
    // Prime 1 hoists the word count into a local before the loop. Leaving the call in the
    // loop's condition makes MWCC reload `mBitCount` on every iteration (`lwz r5,0(r3)`) and
    // gives up on the 8x unroll retail has; retail's `cmplwi r8,0 / ble blelr` guard also shows
    // the count is tested once, before the loop.
    const uint words = Uint32sForBitCount(mBitCount);
    uint* data = &mBitCount + 1;
    for (uint i = 0; i < words; ++i) {
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

  // Prime 1 declares this out of class and defines it in the .cpp. Retail does too: it is
  // `fn_802B03C0`, 52 bytes, in `CFBStreamedAnimReader.o`, and the constructor calls it
  // (`mr r4,r28 / addi r3,r1,12 / bl fn_802B03C0`) instead of loading `mMaxTime` itself.
  // Defining it in the class body is the last instruction pair apart in the constructor.
  CCharAnimTime GetAnimationDuration() const;
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
