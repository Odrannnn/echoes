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
#include <stdio.h>
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
#ifdef TARGET_PC
    printf("[aram] TryARAMFile(%s): CARAMManager::Alloc(%d) FAILED - this pak will not be "
           "staged, and IsARAMFileLoaded will answer true from the start.\n",
           mFilename.data(), mSize);
    fflush(nullptr);
#endif
    return;
  }
  mARAMFile = rs_new CDvdFileARAM();
  CDvdFileARAM* arfile = mARAMFile.get();
  arfile->mInfo.mDvdFile = this;
  arfile->mGotARAMInterrupt = true;
  arfile->mFileSize1 = arfile->mCurBufferLen = arfile->mBufferLen = GetFileSize();
  mARAMAllocated = true;
#ifdef TARGET_PC
  printf("[aram] TryARAMFile(%s): alloc %p ok, %d bytes; queued for staging.\n",
         mFilename.data(), static_cast< void* >(mARAMBuffer), mSize);
  fflush(nullptr);
#endif
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

#ifdef TARGET_PC
  // HOST DIAGNOSTIC. One line per *change* of the staging state, because this is polled from
  // the pak pump thousands of times and the interesting fact is that the two interrupt flags
  // never both come up (or come up and never drain). `mBufferLen`/`mCurBufferLen` are the
  // two counters: bytes still to push to ARAM and bytes still to read off the disc.
  {
    static const CDvdFile* sLast = nullptr;
    static int sLastKey = -1;
    const int key = (mARAMPopped ? 8 : 0) |
                    ((mARAMFile.get() != nullptr && mARAMFile->mGotARAMInterrupt) ? 4 : 0) |
                    ((mARAMFile.get() != nullptr && mARAMFile->mGotDvdInterrupt) ? 2 : 0) |
                    (mARAMFile.get() == nullptr ? 1 : 0);
    if (sLast != this || sLastKey != key) {
      sLast = this;
      sLastKey = key;
      CDvdFileARAM* ar = mARAMFile.get();
      printf("[aram] %s: popped=%d aramIRQ=%d dvdIRQ=%d bufferLen=%d curBufferLen=%d "
             "fileSize2=%u -> loaded=%d\n",
             mFilename.data(), static_cast< int >(mARAMPopped),
             ar != nullptr && ar->mGotARAMInterrupt, ar != nullptr && ar->mGotDvdInterrupt,
             ar != nullptr ? ar->mBufferLen : -1, ar != nullptr ? ar->mCurBufferLen : -1,
             ar != nullptr ? ar->mFileSize2 : 0u,
             (!mARAMPopped ? 0 : 1));
      fflush(nullptr);
    }
  }
#endif

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
#ifdef TARGET_PC
    const BOOL opened = DVDOpen(const_cast< char* >(DecodeARAMFile(mFilename.data())),
                                &aramFile->mInfo.mDvdFileInfo);
    printf("[aram] StartARAMFileLoad(%s): DVDOpen(\"%s\") -> %d, first read %d bytes at 0, "
           "buf0=%p align32=%d\n",
           mFilename.data(), DecodeARAMFile(mFilename.data()), static_cast< int >(opened), len,
           aramFile->mBuffers[0].get(),
           (reinterpret_cast< uintptr_t >(aramFile->mBuffers[0].get()) & 31) == 0);
    fflush(nullptr);
#else
    DVDOpen(const_cast< char* >(DecodeARAMFile(mFilename.data())),
            &aramFile->mInfo.mDvdFileInfo);
#endif
  }
  DVDReadAsync(&aramFile->mInfo.mDvdFileInfo, aramFile->mBuffers[0].get(), len, 0,
               DVDARAMXferCallback);
  lbl_80419B9D = true;
#ifdef TARGET_PC
  printf("[aram] StartARAMFileLoad(%s): DVDReadAsync issued, cb.state=%d userData=%p\n",
         mFilename.data(), static_cast< int >(aramFile->mInfo.mDvdFileInfo.cb.state),
         aramFile->mInfo.mDvdFileInfo.cb.userData);
  fflush(nullptr);
#endif
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
#ifdef TARGET_PC
    printf("[dvd] AsyncSeekRead(%s): ARAM path, len=%u off=%d -> CARAMDvdRequest %p\n",
           mFilename.data(), len, mOffset, static_cast< void* >(request));
    fflush(nullptr);
#endif
  } else {
    CRealDvdRequest* req = rs_new CRealDvdRequest();
    DVDFileInfo* info = &req->FileInfo();
#ifdef TARGET_PC
    // HOST DIAGNOSTIC. Whether `DVDFastOpen` filled the command block is the whole question
    // this item was queued on: it returns FALSE **without touching `fileInfo`** when the
    // entrynum is not valid, so a failed open leaves `cb.state` as whatever `rs_new`'s raw
    // memory held and `IsComplete` then polls a block that was never written. The buffer's
    // 32-byte alignment is printed alongside it because Aurora asserts it in
    // `DVDReadAbsAsyncPrioInternal` (dvd.cpp:743).
    const BOOL fastOpen = DVDFastOpen(mFileEntry, info);
    printf("[dvd] AsyncSeekRead(%s): DVD path, entry=%d len=%u off=%d roundLen=%u "
           "fastOpen=%d buf=%p align32=%d\n",
           mFilename.data(), mFileEntry, len, mOffset, (len + 31) & ~31,
           static_cast< int >(fastOpen), dest,
           (reinterpret_cast< uintptr_t >(dest) & 31) == 0);
    fflush(nullptr);
#else
    DVDFastOpen(mFileEntry, info);
#endif
    DVDReadAsync(info, dest, (len + 31) & ~31, mOffset, internalCallback);
    lbl_80419B9D = true;
    request = req;
#ifdef TARGET_PC
    printf("[dvd] AsyncSeekRead(%s): issued; cb.state=%d userData=%p\n", mFilename.data(),
           static_cast< int >(info->cb.state), info->cb.userData);
    fflush(nullptr);
#endif
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

// ---------------------------------------------------------------------------
// HOST: what `FileExists` answers, and from what
// ---------------------------------------------------------------------------
//
// **The FST this consults on a host is Nod's, built from the real disc - and there is
// deliberately no host `__fstLoad`, because there is no code for one to be called from.** That
// is measured, not assumed, and it corrects a claim that was in this tree's docs:
//
//   * `src/Dolphin/dvd/{dvd,dvdfs,fstload}.c` (and the rest of `src/Dolphin/`) are **not in the
//     port build**. `files.cmake` names seven `src/Dolphin/Carve*.c` decompilation units and
//     nothing else from that directory, with the reason in the file: "src/Dolphin/*.c are
//     configured decompilation units that files.cmake does not name, and that is correct rather
//     than an oversight: all four are GameCube register shims written as assembly-in-C (u32
//     typedefs, inline PPC asm, MMIO pokes) and none of them compiles for an x86-64 host - 15
//     errors. On the host platform/ai_dma.cpp and platform/shims.cpp replace them." The port
//     links `aurora::dvd` (`CMakeLists.txt:238`).
//   * So `DVDInit` in the port executable is **Aurora's empty `void DVDInit(void) {}`**
//     (`extern/aurora/lib/dolphin/dvd/dvd.cpp:717`), and `DVDConvertPathToEntrynum` - the one
//     function `FileExists` calls - is **Aurora's** (`dvd.cpp:995`). Confirmed on the linked
//     binary, not inferred: `addr2line -f -C -e build-port-link/metroid_prime2_port <DVDInit>`
//     answers `extern/aurora/lib/dolphin/dvd/dvd.cpp:717`, the same for
//     `DVDConvertPathToEntrynum` answers `dvd.cpp:995`, and `nm` finds **neither `__fstLoad` nor
//     `__DVDFSInit` in the binary at all**.
//   * The `__fstLoad` chain those files implement - `__DVDFSInit` taking `FstStart` from
//     `BootInfo->FSTLocation`, which only `__fstLoad` writes, reached from `DVDInit` only when
//     `bootInfo->magic == 0xE5207C22` - is the **console's** route, and it is unreachable on a
//     host twice over: the code is not compiled, and a host process has no retail boot-info magic.
//     Writing a host `__fstLoad` in `src/Dolphin/dvd/fstload.c` would therefore be **dead code**,
//     and it would be a *second* FST reader for a disc the port has already opened.
//
// **So the answer to "should a host `__fstLoad` read the ISO's FST, or bypass it?" is: the
// existing path already reads the ISO's FST, and nothing needs to bypass anything.**
// `platform/main.cpp:126`'s `aurora_dvd_open($MP2_DISC)` does
// `nod_disc_open_stream` -> `nod_disc_open_partition_kind(NOD_PARTITION_KIND_DATA)` ->
// `rebuildFST()` -> `nod_partition_iterate_fst` (`extern/aurora/lib/dolphin/dvd/fst.cpp:268`),
// and `aurora_dvd_open` returns false if any of that fails, so **a FST that answered at all is a
// FST derived from the bytes of the ISO the user pointed `MP2_DISC` at.** A bypass "because a host
// has one disc and no FST lookup is needed" would also be the wrong shape: `AddPakFileAsync`
// appends `".pak"` to a name and asks, `CMain::AddWorldPaks` asks for sixteen `<base>N.pak`
// names, and every one of those questions needs a real answer from real disc contents.
//
// **And the disc really is being asked, which is the point of the print below.** `Strings.pak` -
// `AddPaksAndFactories`' first probe (`src/MetroidPrime/mainMid.cpp:371`) - is **not on the MP2
// disc**: `tools/extract_disc_file.py <iso> -l` lists twenty `.pak` files and none of them is
// `Strings.pak`, because retail keeps that pak's contents in ARAM under a file named after it.
// So `FileExists("Strings.pak")` answers **false**, on a real disc, and that is retail's own
// behaviour: the `if` around the `aram:Strings` add is retail's gate, not the port's.
//
// The print is diagnostic and announces itself. It exists because "the port never loads a pak"
// was believed for a session on the strength of a code path that is not in the binary, and the
// cheapest way to keep that from happening again is for the boot log to say, per call, what the
// real disc answered. mwcceppc does not define `TARGET_PC`, so the matching build is unchanged
// and the unit's per-function scores are identical before and after.
#ifdef TARGET_PC
#include <stdio.h>
bool CDvdFile::FileExists(const char* filename) {
  const char* const decodedName = DecodeARAMFile(filename);
  const s32 entry = DVDConvertPathToEntrynum(const_cast< char* >(decodedName));
  const bool exists = entry != -1;
  // The size, because "exists" alone cannot distinguish a pak from a directory or a truncated
  // read, and `DVDFastOpen` is what actually hands the entry to the reader. Printed only on a
  // hit: a miss has no size and the name alone is the whole answer.
  u32 length = 0;
  if (exists) {
    DVDFileInfo info;
    if (DVDFastOpen(entry, &info)) {
      length = info.length;
      DVDClose(&info);
    }
  }
  if (exists) {
    printf("[dvd] FileExists(\"%s\") -> true, %u bytes (entry %d)\n", filename, length, entry);
    if (length == 0) {
      printf("[dvd]   ^ non-zero entry but zero length: the FST entry resolved to nothing "
             "readable, which is a disc problem, not a lookup problem.\n");
    }
  } else {
    printf("[dvd] FileExists(\"%s\") -> false\n", filename);
  }
  return exists;
}
#else
bool CDvdFile::FileExists(const char* filename) {
  return DVDConvertPathToEntrynum(const_cast< char* >(DecodeARAMFile(filename))) != -1;
}
#endif

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
