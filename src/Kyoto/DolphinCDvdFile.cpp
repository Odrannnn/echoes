#include "Kyoto/CDvdFile.hpp"
#include "Kyoto/CDvdRequest.hpp"

#include "Kyoto/CARAMManager.hpp"

#include "rstl/math.hpp"

#include "dolphin/os/OSCache.h"

#include "dolphin/arq.h"
#include "dolphin/dvd.h"
#include "dolphin/os.h"

#include "string.h"

#ifdef TARGET_PC
#include <new>
#endif

static CDvdFile* sFirstARAM = nullptr;
// The original names of these two mode/activity flags are not yet known. Retail defines them
// under C linkage, and CCubeMoviePlayer.cpp declares them extern "C".
//
// `lbl_80419B9C` is a **polling-mode switch**, and every ARAM routine below honours it. When it is
// set, the DVD and ARQ completion callbacks only record that they fired (`HandleDVDInterrupt`,
// `HandleARAMInterrupt`), `DVDARAMXferCallback` does not close the file, and it is the caller of
// `IsARAMFileLoaded` - the pak pump, on the game thread - that issues the next 64 KB transfer.
// Retail never sets it (the DOL has loads of it and no store), so on the cube the chain runs from
// the interrupts.
//
// **The host sets it, because on the host the "interrupts" are other threads.** Aurora runs DVD
// completion callbacks on its DVD worker thread and defers ARQ callbacks to whoever calls
// `ARQPoll()`, and `OSDisableInterrupts` is a no-op (platform/sdk_stubs.cpp). In interrupt mode
// that means `PingARAMTransfer` can run on the worker and the game thread at once - both see both
// flags set and both transfer the same buffer - and that `DVDARAMXferCallback` calls `DVDClose`
// from inside the worker's own callback, which drains the command the worker is executing. Polling
// mode is retail's own answer to "the transfers are not driven by interrupts", and in it only the
// game thread touches the transfer state; the worker writes one `bool`.
#ifdef TARGET_PC
extern "C" bool lbl_80419B9C = true;
#else
extern "C" bool lbl_80419B9C = false;
#endif
extern "C" bool lbl_80419B9D = false;

struct CDvdFileARAM {
  CDvdFileARAM()
  : mGotARAMInterrupt(false)
  , mGotDvdInterrupt(false)
  , mFileSize1(0)
  , mFileSize2(0)
  , mCurBufferLen(0)
  , mAramOffset(0)
  , mBufferLen(0)
  , mBufferIndex(0) {}
  ~CDvdFileARAM();

  ARQRequest mARQRequest;
  struct SDvdInfo {
    SDvdInfo() : mDvdFile(nullptr), mNextfile(nullptr) {}
    DVDFileInfo mDvdFileInfo;
    CDvdFile* mDvdFile;
    CDvdFile* mNextfile;
  } mInfo;
  rstl::reserved_vector< rstl::auto_ptr< uchar >, 2 > mBuffers;
  bool mGotARAMInterrupt;
  bool mGotDvdInterrupt;
  uint mFileSize1;
  uint mFileSize2;
  int mCurBufferLen;
  uint mAramOffset;
  int mBufferLen;
  uint mBufferIndex;
};

CHECK_SIZEOF(CDvdFileARAM, 0x94)

const char* DecodeARAMFile(const char* filename) {
  if (!strncmp(filename, "aram:", 5)) {
    return filename + 5;
  }

  return filename;
}

void CDvdFile::DVDARAMXferCallback(s32 result, DVDFileInfo* info) {
  CDvdFileARAM::SDvdInfo* ptr = reinterpret_cast< CDvdFileARAM::SDvdInfo* >(info);
  if (!lbl_80419B9C) {
    DVDClose(&ptr->mDvdFileInfo);
  }
  ptr->mDvdFile->HandleDVDInterrupt();
}

void CDvdFile::ARAMARAMXferCallback(u32 addr) {
  reinterpret_cast< CDvdFileARAM* >(addr)->mInfo.mDvdFile->HandleARAMInterrupt();
}

#ifdef TARGET_PC
namespace {
// Port: the console stores ARQ payloads in a u32 address; Aurora carries a host
// pointer. The adapter keeps the console signature (and therefore the matching
// build) intact while the port passes a real pointer.
//
// It does `ARAMARAMXferCallback`'s one line itself rather than forwarding to it. It used to
// forward with `static_cast< u32 >(addr)`, which cut the `CDvdFileARAM*` - a 64-bit heap
// pointer - to its low 32 bits, and the callback then dereferenced that. Nothing reached it
// while `CARAMManager::Alloc` was a reach stub and ARAM was never initialised.
void PortARAMARAMXferCallback(uintptr_t addr) {
  reinterpret_cast< CDvdFileARAM* >(addr)->mInfo.mDvdFile->HandleARAMInterrupt();
}
} // namespace
#define ARAMARAM_XFER_CALLBACK PortARAMARAMXferCallback
#else
#define ARAMARAM_XFER_CALLBACK ARAMARAMXferCallback
#endif

void CDvdFile::HandleARAMInterrupt() {
  BOOL enabled = OSDisableInterrupts();
  CDvdFileARAM* arFile = mARAMFile.get();

  arFile->mGotARAMInterrupt = true;

  if (!lbl_80419B9C && arFile->mGotARAMInterrupt && arFile->mGotDvdInterrupt) {
    PingARAMTransfer();
  }

  OSRestoreInterrupts(enabled);
}

void CDvdFile::HandleDVDInterrupt() {
  BOOL enabled = OSDisableInterrupts();
  CDvdFileARAM* arFile = mARAMFile.get();

  arFile->mGotDvdInterrupt = true;

  if (!lbl_80419B9C && arFile->mGotARAMInterrupt && arFile->mGotDvdInterrupt) {
    PingARAMTransfer();
  }

  OSRestoreInterrupts(enabled);
}

void CDvdFile::PingARAMTransfer() {
  CDvdFileARAM* aramFile = mARAMFile.get();

  if (aramFile->mBufferLen == 0) {
    PopARAMFileLoad();
    return;
  }

  int length = rstl::min_val(65536, aramFile->mBufferLen);
#ifdef TARGET_PC
  // Port: Aurora's ARQ takes pointer-width addresses, so hand it the host
  // pointers rather than the console's 32-bit encoding.
  ARQPostRequest(&aramFile->mARQRequest, 0, ARQ_TYPE_MRAM_TO_ARAM, ARQ_PRIORITY_HIGH,
                 reinterpret_cast< uintptr_t >(aramFile->mBuffers[aramFile->mBufferIndex].get()),
                 reinterpret_cast< uintptr_t >(mARAMBuffer + aramFile->mAramOffset), length,
                 ARAMARAM_XFER_CALLBACK);
#else
  ARQPostRequest(&aramFile->mARQRequest, 0, ARQ_TYPE_MRAM_TO_ARAM, ARQ_PRIORITY_HIGH,
                 reinterpret_cast< u32 >(aramFile->mBuffers[aramFile->mBufferIndex].get()),
                 reinterpret_cast< u32 >(mARAMBuffer + aramFile->mAramOffset), length,
                 ARAMARAM_XFER_CALLBACK);
#endif

  aramFile->mBufferLen -= length;
  aramFile->mAramOffset += length;
  aramFile->mGotARAMInterrupt = false;
  aramFile->mBufferIndex ^= 1;

  if (aramFile->mCurBufferLen != 0) {
    int length2 = rstl::min_val(65536, aramFile->mCurBufferLen);
    aramFile->mGotDvdInterrupt = false;
    DVDFastOpen(mFileEntry, &aramFile->mInfo.mDvdFileInfo);
    DVDReadAsync(&aramFile->mInfo.mDvdFileInfo, aramFile->mBuffers[aramFile->mBufferIndex].get(),
                 length2, aramFile->mFileSize2, DVDARAMXferCallback);
    lbl_80419B9D = true;
    aramFile->mFileSize2 += length2;
    aramFile->mCurBufferLen -= length2;
  }
}

void CDvdFile::TryARAMFile() {
  mARAMBuffer = static_cast< uchar* >(CARAMManager::Alloc(mSize));
  if (!CARAMManager::IsAllocValid(mARAMBuffer)) {
    return;
  }
  mARAMFile = rs_new CDvdFileARAM();
  CDvdFileARAM* arfile = mARAMFile.get();
  arfile->mInfo.mDvdFile = this;
  arfile->mGotARAMInterrupt = true;
  arfile->mFileSize1 = arfile->mCurBufferLen = arfile->mBufferLen = GetFileSize();
  mARAMAllocated = true;
  PushARAMFileLoad();
}

void CDvdFile::PushARAMFileLoad() {
  BOOL enabled = true;
  if (!lbl_80419B9C) {
    enabled = OSDisableInterrupts();
  }
  CDvdFile* file = sFirstARAM;
  if (file == NULL) {
    sFirstARAM = this;
    StartARAMFileLoad();
  } else {
    for (CDvdFile* p = file; p != nullptr; p = p->mARAMFile->mInfo.mNextfile) {
      if (p->mARAMFile->mInfo.mNextfile == nullptr) {
        p->mARAMFile->mInfo.mNextfile = this;
        break;
      }
    }
  }
  if (!lbl_80419B9C) {
    OSRestoreInterrupts(enabled);
  }
}

void CDvdFile::PopARAMFileLoad() {
  BOOL enabled = true;
  if (!lbl_80419B9C) {
    enabled = OSDisableInterrupts();
  }
  CDvdFile* file = mARAMFile->mInfo.mNextfile;
  mARAMPopped = true;
  sFirstARAM = file;
  if (file != nullptr) {
    file->StartARAMFileLoad();
  }

  if (!lbl_80419B9C) {
    OSRestoreInterrupts(enabled);
  }
}

bool CDvdFile::IsARAMFileLoaded() {
  if (!mARAMAllocated) {
    return true;
  }

  if (!mARAMPopped) {
#ifdef TARGET_PC
    // Aurora delivers ARQ completions only from `ARQPoll()`; this is where polling mode waits
    // for them, so this is where they are delivered.
    ARQPoll();
    if (lbl_80419B9C && mARAMFile->mGotARAMInterrupt && mARAMFile->mGotDvdInterrupt) {
      // Polling mode skips `DVDARAMXferCallback`'s `DVDClose`, and `PingARAMTransfer`'s
      // `DVDFastOpen` then zeroes the file info - which on Aurora drops the open handle on the
      // floor, one per 64 KB. The read has completed (that is what the flag says), so close it
      // here, on this thread. Closing an already-closed info is a no-op.
      DVDClose(&mARAMFile->mInfo.mDvdFileInfo);
      PingARAMTransfer();
    }
#else
    if (lbl_80419B9C && mARAMFile->mGotARAMInterrupt && mARAMFile->mGotDvdInterrupt) {
      PingARAMTransfer();
    }
#endif
    return false;
  }

  mARAMFile = nullptr;

  return true;
}

void CDvdFile::StartARAMFileLoad() {
  CDvdFileARAM* aramFile = mARAMFile.get();
#ifdef TARGET_PC
  // Port: the two buffers are owned by `auto_ptr< uchar >`, whose destructor is `delete`. Under
  // mwcceppc `delete` is `CMemory::Free` (Kyoto/Alloc/CMemory.hpp), so retail's `CMemory::Alloc`
  // pairs with it; on the host `delete` is the C++ runtime's, and handing it a game-heap block
  // aborts in glibc (`munmap_chunk(): invalid pointer`) the moment the first `aram:` pak finishes
  // staging and `IsARAMFileLoaded` drops its `CDvdFileARAM`. So allocate them from the runtime.
  // 32-byte alignment is retail's (the game heap's) and Aurora's `DVDReadAsync` asserts it; the
  // aligned global `operator new` is `aligned_alloc` on glibc, which `delete` frees correctly.
  aramFile->mBuffers.push_back(static_cast< uchar* >(::operator new(0x10000, std::align_val_t(32))));
  aramFile->mBuffers.push_back(static_cast< uchar* >(::operator new(0x10000, std::align_val_t(32))));
#else
  aramFile->mBuffers.push_back(
      static_cast< uchar* >(CMemory::Alloc(0x10000, IAllocator::kHI_RoundUpLen)));
  aramFile->mBuffers.push_back(
      static_cast< uchar* >(CMemory::Alloc(0x10000, IAllocator::kHI_RoundUpLen)));
#endif

  int len = rstl::min_val(mSize, 65536);
  aramFile->mCurBufferLen -= len;
  aramFile->mFileSize2 = len;
  if (!lbl_80419B9C) {
    DVDFastOpen(mFileEntry, &aramFile->mInfo.mDvdFileInfo);
  } else {
    DVDOpen(const_cast< char* >(DecodeARAMFile(mFilename.data())),
            &aramFile->mInfo.mDvdFileInfo);
  }
  DVDReadAsync(&aramFile->mInfo.mDvdFileInfo, aramFile->mBuffers[0].get(), len, 0,
               DVDARAMXferCallback);
  lbl_80419B9D = true;
}

void CDvdFile::StallForARAMFile() {
  while (mARAMFile.get() != nullptr) {
    OSYieldThread();
  }
}

CDvdFile::CDvdFile(const char* filename)
: mFileEntry(-1)
, mARAMBuffer(0)
, mARAMAllocated(false)
, mARAMPopped(false)
, mARAMFile(nullptr)
, mOffset(0)
, mSize(0)
, mFilename(filename, -1) {
  const char* decodedName = DecodeARAMFile(filename);
  mFileEntry = DVDConvertPathToEntrynum(const_cast< char* >(decodedName));
  DVDFileInfo fileInfo;
  if (mFileEntry != -1) {
    DVDFastOpen(mFileEntry, &fileInfo);
  }

  mSize = fileInfo.length;
  DVDClose(&fileInfo);

  if (filename != decodedName) {
    TryARAMFile();
  }
}

CDvdFileARAM::~CDvdFileARAM() {}

CDvdFile::~CDvdFile() { CloseFile(); }

CDvdRequest* CDvdFile::SyncRead(void* dest, uint len) {
  return AsyncSeekRead(dest, len, kSO_Cur, 0);
}

void CDvdFile::SyncSeekRead(void* dest, uint len, ESeekOrigin origin, int offset) {
  StallForARAMFile();
  CalcFileOffset(offset, origin);

  if (mARAMAllocated) {
    uint roundedLen = (len + 31) & ~31;
    DCFlushRange(dest, roundedLen);
    CARAMManager::WaitForDMACompletion(CARAMManager::DMAToMRAM(
        mARAMBuffer + mOffset, dest, roundedLen, CARAMManager::kDMAPrio_One));
  } else {
    DVDFileInfo info;
    if (!lbl_80419B9C) {
      DVDFastOpen(mFileEntry, &info);
    } else {
      DVDOpen(const_cast< char* >(DecodeARAMFile(mFilename.data())), &info);
    }
    DVDReadAsync(&info, dest, (len + 31) & ~31, mOffset, internalCallback);
    lbl_80419B9D = true;
    while (DVDGetCommandBlockStatus(&info.cb) != DVD_STATE_END) {
    }
    DVDClose(&info);
  }

  UpdateFilePos(len);
}

CDvdRequest* CDvdFile::AsyncSeekRead(void* dest, uint len, ESeekOrigin origin, int offset) {
  StallForARAMFile();
  CalcFileOffset(offset, origin);
  CDvdRequest* request;
  if (mARAMAllocated) {
    const int roundedLen = (len + 31) & ~31;
    DCFlushRange(dest, roundedLen);
    request = rs_new CARAMDvdRequest(CARAMManager::DMAToMRAM(
        mARAMBuffer + mOffset, dest, roundedLen, CARAMManager::kDMAPrio_One));
  } else {
    CRealDvdRequest* req = rs_new CRealDvdRequest();
    DVDFileInfo* info = &req->FileInfo();
    DVDFastOpen(mFileEntry, info);
    DVDReadAsync(info, dest, (len + 31) & ~31, mOffset, internalCallback);
    lbl_80419B9D = true;
    request = req;
  }

  UpdateFilePos(len);

  return request;
}

void CDvdFile::CloseFile() {
  if (!mARAMAllocated) {
    return;
  }

  StallForARAMFile();
  CARAMManager::Free(mARAMBuffer);
}

bool CDvdFile::FileExists(const char* filename) {
  return DVDConvertPathToEntrynum(const_cast< char* >(DecodeARAMFile(filename))) != -1;
}

void CDvdFile::internalCallback(s32 res, DVDFileInfo* info) {
  if (res != DVD_STATE_CANCELED) {
    DCInvalidateRange(info->cb.addr, info->cb.length);
    lbl_80419B9D = true;
  }
}

void CDvdFile::CalcFileOffset(int offset, ESeekOrigin origin) {
  switch (origin) {
  case kSO_Set:
    mOffset = offset;
    break;
  case kSO_Cur:
    mOffset += offset;
    break;
  case kSO_End:
    mOffset = offset + mSize;
    break;
  }
}

void CDvdFile::UpdateFilePos(int pos) {
  mOffset += (pos + 31) & ~31;
  int filesize = GetFileSize();
  if (mOffset > filesize) {
    mOffset = filesize;
  }
}

rstl::single_ptr< CDvdFileARAM >::~single_ptr() { delete x0_ptr; }

rstl::single_ptr< CDvdFileARAM >& rstl::single_ptr< CDvdFileARAM >::operator=(CDvdFileARAM* ptr) {
  delete x0_ptr;
  x0_ptr = ptr;
  return *this;
}
