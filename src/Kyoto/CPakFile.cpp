// Retail's build contains two revisions of the `rstl` headers and `Kyoto/CPakFile.o` is the one
// object compiled against the second. Two independent codegen decisions mark it, and both are
// visible in its `reserve<rstl::vector<CPakFile::SResInfo>>` (`.text:0x80324A64`):
//
//  * it inlines `rstl::rmemory_allocator::allocate`, as
//    `CMemory::Alloc(size, kHI_RoundUpLen, kSC_Unk1, kTP_Heap, CCallStack(-1, "??(??)"))`,
//    where all fifteen other `reserve` instantiations in the DOL call the out-of-line
//    `allocate(int)` at `0x802FDAB8`, which is `kHI_None`/`kTP_Array` via `operator new[]`; and
//  * it inlines `rstl::uninitialized_copy`'s element loop, where the other fifteen - including
//    `reserve<vector<SConnection>>` at 0x800485E4, which is byte for byte what this tree
//    produces by default - call it out of line.
//
// The two bodies differ in literal arguments, so this has to be selected per translation unit,
// and this is the only translation unit in the tree that selects it. The full disassembly, the
// fifteen other addresses, and the measured cost of inlining both everywhere (six functions at
// 100% in three `Matching` units, linked 1771 -> 1765) are in
// `include/rstl/rmemory_allocator.hpp` and `include/rstl/construct.hpp`. Without this
// `reserve<vector<CPakFile::SResInfo>>` reads 33.84% and this unit 88.30%; with it 99.74% and
// 90.36%.
//
// It has to be set before *any* include, which is the same constraint as
// `CResLoaderAddPakFileAsync.cpp` and for the same reason: `rstl/rmemory_allocator.hpp` is
// reached from `Kyoto/CPakFile.hpp` -> `rstl/vector.hpp`, and the macro is read inside the
// class body, so an include first would silently give this unit the other revision and the
// regression would look like a scoring accident.
#define RSTL_INLINE_RESERVE_HELPERS

// `lbl_803B0098` - `.rodata:0x803B0098`, the 0x58 bytes this unit's split claims, and retail's
// linker merged **two** strings into it: the 72-character pak-version message that
// `CPakFile::InitialHeaderLoad` hands to `sprintf` at 0x80323F58, and at **+76** the 7-byte
// `"??"(??)?"` that the inlined `rstl::rmemory_allocator::allocate` passes as its `CCallStack`'s
// file-and-line text (0x80324AA4/0x80324AB4, `addi r5,r5,76`).
//
//   803b0098  25 73 3a 20 49 6e 63 6f 6d 70 61 74   "%s: Incompat"
//   ...
//   803b00d8  72 65 20 75 73 69 6e 67 20 25 78 00   "re using %x\0"
//   803b00e4  3f 3f 28 3f 3f 29 00                  "??(??)\0"
//
// Both are named rather than spelled as literals, for the reason
// `src/Kyoto/CResLoaderAddPakFileAsync.cpp` gives for `lbl_803AFAA0`: mwcceppc emits a literal as
// a local `@stringBase0`, and objdiff then has no symbol to pair the relocation against, which is
// the whole of what is missing from `InitialHeaderLoad` and from
// `reserve<vector<CPakFile::SResInfo>>`. Naming it is also the only way this unit can ever own
// 0x803B0098..0x803B00F0 legally, since a `Matching` unit may not own a `.rodata` byte.
#define RSTL_ALLOCATE_FILE_AND_LINE (lbl_803B0098 + 76)

extern "C" const char lbl_803B0098[];

// Ported from upstream PrimeDecomp/echoes @ d83da79: src/Kyoto/CPakFile.cpp
#include "Kyoto/CPakFile.hpp"

#include "Kyoto/Alloc/CMemory.hpp"
#include "Kyoto/CARAMManager.hpp"
#include "Kyoto/CFactoryMgr.hpp"
#include "Kyoto/Math/CMath.hpp"
#include "Kyoto/Streams/CMemoryInStream.hpp"
#include "rstl/StringExtras.hpp"
#include "rstl/algorithm.hpp"
#include "rstl/math.hpp"
#include "rstl/reserved_vector.hpp"

#include <stdio.h>

#ifdef TARGET_PC
#include "dolphin/os.h"
#endif

// Only the host's bounded pump in ~CPakFile reads this; see the comment there for why the
// unbounded version cannot be used on a host. 1<<16 pumps is far more than the three a healthy
// pak needs and is still a few milliseconds, because the loop yields whenever the phase does
// not advance.
static const int kHostMaxIdlePumps = 1 << 16;

CPakFile::SResInfo::SResInfo(uint id, uint fourCC, uint offset, uint size, uint flags,
                            uint groupedSize)
: x0_id(id) {
  const uint typeIdx = CFactoryMgr::FourCCToTypeIdx(fourCC);
  x4_data[0] = static_cast< uchar >(typeIdx | (flags != 0 ? 0x80 : 0));
  x4_data[1] = static_cast< uchar >(offset >> 5);
  x4_data[2] = static_cast< uchar >(offset >> 13);
  x4_data[3] = static_cast< uchar >((offset >> 21) | ((size << 2) & 0x80));
  x4_data[4] = static_cast< uchar >(size >> 6);
  x4_data[5] = static_cast< uchar >(size >> 14);
  x4_data[6] = static_cast< uchar >(groupedSize >> 5);
}

uint CPakFile::SResInfo::GetType() const { return CFactoryMgr::TypeIdxToFourCC(x4_data[0] & 0x7f); }

uint CPakFile::SResInfo::GetOffset() const {
  return ((x4_data[1] | (x4_data[2] << 8) | (x4_data[3] << 16)) & 0x7fffff) << 5;
}

uint CPakFile::SResInfo::GetSize() const {
  return ((x4_data[3] >> 7) | (x4_data[4] << 1) | (x4_data[5] << 9)) << 5;
}

bool CPakFile::SResInfo::IsCompressed() const { return (x4_data[0] & ~0x7f) != 0; }

void CPakFile::SResInfo::SetGroupedSize(uint size) { x4_data[6] = static_cast< uchar >(size >> 5); }

uint CPakFile::SResInfo::GetGroupedSize() const { return x4_data[6] << 5; }

CPakFile::CPakFile(const rstl::string& filename, bool buildDepList, bool worldPak)
: x0_file(filename.data())
, x28_buildDepList(buildDepList)
, x28_aramFile(x0_file.IsARAMFile())
, x28_worldPak(worldPak)
, x28_stashedInARAM(false)
, x2c_asyncLoadPhase(kAP_Warmup)
, x48_resTableOffset(0)
, x4c_resTableCount(0)
, x50_fakeStaticSize(0)
, x54_aramBase(CARAMManager::GetInvalidAlloc())
, x94_currentSeek(-1) {}

CPakFile::~CPakFile() {
  // Retail, .text:0x803244B4-0x803244C8, is exactly the loop below and nothing else:
  //
  //   b        0x803244C0        ; enter at the test, so it is a do-while's back edge
  //   mr  r3,r30 ; bl AsyncIdle
  //   lwz r0,44(r30) ; cmpwi r0,3 ; bne 0x803244B8
  //
  // i.e. spin on `CPakFile::AsyncIdle` until `x2c_asyncLoadPhase` reaches kAP_Loaded.
  //
  // ---------------------------------------------------------------------------
  // Why the host cannot use that loop unconditionally, measured rather than assumed
  // ---------------------------------------------------------------------------
  //
  // On retail's own paths the spin is a no-op, and it is worth saying why, because the obvious
  // reading of `AddPakFileAsync` is wrong. That function does contain a same-call `delete`
  // (`if (inList) delete pakFile;`), but the insert it calls **clears the caller's flag byte** -
  // `fn_802FC378`'s `stb r0,0(r30)` with `r0 = 0` at 0x802fc3d0 - so the flag comes back clear
  // and the branch is not taken. The other two callers of `~CPakFile` are both safe too:
  // `fn_802FD174` (the list erase) only destroys an entry whose pak `IsCompletelyLoaded()` (that
  // is the `lwz r0,44(r30); cmpwi r0,3` test in `fn_802FCCF4` at 0x802fcd34), and retail's
  // `~CResLoader` runs after the frame loop has stopped.
  //
  // So the loop is a hazard on a host for a *different* reason, and it is a real one: a `CPakFile`
  // whose load never completes is destroyed by **any** path that does not test the phase, and
  // `CPakFile::InitialHeaderLoad` (0x80323F0C) `return`s **without touching x2c_asyncLoadPhase**
  // when the first word it reads is not 0x30005 - the version check at 0x80323F58. The phase stays
  // kAP_InitialHeaderLoad, `AsyncIdle` re-enters the same branch, and the loop below is a provable
  // infinite loop. A pak name that resolves to a file whose first word is not 0x30005 - a pak not
  // written by retail's own writer, or a truncated one, since `CInputStream::ReadInt32` has no
  // bounds check (include/Kyoto/Streams/CInputStream.hpp:52) - therefore hangs any host shutdown
  // that walks the loader's lists, and it hangs *silently*, with no frame ever drawn.
  //
  // A healthy pak does finish, incidentally, and not by luck: Aurora's `DVDReadAsync` really is
  // asynchronous (`extern/aurora/lib/dolphin/dvd/dvd.cpp:1166` -> `DVDReadAbsAsyncPrio`, a worker
  // thread) and `CRealDvdRequest::IsComplete` polls `cb.state`, so Warmup -> InitialHeaderLoad ->
  // DataLoad -> Loaded takes three pumps. The unbounded loop below works for those; it is the
  // failure case that never returns.
  //
  // The wait is **not** dropped. The host still pumps the load to completion and only gives up
  // when the state machine has stopped advancing, because a destructor that never returns is
  // strictly worse than a pak that failed to load - and it says which pak and which phase on the
  // way out, so the failure is diagnosable. mwcceppc does not define TARGET_PC, so the matching
  // build compiles retail's loop unchanged, and the per-function report diff confirms it: this
  // unit's scores are identical before and after the block below was added.
#ifdef TARGET_PC
  for (int pumps = 0; x2c_asyncLoadPhase != kAP_Loaded && pumps < kHostMaxIdlePumps; ++pumps) {
    const EAsyncPhase before = x2c_asyncLoadPhase;
    AsyncIdle();
    if (x2c_asyncLoadPhase == before) {
      // The DVD read this phase issued is still in flight. Aurora services it on a worker
      // thread, so the host has to give that thread the CPU - retail gets this for free
      // because retail's pump is the frame loop, which yields anyway.
      OSYieldThread();
    }
  }
  if (x2c_asyncLoadPhase != kAP_Loaded) {
    printf("CPakFile('%s'): gave up after %d idle pumps, phase %d != kAP_Loaded.\n"
           "  A pak whose version word is not 0x30005 leaves the phase at kAP_InitialHeaderLoad\n"
           "  forever (CPakFile::InitialHeaderLoad returns without advancing it), so this pak is\n"
           "  unusable and everything read out of it will be null.\n",
           x0_file.GetFilename().data(), kHostMaxIdlePumps, x2c_asyncLoadPhase);
  }
#else
  while (x2c_asyncLoadPhase != kAP_Loaded) {
    AsyncIdle();
  }
#endif
  CMemory::OffsetFakeStatics(-x50_fakeStaticSize);
  CARAMManager::Free(x54_aramBase);
}

uint CPakFile::GetFakeStaticSize() const {
  return x4c_resTableCount * sizeof(SResInfo) + x68_depList.size() * sizeof(CAssetId) +
         x78_resList.size() * sizeof(SResInfo) + x88_bucketOffsets.size() * sizeof(uint);
}

void CPakFile::UpdateFakeStaticSize() {
  const int newSize = GetFakeStaticSize();
  CMemory::OffsetFakeStatics(newSize - x50_fakeStaticSize);
  x50_fakeStaticSize = newSize;
}

void CPakFile::AsyncIdle() {
  if (x2c_asyncLoadPhase != kAP_Loaded && x0_file.IsARAMFileLoaded() &&
      (x30_dvdReq.null() || x30_dvdReq->IsComplete())) {
    switch (x2c_asyncLoadPhase) {
    case kAP_Warmup:
      Warmup();
      break;
    case kAP_InitialHeaderLoad:
      InitialHeaderLoad();
      break;
    case kAP_DataLoad:
      DataLoad();
      break;
    default:
      break;
    }
  }
}

void CPakFile::Warmup() {
  const int length = rstl::min_val< int >(x0_file.Length(), 8192);
  x38_headerData.resize(length);
  x30_dvdReq = rstl::auto_ptr< CDvdRequest >(x0_file.SyncRead(x38_headerData.data(), length));
  x2c_asyncLoadPhase = kAP_InitialHeaderLoad;
#ifdef TARGET_PC
  // HOST DIAGNOSTIC: phase 0 -> 1, and the read that phase 1 is now waiting on.
  printf("[pak] %s: Warmup done, %d bytes requested, phase -> kAP_InitialHeaderLoad, "
         "x30_dvdReq=%p\n",
         x0_file.GetFilename().data(), length, static_cast< void* >(x30_dvdReq.get()));
  fflush(nullptr);
#endif
}

void CPakFile::InitialHeaderLoad() {
  CMemoryInStream in(x38_headerData.data(), x38_headerData.size());
  x30_dvdReq = rstl::auto_ptr< CDvdRequest >();

  const int version = in.ReadInt32();
#ifdef TARGET_PC
  // HOST DIAGNOSTIC: the version word, which is the byte-order item's whole claim. 0x30005 is
  // what retail tests for; anything else means `InitialHeaderLoad` returns below without
  // advancing the phase and this pak is stuck at 1 forever.
  printf("[pak] %s: InitialHeaderLoad, version=0x%08x (want 0x00030005), header %d bytes\n",
         x0_file.GetFilename().data(), static_cast< unsigned >(version), x38_headerData.size());
  fflush(nullptr);
#endif
  if (version != 0x30005) {
    char buf[248];
    sprintf(buf, lbl_803B0098, x0_file.GetFilename().data(), 0x30005, version);
    return;
  }

  in.ReadInt32();
  const int nameCount = in.ReadInt32();
  x58_nameList.reserve(nameCount);
  for (int i = 0; i < nameCount; ++i) {
    const FourCC type = in.ReadInt32();
    const CAssetId id = in.ReadInt32();
    const rstl::string name = CStringExtras::ReadString(in);
    x58_nameList.push_back_unsafe(
        rstl::pair< rstl::string, SObjectTag >(name, SObjectTag(type, id)));
  }

  x4c_resTableCount = in.ReadInt32();
  x48_resTableOffset = in.GetReadPosition();
  x2c_asyncLoadPhase = kAP_DataLoad;
#ifdef TARGET_PC
  printf("[pak] %s: header ok, nameList=%d resTableCount=%d at +%u, phase -> kAP_DataLoad\n",
         x0_file.GetFilename().data(), x58_nameList.size(), x4c_resTableCount,
         x48_resTableOffset);
  fflush(nullptr);
#endif

  const int oldSize = x38_headerData.size();
  const uint resourceBytes = x4c_resTableCount * 20;
  const int newSize = (resourceBytes + x48_resTableOffset + 31) & ~31;
  if (newSize > oldSize) {
    x38_headerData.resize(newSize);
    x30_dvdReq = rstl::auto_ptr< CDvdRequest >(x0_file.AsyncSeekRead(
        x38_headerData.data() + oldSize, x38_headerData.size() - oldSize, kSO_Set, oldSize));
  } else {
    DataLoad();
  }
}

void CPakFile::DataLoad() {
  x30_dvdReq = rstl::auto_ptr< CDvdRequest >();
  CMemoryInStream in(&x38_headerData[x48_resTableOffset],
                     x38_headerData.size() - x48_resTableOffset);
  LoadResourceTable(in);
  x2c_asyncLoadPhase = kAP_Loaded;
#ifdef TARGET_PC
  printf("[pak] %s: resource table loaded (%d entries), phase -> kAP_Loaded\n",
         x0_file.GetFilename().data(), x4c_resTableCount);
  fflush(nullptr);
#endif

  if (x28_worldPak) {
    const uint size = (x4c_resTableCount * sizeof(SResInfo) + 31) & ~31;
    x54_aramBase = CARAMManager::Alloc(size);
    const uint handle = CARAMManager::DMAToARAM(x78_resList.data(),
                                                 const_cast< void* >(x54_aramBase), size,
                                                 CARAMManager::kDMAPrio_One);
    CARAMManager::WaitForDMACompletion(handle);
  }

  x38_headerData = rstl::vector< uchar >();
  UpdateFakeStaticSize();
}

void CPakFile::LoadResourceTable(CMemoryInStream& in) {
  rstl::vector< SResInfo > sortedResources;
  sortedResources.reserve(x4c_resTableCount);
  if (x28_buildDepList)
    x68_depList.reserve(x4c_resTableCount);

  for (int i = 0; i < static_cast< int >(x4c_resTableCount); ++i) {
    const uint flags = in.ReadInt32();
    const uint type = in.ReadInt32();
    const uint id = in.ReadInt32();
    const uint size = in.ReadInt32();
    const uint offset = in.ReadInt32();
    sortedResources.push_back_unsafe(SResInfo(id, type, offset, size, flags, 0));
    if (x28_buildDepList)
      x68_depList.push_back_unsafe(id);
  }

  for (int i = 0; i < static_cast< int >(x4c_resTableCount); ++i) {
    SResInfo& info = sortedResources[i];
    if (info.GetSize() <= 8192) {
      uint groupedSize = 0;
      for (int j = i + 1; j < static_cast< int >(x4c_resTableCount); ++j) {
        const uint nextSize = sortedResources[j].GetSize();
        if (groupedSize + nextSize >= 8192)
          break;
        groupedSize += nextSize;
      }
      info.SetGroupedSize(groupedSize);
    }
  }

  static rstl::less< SResInfo > compare;
  rstl::sort(sortedResources.begin(), sortedResources.end(), compare);
  RebuildResourceLists(sortedResources);
}

const SObjectTag* CPakFile::GetResIdByName(const char* name) const {
  if (!x28_stashedInARAM) {
    for (rstl::vector< rstl::pair< rstl::string, SObjectTag > >::const_iterator it =
             x58_nameList.begin();
         it != x58_nameList.end(); ++it) {
      const int cmp = CStringExtras::CompareCaseInsensitive(it->first, rstl::string_l(name));
      if (cmp == 0)
        return &it->second;
    }
  }
  return nullptr;
}

const CPakFile::SResInfo* CPakFile::GetResInfo(uint id) const {
  if (!IsCompletelyLoaded())
    return nullptr;
  if (x28_stashedInARAM)
    return nullptr;
  const uint bucket = id & 0xff;
  rstl::vector< SResInfo >::const_iterator first =
      x78_resList.begin() + x88_bucketOffsets[bucket];
  rstl::vector< SResInfo >::const_iterator last =
      x78_resList.begin() + x88_bucketOffsets[bucket + 1];
  static rstl::less< SResInfo > compare;
  rstl::vector< SResInfo >::const_iterator it =
      rstl::lower_bound(first, last, SResInfo(id, 'TXTR', 0, 0, 0, 0), compare);
  if (it == last || it->GetId() != id)
    return nullptr;
  return &*it;
}

const CPakFile::SResInfo* CPakFile::GetResInfoForLoadDirectionless(uint id) {
  if (x28_stashedInARAM)
    return nullptr;
  const uint bucket = id & 0xff;
  rstl::vector< SResInfo >::const_iterator first =
      x78_resList.begin() + x88_bucketOffsets[bucket];
  rstl::vector< SResInfo >::const_iterator last =
      x78_resList.begin() + x88_bucketOffsets[bucket + 1];
  static rstl::less< SResInfo > compare;
  rstl::vector< SResInfo >::const_iterator it =
      rstl::lower_bound(first, last, SResInfo(id, 'TXTR', 0, 0, 0, 0), compare);
  if (it == last || it->GetId() != id)
    return nullptr;

  const SResInfo* best = &*it;
  int bestDelta = CMath::AbsI(static_cast< int >(it->GetOffset() - x94_currentSeek));
  ++it;
  while (it != last) {
    if (it->GetId() != id)
      break;
    const int delta = CMath::AbsI(static_cast< int >(it->GetOffset() - x94_currentSeek));
    if (delta < bestDelta) {
      best = &*it;
      bestDelta = delta;
    }
    ++it;
  }
  x94_currentSeek = best->GetOffset() + best->GetSize();
  return best;
}

const CPakFile::SResInfo* CPakFile::GetResInfoForLoadPreferForward(uint id) {
  if (x28_stashedInARAM)
    return nullptr;
  const uint bucket = id & 0xff;
  rstl::vector< SResInfo >::const_iterator first =
      x78_resList.begin() + x88_bucketOffsets[bucket];
  rstl::vector< SResInfo >::const_iterator last =
      x78_resList.begin() + x88_bucketOffsets[bucket + 1];
  static rstl::less< SResInfo > compare;
  rstl::vector< SResInfo >::const_iterator it =
      rstl::lower_bound(first, last, SResInfo(id, 'TXTR', 0, 0, 0, 0), compare);
  if (it == last || it->GetId() != id)
    return nullptr;

  const SResInfo* best = &*it;
  int bestDelta = x94_currentSeek - static_cast< int >(it->GetOffset());
  ++it;
  while (it != last) {
    if (it->GetId() != id)
      break;
    const int delta = x94_currentSeek - static_cast< int >(it->GetOffset());
    if ((bestDelta < 0 && (delta > 0 || delta > bestDelta)) ||
        (bestDelta >= 0 && delta > 0 && delta < bestDelta)) {
      best = &*it;
      bestDelta = delta;
    }
    ++it;
  }
  x94_currentSeek = best->GetOffset() + best->GetSize();
  return best;
}

void CPakFile::RebuildResourceLists(const rstl::vector< SResInfo >& sortedResources) {
  rstl::reserved_vector< uint, 256 > bucketCounts(0);

  // The copy is deliberate and must not be "simplified" away. Retail builds this `SResInfo`
  // once and then **copy**-constructs it before handing it to `resize` - one `bl
  // __ct__Q28CPakFile8SResInfoFUiUiUiUiUiUi` and then `lwz r0,8(r1) ; stw r0,20(r1) ; addi r3,r1,24 ;
  // addi r4,r1,12 ; li r5,7 ; bl __copy` at 0x80323270-0x80323284, with `resize` given r1+20.
  // Passing the named local by address produces no copy at all and this function reads 39.63%;
  // passing a bare prvalue elides the copy and reads 37.97%. Only the explicit copy is right.
  const SResInfo emptyInfo(0, 'TXTR', 0, 0, 0, 0);
  x78_resList.clear();
  x78_resList.resize(x4c_resTableCount, SResInfo(emptyInfo));
  x88_bucketOffsets.clear();
  x88_bucketOffsets.reserve(257);
  for (rstl::vector< SResInfo >::const_iterator it = sortedResources.begin();
       it != sortedResources.end(); ++it)
    ++bucketCounts[it->GetId() & 0xff];
  x88_bucketOffsets.push_back_unsafe(0);
  uint offset = 0;
  for (uint i = 0; i < 256; ++i) {
    offset += bucketCounts[i];
    x88_bucketOffsets.push_back_unsafe(offset);
    bucketCounts[i] = 0;
  }

  for (int i = 0; i < sortedResources.size(); ++i) {
    const SResInfo& info = sortedResources[i];
    const uint bucket = info.GetId() & 0xff;
    x78_resList[x88_bucketOffsets[bucket] + bucketCounts[bucket]] = info;
    ++bucketCounts[bucket];
  }
}

void CPakFile::EnsureWorldPakReady() {
  if (x28_worldPak && x28_stashedInARAM) {
    rstl::vector< SResInfo > resources(x4c_resTableCount);
    const uint size = (x4c_resTableCount * sizeof(SResInfo) + 31) & ~31;
    CARAMManager::WaitForDMACompletion(
        CARAMManager::DMAToMRAM(const_cast< void* >(x54_aramBase), resources.data(), size,
                                CARAMManager::kDMAPrio_One));
    RebuildResourceLists(resources);
    if (x28_buildDepList) {
      x68_depList.reserve(x4c_resTableCount);
      const SResInfo* info = resources.data();
      for (int i = 0; i < x4c_resTableCount; ++i, ++info)
        x68_depList.push_back_unsafe(info->GetId());
    }
    x28_stashedInARAM = false;
    UpdateFakeStaticSize();
  }
}

void CPakFile::sub_80323554() {
  if (x28_worldPak) {
    x28_stashedInARAM = true;
    x68_depList = rstl::vector< CAssetId >();
    x78_resList = rstl::vector< SResInfo >();
    x88_bucketOffsets = rstl::vector< uint >();
    UpdateFakeStaticSize();
  }
}
