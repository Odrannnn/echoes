// #include "dolphin/os/OSArena.h"
#include "dolphin/types.h"
#include "stddef.h"
#include <Kyoto/Alloc/CGameAllocator.hpp>

#include <Kyoto/Alloc/CCallStack.hpp>
#include <Kyoto/Alloc/CMediumAllocPool.hpp>
#include <Kyoto/Alloc/CSmallAllocPool.hpp>
#include <Kyoto/Basics/COsContext.hpp>
#include <Kyoto/Basics/CStopwatch.hpp>

#include <stdint.h>

/* Here just to make sure the data section matches */
static const char* string_NULL = "<NULL>";
static const char* string_SOURCE_MODULE_UNLOADED = "<SOURCE MODULE UNLOADED>";
static const char* string_ = "";
static int gAllocatorTime = 0;

template < typename U1, typename U2 >
static inline U1 T_round_up(U2 val, int align) {
  return (val + (align - 1)) & ~(align - 1);
}

CGameAllocator::SGameMemInfo* CGameAllocator::GetMemInfoFromBlockPtr(const void* ptr) const {
  return reinterpret_cast< SGameMemInfo* >(reinterpret_cast< uintptr_t >(ptr) -
                                           sizeof(SGameMemInfo));
}

CGameAllocator::CGameAllocator()
: x4_(0)
, x8_heapSize(0)
, xc_first(nullptr)
, x10_last(nullptr)
, x54_(0)
, x58_oomCallback(nullptr)
, x5c_oomTarget(nullptr)
, x60_smallAllocPool(nullptr)
, x64_smallAllocMainData(nullptr)
, x68_smallAllocBookKeeping(nullptr)
, x6c_(false)
, x70_(0)
, x74_mediumPool(nullptr)
, x80_(0)
, x84_(0)
, x88_(0)
, x8c_(0)
, x90_heapSize2(0)
, x94_(0)
, x98_(0)
, x9c_(0)
, xa0_(0)
, xa4_(0)
, xa8_(0)
, xac_(0)
, xb0_(0)
, xb4_(0)
, xb8_physicalAddr(nullptr)
, xbc_fakeStatics(0)
, xc0_(0) {}

CGameAllocator::~CGameAllocator() {
  if (x74_mediumPool) {
    x74_mediumPool->ClearPuddles();
    FreeNormalAllocation(x74_mediumPool);
    x74_mediumPool = nullptr;
  }
}

bool CGameAllocator::Initialize(COsContext& ctx) {
  x8_heapSize = ctx.GetBaseFreeRam() - 2 * sizeof(SGameMemInfo);
  xc_first = static_cast< SGameMemInfo* >(OSAllocFromArenaLo(x8_heapSize, sizeof(SGameMemInfo)));
  xb8_physicalAddr = reinterpret_cast< void* >(
      reinterpret_cast< intptr_t >(xc_first) -
      (reinterpret_cast< uintptr_t >(xc_first) & kAllocatorPointerTopNybbleMask));
  OSGetArenaLo();
  x10_last =
      reinterpret_cast< SGameMemInfo* >(reinterpret_cast< char* >(xc_first) + x8_heapSize) - 1;

  const SGameMemInfo& head = SGameMemInfo(
      nullptr, x10_last, x10_last, x8_heapSize - sizeof(SGameMemInfo) * 2, "MemHead", "MemHead");
  *xc_first = head;
  const SGameMemInfo& tail = SGameMemInfo(xc_first, nullptr, nullptr, 0, "MemTail", "MemTail");
  *x10_last = tail;
  for (uint i = 0; i < 16; i++) {
    x14_bins[i] = nullptr;
  }

  AddFreeEntryToFreeList(xc_first);
  x80_ = 0;
  x84_ = 0;
  x88_ = 0;
  x8c_ = 0;
  x90_heapSize2 = x8_heapSize;
  x94_ = 0;
  x98_ = 0;
  x9c_ = 0;
  xa0_ = 0;
  xa4_ = 0;
  xa8_ = 0;
  xac_ = 0;
  x4_ = 1;

  x64_smallAllocMainData = Alloc(0xb0000, kHI_None, kSC_Unk1, kTP_Heap,
                                 CCallStack(0xffffffff, "SmallAllocMainData   ", " - Ignore"));

  x68_smallAllocBookKeeping = Alloc(0x16000, kHI_None, kSC_Unk1, kTP_Heap,
                                    CCallStack(0xffffffff, "SmallAllocBookKeeping", " - Ignore"));

  // The two placement-news below ask for `sizeof(the object)`, which is what retail's 0x20 and
  // 0x1c *are* - measured with mwcceppc, sizeof(CSmallAllocPool) = 0x20 and
  // sizeof(CMediumAllocPool) = 0x1c, so this is a no-op for the decomp build.
  //
  // Hardcoding retail's number was a third instance of the kAllocatorPointerBits defect, and it
  // was the one that corrupted the heap. sizeof(CSmallAllocPool) is 0x30 on a 64-bit host
  // (measured) against 0x20 on retail, because its members are pointers; sizeof(CMediumAllocPool)
  // is 0x38 against 0x1c, because its rstl::list is. Constructing a 0x30-byte object in a 0x20-byte
  // cell wrote its last 16 bytes over the *next* block's header: x10_/x14_ = -1 landed on that
  // block's x0_priorGuard, x18_numBlocksAvailable (0x2c000, then -4) landed on its x4_len, and
  // x1c_numAllocs (0, then 1) landed on x4_len's high half - a 4-byte store into a 64-bit field,
  // which is why the block reported 180220 with an upper half of 1. `FindFreeBlock`'s
  // `x4_len - len < bestDelta` then promoted to 64-bit and failed against bestDelta = 0x10000000,
  // so the block the 135168-byte request wanted was rejected, Alloc returned null and x78_ (the
  // medium pool's memory) stayed null.
  //
  // The literal cannot be "corrected" to a host number: it is a property of the type, so it is
  // spelled as one. Same shape as the mask fix - the number follows the type rather than the host.
  x60_smallAllocPool = new (Alloc(sizeof(CSmallAllocPool), kHI_None, kSC_Unk1, kTP_Heap,
                                  CCallStack(0xffffffff, "SmallAllocClass      ", " - Ignore")))
      CSmallAllocPool(0x2c000, x64_smallAllocMainData, x68_smallAllocBookKeeping);

  x74_mediumPool =
      new (Alloc(sizeof(CMediumAllocPool), kHI_None, kSC_Unk1, kTP_Heap,
                 CCallStack(0xffffffff, "MediumAllocClass      ", " - Ignore"))) CMediumAllocPool();

  uint mediumSize = CMediumAllocPool::GetAllocMemoryRequired(0x1000);
  mediumSize += CMediumAllocPool::GetBookKeepingMemoryRequired(0x1000);
  x78_ = Alloc(mediumSize, kHI_None, kSC_Unk1, kTP_Heap,
               CCallStack(0xffffffff, "MediumAllocMainData   ", " - Ignore"));
  x84_ -= 4;
  xc0_ = 0xc6000;
  return true;
}

void CGameAllocator::Shutdown() {
  ReleaseAll();
  x4_ = 0;
  x54_ = 0;
}

void* CGameAllocator::Alloc(size_t size, const EHint hint, const EScope scope, const EType type,
                            const CCallStack& callstack) {
  uint tmp = 0;
  void* buf = nullptr;
  const OSTick startTick = OSGetTick();

  if (hint & kHI_RoundUpLen) {
    size = T_round_up< size_t, size_t >(size, 32);
  }

  bool bVar1 = size <= 56 && !(hint & (kHI_RoundUpLen | kHI_TopOfHeap)) && x60_smallAllocPool;

  if (bVar1 && x70_ > 0) {
    bVar1 = false;
    --x70_;
  }

  if (bVar1) {
    buf = x60_smallAllocPool->Alloc(size);
    tmp = x60_smallAllocPool->GetAllocatedSize();
    if (xb0_ < tmp) {
      xb0_ = tmp;
      static int sLastSmallAllocSize = 0;
      if (sLastSmallAllocSize + 128 < tmp) {
        sLastSmallAllocSize = tmp;
      }
    }

    if (buf != nullptr) {
      gAllocatorTime += (OSGetTick() - startTick);
      return buf;
    }
    x70_ = 25;
    x6c_ = true;
  }

#ifdef TARGET_PC
  // The medium pool must not be entered while it is being grown; see the comment inside the
  // branch. `static` rather than a member because a member would be a new field in a class whose
  // layout is retail's, and a plain global would need a definition in a `.data` section this
  // unit does not own.
  //
  // `#ifdef TARGET_PC` is load-bearing and not decoration: `CGameAllocator::Alloc` is one of the
  // three functions in this unit that are **not** `Matching` (98.90%, 884 bytes, retail
  // 0x8030D6E8), but its bytes are still in main.dol, so mwcceppc must compile exactly what it
  // compiled before or `sha1sum build/G2ME01/main.dol` stops being 6ef9b491. mwcceppc does not
  // define TARGET_PC, so the retail branch below is the pre-existing expression tree.
  static bool sGrowingMediumPool = false;
#endif // TARGET_PC

  if (x74_mediumPool
#ifdef TARGET_PC
      && !sGrowingMediumPool
#endif // TARGET_PC
      && size <= 0x400 && !(hint & kHI_TopOfHeap)) {
#ifdef TARGET_PC
    // **This guard is what stops the medium pool from recursing into itself.**
    //
    // `AddPuddle` pushes a `rstl::list< SMediumAllocPuddle >` node. On retail that node is
    // `2 * sizeof(void*) + sizeof(SMediumAllocPuddle)` = 8 + 8 + 0x24 = **52** bytes, which is
    // under the small pool's 56-byte ceiling, so the node comes out of the *small* pool and this
    // function is not re-entered. Measured on the 64-bit host: `sizeof(SMediumAllocPuddle)` is 48
    // because every member is a pointer, so the node is **64** bytes; 64 > 56, the node's own
    // allocation comes back in here with the puddle list still empty, `!HasPuddles()` is still
    // true, and `AddPuddle` allocates another node. Unbounded recursion, and it is what the boot
    // hit as soon as `rstl::basic_string` buffers were routed into the game heap
    // (src/rstl/rstl_misc.cpp). Captured as a repeating four-frame backtrace and a SIGSEGV on
    // the stack guard, not on a bad pointer:
    //
    //   CMemory::Alloc -> CGameAllocator::Alloc -> CMediumAllocPool::AddPuddle
    //     -> rstl::rmemory_allocator::allocate -> CMemory::Alloc -> CGameAllocator::Alloc -> ...
    //
    // So the nested allocation takes the normal block path below instead, which is the same place
    // retail's 52-byte node comes from. **This is not a relaxation of a check** - it removes a
    // cycle, and the only behaviour it suppresses is the pool trying to grow itself with memory
    // that the growth is itself trying to allocate. The `bTriedCallback` guard further down is
    // retail's own precedent for exactly this shape in this function.
    sGrowingMediumPool = true;
#endif // TARGET_PC
    if (!x74_mediumPool->HasPuddles()) {
      buf = nullptr;
      x74_mediumPool->AddPuddle(0x1000, x78_, false);
      x78_ = buf;
    }

    buf = x74_mediumPool->Alloc(size);

    if (buf == nullptr) {
      void* puddlePtr = Alloc(CMediumAllocPool::GetAllocMemoryRequired(0x1000) +
                                  CMediumAllocPool::GetBookKeepingMemoryRequired(0x1000),
                              kHI_None, kSC_Unk1, kTP_Heap,
                              CCallStack(-1, "MediumAllocMainData   ", " - Ignore"));
      x74_mediumPool->AddPuddle(0x1000, puddlePtr, true);
      buf = x74_mediumPool->Alloc(size);
    }

#ifdef TARGET_PC
    sGrowingMediumPool = false;
#endif // TARGET_PC

    if (buf != nullptr) {
      gAllocatorTime += OSGetTick() - startTick;
      return buf;
    }

    x7c_ = true;
  }

  const bool topOfHeap = (hint & kHI_TopOfHeap) != 0;
  uint roundedSize = T_round_up< uint, size_t >(size, 32);
  SGameMemInfo* info = nullptr;

  if (topOfHeap) {
    info = FindFreeBlockFromTopOfHeap(roundedSize);
  } else {
    info = FindFreeBlock(roundedSize);
  }

  if (info == nullptr) {
    void* mediumBuf = nullptr;
    if (x58_oomCallback) {
      x58_oomCallback(x5c_oomTarget, size);

      static bool bTriedCallback = false;
      if (!bTriedCallback) {
        bTriedCallback = true;
        mediumBuf = Alloc(size, hint, scope, type, callstack);
        bTriedCallback = false;
      } else {
        return nullptr;
      }
    }
    if (mediumBuf == nullptr) {
      DumpAllocations();
      return nullptr;
    }
    return mediumBuf;
  }

  tmp = FixupAllocPtrs(info, size, roundedSize, hint, callstack);
  if (topOfHeap && !info->IsAllocated()) {
    info = info->GetNext();
  }

  UpdateAllocDebugStats(size, roundedSize, tmp);
  gAllocatorTime += OSGetTick() - startTick;
  return ++info;
}

CGameAllocator::SGameMemInfo* CGameAllocator::FindFreeBlock(uint len) {
  CGameAllocator::SGameMemInfo* ret = nullptr;
  uint binIndex = GetFreeBinEntryForSize(len);

  uint chosenBin = 0;
  SGameMemInfo* previous = nullptr;
  uint bestDelta = 0x10000000;

  for (; binIndex < 16 && !ret; ++binIndex) {
    SGameMemInfo* candidate = x14_bins[binIndex];
    SGameMemInfo* last = nullptr;
    for (; candidate; last = candidate, candidate = candidate->GetNextFree()) {
      if (!candidate->IsAllocated() && candidate->x4_len >= len) {
        if (candidate->x4_len - len < bestDelta && candidate->GetNext()) {
          ret = candidate;
          previous = last;
          bestDelta = candidate->x4_len - len;
          chosenBin = binIndex;
          if (bestDelta < sizeof(SGameMemInfo)) {
            break;
          }
        }
      }
    }
  }

  if (ret) {
    if (previous == NULL) {
      x14_bins[chosenBin] = ret->GetNextFree();
    } else {
      previous->SetNextFree(ret->GetNextFree());
    }
  }
  return ret;
}

CGameAllocator::SGameMemInfo* CGameAllocator::FindFreeBlockFromTopOfHeap(uint size) {
  SGameMemInfo* iter = x10_last;
  SGameMemInfo* ret = nullptr;

  while (iter != nullptr) {
    if (!iter->IsAllocated() && iter->GetLength() >= size) {
      ret = iter;
      break;
    }
    iter = iter->GetPrev();
  }

  RemoveFreeEntryFromFreeList(ret);
  return ret;
}

uint CGameAllocator::FixupAllocPtrs(SGameMemInfo* info, const uint len, uint roundedLen, EHint hint,
                                    const CCallStack& cs) {

  const bool topOfHeap = (hint & kHI_TopOfHeap) != 0;
  uint ret = 0;
  const size_t blockLength = info->x4_len;
  if (blockLength == roundedLen + sizeof(SGameMemInfo)) {
    ret = sizeof(SGameMemInfo);
    roundedLen += sizeof(SGameMemInfo);
  }

  SGameMemInfo* newPtr = info;
  if (blockLength != roundedLen) {
    SGameMemInfo* newInfo;

    SGameMemInfo* infoNext = info->GetNext();
    if (topOfHeap) {
      newInfo =
          reinterpret_cast< SGameMemInfo* >(reinterpret_cast< char* >(infoNext) - roundedLen) - 1;
      const SGameMemInfo& block = SGameMemInfo(info, infoNext, nullptr, len, "", "");
      *newInfo = block;
      info->x4_len -= roundedLen + sizeof(SGameMemInfo);
      AddFreeEntryToFreeList(info);
      newPtr = newInfo;
    } else {
      newInfo = reinterpret_cast< SGameMemInfo* >(reinterpret_cast< uintptr_t >(info) + roundedLen +
                                                  sizeof(SGameMemInfo));
      const SGameMemInfo& block =
          SGameMemInfo(info, infoNext, info->GetNextFree(),
                       info->x4_len - roundedLen - sizeof(SGameMemInfo), "", "");
      *newInfo = block;
      AddFreeEntryToFreeList(newInfo);
    }
    newPtr->x8_fileAndLine = cs.GetFileAndLineText();
    newPtr->xc_type = cs.GetTypeText();
    ret = sizeof(SGameMemInfo);

    infoNext->SetPrev(newInfo);
    info->SetNext(newInfo);
  } else {
    info->x8_fileAndLine = cs.GetFileAndLineText();
    info->xc_type = cs.GetTypeText();
  }

  newPtr->SetTopOfHeapAllocated(topOfHeap);
  newPtr->SetAllocated(true);
  newPtr->x4_len = len;
  return ret;
}

void CGameAllocator::UpdateAllocDebugStats(uint len, uint roundedLen, uint offset) {
  ++x84_;
  ++x80_;
  x88_ += len;
  x8c_ += roundedLen + offset;
  x90_heapSize2 -= roundedLen + offset;

  if (x84_ > x94_) {
    x94_ = x84_;
  }

  if (x8c_ > x98_) {
    x98_ = x8c_;
  }

  if (x8c_ > x9c_) {
    x9c_ = x8c_;
  }

  if (len < xa0_) {
    xa0_ = len;
  }

  if (len > xa4_) {
    xa4_ = len;
  }
  xa8_ = (len + xa8_ * (x80_ - 1)) / x80_;
  if (len > 56) {
    return;
  }

  ++xac_;
}

bool CGameAllocator::Free(const void* ptr) {
  if (ptr == nullptr) {
    return true;
  }

  if (x60_smallAllocPool && x60_smallAllocPool->PtrWithinPool(ptr)) {
    return x60_smallAllocPool->Free(ptr);
  }

  if (x74_mediumPool) {
    int tmp = x74_mediumPool->Free(ptr);
    if (tmp != 1) {
      return tmp > 0;
    }
  }
  return FreeNormalAllocation(ptr);
}

bool CGameAllocator::FreeNormalAllocation(const void* ptr) {
  SGameMemInfo* info = GetMemInfoFromBlockPtr(ptr);
  size_t newLen = 0;
  const size_t infoLen = info->x4_len;
  SGameMemInfo* k = info->GetNext();
  size_t len = 0;
  if (k) {
    len = reinterpret_cast< size_t >(k) - reinterpret_cast< size_t >(info) - sizeof(SGameMemInfo);
  }
  info->SetLength(len);

  SGameMemInfo* prev = info->GetPrev();
  SGameMemInfo* next = info->GetNext();

  if (prev && !prev->IsAllocated()) {
    RemoveFreeEntryFromFreeList(prev);
    prev->SetNext(next);
    if (next) {
      next->SetPrev(prev);
    }
    newLen = sizeof(SGameMemInfo);
    prev->x4_len += info->x4_len + sizeof(SGameMemInfo);
    info = prev;
  }

  if (next && !next->IsAllocated() && next->GetNext()) {
    RemoveFreeEntryFromFreeList(next);
    info->SetNext(next->GetNext());
    if (info->GetNext()) {
      info->GetNext()->SetPrev(info);
    }
    newLen += sizeof(SGameMemInfo);
    info->x4_len += next->x4_len + sizeof(SGameMemInfo);
    info->SetAllocated(false);
  } else {
    info->SetAllocated(false);
  }
  AddFreeEntryToFreeList(info);

  x84_ -= 1;
  x88_ -= infoLen;
  x8c_ -= (len + newLen);
  x90_heapSize2 += (len + newLen);
  if (infoLen <= 56) {
    xac_ -= 1;
  }

  return true;
};

void CGameAllocator::ReleaseAll() {
  if (x74_mediumPool) {
    x74_mediumPool->ClearPuddles();
    FreeNormalAllocation(x74_mediumPool);
    x74_mediumPool = nullptr;
  }

  SGameMemInfo* iter = xc_first;
  while (iter != nullptr) {
    SGameMemInfo* next = iter->GetNext();
    if (iter->IsAllocated()) {
      FreeNormalAllocation(((uchar*)iter) + sizeof(SGameMemInfo));
    }
    iter = next;
  }

  xc_first = nullptr;
  x10_last = nullptr;
};

void* CGameAllocator::AllocSecondary(size_t size, EHint hint, EScope scope, EType type,
                                     const CCallStack& callstack) {
  return Alloc(size, hint, scope, type, callstack);
};

bool CGameAllocator::FreeSecondary(const void* ptr) { return Free(ptr); };

void CGameAllocator::ReleaseAllSecondary() {};

void CGameAllocator::SetOutOfMemoryCallback(FOutOfMemoryCb cb, const void* target) {
  x58_oomCallback = cb;
  x5c_oomTarget = target;
};

IAllocator::SAllocInfo CGameAllocator::GetAllocInfo(const void* ptr) const {
  const SGameMemInfo* info = GetMemInfoFromBlockPtr(ptr);

  return SAllocInfo(info, info->GetLength(), info->IsAllocated(), false, info->x8_fileAndLine,
                    info->xc_type);
};

IAllocator::SMetrics CGameAllocator::GetMetrics(bool unk1, bool unk2) const {
  uint mediumAllocTotalAllocated =
      x74_mediumPool != nullptr ? x74_mediumPool->GetTotalEntries() * 32 : 0;
  uint mediumAllocBlocksAvailable =
      x74_mediumPool != nullptr ? x74_mediumPool->GetNumBlocksAvailable() : 0;
  uint mediumAllocAllocatedSize =
      x74_mediumPool != nullptr
          ? x74_mediumPool->GetTotalEntries() - x74_mediumPool->GetNumBlocksAvailable()
          : 0;
  const uint mediumAllocNumAllocs = x74_mediumPool != nullptr ? x74_mediumPool->GetNumAllocs() : 0;
  SMetrics ret(x8_heapSize, x80_, x84_, x88_, x8c_, x90_heapSize2, x94_, x98_, x9c_, xa0_, xa4_, xa8_,
               x60_smallAllocPool != nullptr ? x60_smallAllocPool->GetNumAllocs() : 0,
               x60_smallAllocPool != nullptr ? x60_smallAllocPool->GetAllocatedSize() : 0,
               x60_smallAllocPool != nullptr ? x60_smallAllocPool->GetNumBlocksAvailable() : 0,
               mediumAllocNumAllocs, mediumAllocAllocatedSize, mediumAllocBlocksAvailable,
               x80_ - xb4_, reinterpret_cast< uintptr_t >(xb8_physicalAddr), xc0_,
               mediumAllocTotalAllocated, xbc_fakeStatics);
  xb4_ = x80_;
  if (unk1) {
    x9c_ = 0;
  }
  if (unk2) {
    x98_ = 0;
  }
  return ret;
};

int CGameAllocator::EnumAllocations(FEnumAllocationsCb func, const void* ptr, bool b) const {

  int i = 0;
  const SGameMemInfo* iter = xc_first;

  while (iter != nullptr) {
    if (!iter->IsPostGuardIntact()) {
      return -1;
    }

    if (!iter->IsPriorGuardIntact()) {
      return -1;
    }

    const SGameMemInfo* next = iter->GetNext();
    SAllocInfo alloc(iter, iter->GetLength(), iter->IsAllocated(), false, iter->x8_fileAndLine,
                     iter->xc_type);
    func(alloc, ptr);
    ++i;
    iter = next;
  }

  return i;
};

uint CGameAllocator::GetFreeBinEntryForSize(const uint size) {
  uint maxLen = 0x20;
  uint bin = 0;

  while (maxLen < 0x200000) {
    if (size < maxLen) {
      return bin;
    }

    maxLen <<= 1;
    ++bin;
  }

  return 0xf;
}

void CGameAllocator::AddFreeEntryToFreeList(SGameMemInfo* info) {
  uint bin = GetFreeBinEntryForSize(info->GetLength());
  info->SetNextFree(x14_bins[bin]);
  x14_bins[bin] = info;
}

void CGameAllocator::RemoveFreeEntryFromFreeList(SGameMemInfo* memInfo) {
  uint bin = GetFreeBinEntryForSize(memInfo->GetLength());
  SGameMemInfo* curBin = nullptr;
  SGameMemInfo* binIt = x14_bins[bin];

  while (binIt != nullptr) {
    if (binIt == memInfo) {
      if (curBin == nullptr) {
        x14_bins[bin] = binIt->GetNextFree();
      } else {
        curBin->SetNextFree(binIt->GetNextFree());
      }
      return;
    }

    curBin = binIt;
    binIt = binIt->GetNextFree();
  }
}

static inline bool DoWait(int v) { return (v % 4) == 0; }

void CGameAllocator::DumpAllocations() const {
  uint i = 0;
  SGameMemInfo* iter = xc_first;

  while (iter != nullptr) {
    ++i;

    if (DoWait(i)) {
      CStopwatch::Wait(0.005f);
    }
    iter = iter->GetNext();
  }
}

void CGameAllocator::OffsetFakeStatics(const int offset) { xbc_fakeStatics += offset; }
