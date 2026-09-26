#ifndef _CPAKFILE
#define _CPAKFILE

#include "types.h"

#include "rstl/auto_ptr.hpp"
#include "rstl/string.hpp"
#include "rstl/vector.hpp"
#include "rstl/pair.hpp"

#include "Kyoto/CDvdFile.hpp"
#include "Kyoto/IObjectStore.hpp"

class CDvdRequest;
class CMemoryInStream;

class CPakFile {
public:
  enum EAsyncPhase { kAP_Warmup, kAP_InitialHeaderLoad, kAP_DataLoad, kAP_Loaded };

#pragma pack(push, 1)
  struct SResInfo {
    CAssetId x0_id;
    uchar x4_data[7];

    SResInfo(uint id, uint fourCC, uint offset, uint size, uint flags, uint groupedSize);
    uint GetType() const;
    uint GetOffset() const;
    uint GetSize() const;
    bool IsCompressed() const;
    void SetGroupedSize(uint size);
    uint GetGroupedSize() const;
    CAssetId GetId() const { return x0_id; }
    bool operator<(const SResInfo& other) const { return x0_id < other.x0_id; }
  };
#pragma pack(pop)

  CPakFile(const rstl::string& filename, bool buildDepList, bool worldPak);
  ~CPakFile();

  CDvdFile& DvdFile() { return x0_file; }
  const CDvdFile& GetDvdFile() const { return x0_file; }
  void AsyncIdle();
  bool IsWorldPak() const { return x28_worldPak; }
  // The cached copy of `DvdFile().IsARAMFile()`, which the constructor fills from
  // `CDvdFile`'s `mARAMAllocated`. **`CResLoader::AsyncIdlePakLoading` (0x802FCCE4's loop,
  // 0x802FCCF4) reads bit field 25 of the flag byte, which is this, and not `IsWorldPak()`
  // (field 26)** - `rlwinm. r31,r0,26,31,31` at 0x802fcd1c against `rlwimi r0,r4,6,25,25` in
  // the constructor at 0x803245d0. Reading the CDvdFile member instead compiles to
  // `lbz`/`cmplwi` and loses the rotation.
  bool IsARAMFile() const { return x28_aramFile; }
  bool IsCompletelyLoaded() const { return x2c_asyncLoadPhase == kAP_Loaded; }
  void EnsureWorldPakReady();
  void sub_80323554();

  rstl::vector< rstl::pair< rstl::string, SObjectTag > >& NameList() { return x58_nameList; }
  const SObjectTag* GetResIdByName(const char* name) const;
  const SResInfo* GetResInfo(uint id) const;
  const SResInfo* GetResInfoForLoadDirectionless(uint id);
  const SResInfo* GetResInfoForLoadPreferForward(uint id);
  uint GetFakeStaticSize() const;

private:
  void UpdateFakeStaticSize();
  void RebuildResourceLists(const rstl::vector< SResInfo >& sortedResources);
  void LoadResourceTable(CMemoryInStream& in);
  void Warmup();
  void InitialHeaderLoad();
  void DataLoad();

  CDvdFile x0_file;
  bool x28_buildDepList : 1;
  bool x28_aramFile : 1;
  bool x28_worldPak : 1;
  bool x28_stashedInARAM : 1;
  EAsyncPhase x2c_asyncLoadPhase;
  rstl::auto_ptr< CDvdRequest > x30_dvdReq;
  rstl::vector< uchar > x38_headerData;
  uint x48_resTableOffset;
  uint x4c_resTableCount;
  int x50_fakeStaticSize;
  const void* x54_aramBase;
  rstl::vector< rstl::pair< rstl::string, SObjectTag > > x58_nameList;
  rstl::vector< CAssetId > x68_depList;
  rstl::vector< SResInfo > x78_resList;
  rstl::vector< uint > x88_bucketOffsets;
  mutable int x94_currentSeek;
};
CHECK_SIZEOF(CPakFile, 0x9c)
NESTED_CHECK_SIZEOF(CPakFile, SResInfo, 0xb)

namespace rstl {
template <>
struct is_trivially_destructible< CPakFile::SResInfo > {
  enum { value = true };
};

template <>
inline void construct< CPakFile::SResInfo >(void* dest, const CPakFile::SResInfo& src) {
  *static_cast< CPakFile::SResInfo* >(dest) = src;
}
} // namespace rstl

#endif // _CPAKFILE
