#include "MetroidPrime/CRelFile.hpp"

#include "Kyoto/Alloc/CMemory.hpp"
#include "Kyoto/Basics/CBasics.hpp"
#include "Kyoto/CDvdFile.hpp"
#include "Kyoto/CDvdRequest.hpp"

#include "dolphin/os.h"
#include "dolphin/os/OSModule.h"

#ifdef TARGET_PC
extern "C" void port_modules_prolog(const char* path);
extern "C" void port_modules_epilog(const char* path);

static uint PortReadBE32(const uchar* p) {
  return (uint(p[0]) << 24) | (uint(p[1]) << 16) | (uint(p[2]) << 8) | uint(p[3]);
}

// Where the REL file format keeps `OSModuleHeader::bssSize` and `fixSize`.
enum { kRelHeaderBssSize = 0x20, kRelHeaderFixSize = 0x48 };
#endif

CRelFile::CRelFile(const rstl::string& name)
: mName(name)
, mDvdRequest(nullptr)
, mData(nullptr)
, mDataSize(0)
, mModule(nullptr)
, mReferenceCount(0)
, mLoadRequestCount(0)
, mState(kS_Unloaded) {}

CRelFile::~CRelFile() {
  if (mState != kS_Unloaded) {
    Update();
  }
}

void CRelFile::AddReference() { ++mReferenceCount; }

short CRelFile::RemoveReference() {
  --mReferenceCount;
  return mReferenceCount;
}

void CRelFile::FreeData() {
  CMemory::OffsetFakeStatics(-mDataSize);
  mDvdRequest = nullptr;
  mData = nullptr;
  mDataSize = 0;
  mModule = nullptr;
}

bool CRelFile::StartLoad() {
  if (!CDvdFile::FileExists(mName.data())) {
    return false;
  }
  CDvdFile file(mName.data());
  mDataSize = (file.Length() + 31) & ~31;
  mData = static_cast< uchar* >(CMemory::Alloc(mDataSize, IAllocator::kHI_RoundUpLen));
  mModule = reinterpret_cast< OSModuleHeader* >(mData.get());
  CMemory::OffsetFakeStatics(mDataSize);
  mDvdRequest = file.SyncRead(mModule, mDataSize);
  return true;
}

void CRelFile::Update() {
  switch (mState) {
  case kS_Cancelling:
    if (!mDvdRequest->IsComplete()) {
      break;
    }
    FreeData();
    mState = kS_Unloaded;
  case kS_Unloaded:
    if (mLoadRequestCount >= 1) {
      if (StartLoad()) {
        mState = kS_Loading;
      } else {
        mState = kS_Loaded;
      }
    }
    break;
  case kS_Loading:
    if (mDvdRequest->IsComplete()) {
      mState = kS_Loaded;
      Link();
    } else if (mLoadRequestCount == 0) {
      mDvdRequest->PostCancelRequest();
      mState = kS_Cancelling;
    }
    break;
  case kS_Loaded:
    if (mLoadRequestCount == 0) {
      Unlink();
      mState = kS_Unloaded;
    }
    break;
  }
}

void CRelFile::AddLoadRequest() { ++mLoadRequestCount; }

void CRelFile::RemoveLoadRequest() { --mLoadRequestCount; }

void CRelFile::Unlink() {
  mDebugInfo.Unregister();
  if (mModule != nullptr) {
#ifdef TARGET_PC
    // The image is PowerPC code the host cannot run: the module's host shutdown stands in for
    // the epilog and `OSUnlink` (platform/compiled_modules.cpp).
    port_modules_epilog(mName.data());
#else
    reinterpret_cast< void (*)() >(mModule->epilog)();
    OSUnlink(&mModule->info);
#endif
  }
  FreeData();
}

void CRelFile::Link() {
  OSGetTime();
#ifdef TARGET_PC
  // The image is the disc's bytes, so its header is big-endian on every host, and its fields sit
  // at the REL format's offsets rather than at the host `OSModuleHeader`'s (which holds pointers).
  const uchar* image = reinterpret_cast< const uchar* >(mModule);
  uint bssSize = PortReadBE32(&image[kRelHeaderBssSize]);
  uint fixSize = (PortReadBE32(&image[kRelHeaderFixSize]) + 31) & ~31;
#else
  uint bssSize = mModule->bssSize;
  uint fixSize = (mModule->fixSize + 31) & ~31;
#endif
  if (mDataSize - fixSize < bssSize) {
    uint newSize = ((bssSize + 31) & ~31) + fixSize;
    rstl::single_ptr< uchar > newData(
        static_cast< uchar* >(CMemory::Alloc(newSize, IAllocator::kHI_RoundUpLen)));
    CBasics::CopyMemory(newData.get(), mData.get(), mDataSize);
    mDataSize = newSize;
    mData = newData;
    mModule = reinterpret_cast< OSModuleHeader* >(mData.get());
  }
#ifdef TARGET_PC
  // No `OSLinkFixed` and no prolog `bctrl`: the module's host init stands in for both, and a
  // module that is not compiled in is a declared stop there (platform/compiled_modules.cpp).
  mDebugInfo.Register(mName.data(), mData.get(), mDataSize);
  port_modules_prolog(mName.data());
#else
  OSLinkFixed(&mModule->info, reinterpret_cast< uchar* >(mModule) + fixSize);
  mDebugInfo.Register(mName.data(), mData.get(), mDataSize);
  reinterpret_cast< void (*)() >(mModule->prolog)();
#endif
  OSGetTime();
}

bool CRelFile::IsLoaded() const { return mState == kS_Loaded && mLoadRequestCount > 0; }

bool CRelFile::IsDeletable() const { return mState == kS_Unloaded && mReferenceCount == 0; }
