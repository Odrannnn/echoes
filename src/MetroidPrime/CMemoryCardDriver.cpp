// Retail's `operator new` placement string for this TU is `lbl_803A9A94 + 459`
// (`.rodata:0x803A9C5F`, `symbols.txt:17212`'s 0x1EC-byte pool) - the six bytes `??(??)`, the
// same literal `rs_new` spells. **Declared, never defined**, for the reason
// `src/MetroidPrime/Factories/CStateMachineFactory.cpp` gives: a literal of our own is routed
// through mwcceppc's per-TU `@stringBase0` pool and makes this object emit a `.rodata` section,
// which the linker appends to the global pool and which shifts every later entry. Retail's copy
// is in `auto_06_803A9A58_rodata.o`, which precedes this object, so naming it resolves as-is.
// `CMEMORY_NEW_FILE` must be set before any include - see `Kyoto/Alloc/CMemory.hpp`.
extern "C" const char lbl_803A9A94[];
#define CMEMORY_NEW_FILE (lbl_803A9A94 + 459)

#include "MetroidPrime/CMemoryCardDriver.hpp"

#include "Kyoto/Streams/CBitStreamReader.hpp"
#include "Kyoto/Streams/CBitStreamWriter.hpp"
#include "Kyoto/Streams/CMemoryInStream.hpp"
#include "Kyoto/Streams/CMemoryStreamOut.hpp"
#include "Kyoto/Streams/CInputStream.hpp"
#include "Kyoto/Streams/COutputStream.hpp"
#include "MetroidPrime/Player/CGameOptions.hpp"
#include "MetroidPrime/Player/CGameStateBlocks.hpp"
#include "MetroidPrime/Player/CPersistentOptions.hpp"
#include "Kyoto/CSimplePool.hpp"
#include "rstl/string.hpp"
#include "dolphin/os.h"

//! `sprintf`, `symbols.txt:15618` (`0x8034BEEC`). `dolphin/os.h` does not declare it and
//! `<stdio.h>` would drag mwcceppc's own declarations into a matching unit, so it is declared
//! here with retail's plain C name.
extern "C" int sprintf(char* buffer, const char* format, ...);

//! `lbl_8041C868`, `.sdata2:0x8041C868`, holds a pointer to `"MetroidPrime2"` - the save file's
//! name. `InitializeFileInfo` loads it with `lwz r4,-23384(r2)` (0x8017BC6C), the single
//! `R_PPC_EMB_SDA21` form, so the object is read through a pointer, not named as a literal.
extern "C" const char* lbl_8041C868;

//! `lbl_803A9A70`, `.rodata:0x803A9A70`: the 33-byte comment name constant
//! `"Metroid Prime 2 Echoes" + 11 spaces` + NUL. `InitializeFileInfo` copies all 33 bytes out of
//! it with eight `lwz` and one `lbz` (0x8017BCC4..0x8017BCE4), so its address has to be retail's.
extern "C" const char lbl_803A9A70[33];

//! `lbl_803A9A94 + 466` (`.rodata:0x803A9C66`) is `"%02d.%02d.%02d  %02d:%02d"` - retail's comment
//! timestamp format, `addi r4,r4,466` at 0x8017BD4C.
#define CMEMORY_SAVE_TIME_FORMAT (lbl_803A9A94 + 466)

//! `lbl_80418533`, `.sdata:0x80418533`, `size:0x1 data:byte`, used by address - the fill-byte
//! operand of `fn_80142BA4` in `BuildSaveBuffer` (`addi r5,r13,-30797`, 0x8017B0FC). Same object
//! `src/MetroidPrime/Player/CGameStateSlotDefaults.cpp` names for its own call, one `.sdata` byte
//! higher, and **not `const`**: a `const` declaration puts the address in the read-only small-data
//! area and mwcceppc emits a `lis`/`addi` pair with `R_PPC_ADDR16_HA`/`R_PPC_ADDR16_LO` where
//! retail has the single `R_PPC_EMB_SDA21`.
extern "C" unsigned char lbl_80418533;

// This TU is a scaffold. Save serialization and option synchronization remain incomplete.
static bool sDriverExists; // Guessed name

//!< `SGameFileSlot::mSaveBuffer`'s capacity. Retail passes it as an immediate to the
//!< `CMemoryInStream` in `BuildExistingFileSlot` (`li r5,2616`, 0x8017A74C), and the slot is not
//!< reachable from a constant expression, so the number is named here.
enum { sSaveSlotSize = 0xa38 };

//!< `InitializeFileInfo`'s 33-byte comment name constant is copied out of `.rodata`, so the
//!< aggregate type is named to make the copy an aggregate copy rather than a `memcpy` call.
struct SNameConstant {
  char mData[33];
};

// Guessed name
static uint GetSaveSignature() {
  // TODO: Cache the USA seed XOR the signature of each world's SAVW resource.
  return 0;
}

// Guessed name
static bool IsSaveSignatureInvalid(const void* data) {
  return *static_cast< const uint* >(data) != GetSaveSignature();
}

bool CMemoryCardDriver::IsCardBusy(EState state) {
  return state >= kS_CardMount && state <= kS_CardFormat;
}

bool CMemoryCardDriver::IsCardReading(EState state) {
  return state == kS_CardProbe || state == kS_CardMount || state == kS_CardCheck ||
         state == kS_FileRead;
}

CMemoryCardDriver::CMemoryCardDriver(CMemoryCardSys::EMemoryCardPort cardPort, CAssetId saveBanner,
                                     CAssetId saveIcon0, CAssetId saveIcon1, bool importPersistent)
: mCardPort(cardPort)
, mSaveBanner(saveBanner)
, mSaveIcon0(saveIcon0)
, mSaveIcon1(saveIcon1)
, mState(kS_Initial)
, mError(kE_OK)
, mCardFreeBytes(0)
, mCardFreeFiles(0)
, mFileTime(0)
, mCardSerial(0)
, mSystemData(uchar(0))
, mFileSlots(rstl::auto_ptr< SGameFileSlot >())
, mSaveIdx(0)
, mGameOptionsData(rstl::reserved_vector< uchar, 32 >(uchar(0)))
, mGlobalGameOptionsData(uchar(0))
, mFileInfo(nullptr)
, x1ac_(false)
, mImportPersistent(importPersistent) {
  sDriverExists = true;
  InitializeFileInfo();
  // TODO: Read the selected save index from persistent options and serialize the
  // system options and default CGameOptions into their respective bitstream buffers.
}

CMemoryCardDriver::~CMemoryCardDriver() {
  CMemoryCardSys::UnmountCard(mCardPort);
  sDriverExists = false;
  CMemoryCardSys::mIsCardBusy = false;
}

void CMemoryCardDriver::InitializeFileInfo() {
  mFileInfo = rs_new CMemoryCardSys::CCardFileInfo(mCardPort, rstl::string_l(lbl_8041C868));

  CMemoryCardSys::CCardFileInfo& fileInfo = *mFileInfo;

  fileInfo.ResetHeaderInfo();

  const SNameConstant kName = *(const SNameConstant*)lbl_803A9A70;

  OSCalendarTime time;
  OSTicksToCalendarTime(OSGetTime(), &time);

  char nameBuffer[36];
  sprintf(nameBuffer, CMEMORY_SAVE_TIME_FORMAT, time.mon + 1, time.mday, time.year % 100,
          time.hour, time.min);

  fileInfo.SetComment(rstl::string_l(kName.mData) + nameBuffer);
  fileInfo.LockBannerToken(mSaveBanner, *gpSimplePool);
  fileInfo.LockIconToken(mSaveIcon0, 2, *gpSimplePool);
  fileInfo.BuildHeaderBuffer();

}

// The state machine's poll. Retail's 0x8017BAA8 reads `mState` (+0x10) and the card port (+0x00),
// takes no argument, and its `switch` spans `kS_CardProbe`..`kS_CardFormat` - `addi r0,r3,-19;
// cmplwi r0,8` at 0x8017BB5C/0x8017BB60 - with `kS_CardProbe` a case that only breaks. That
// no-op case is load-bearing: without it the range check is 8 cases wide, not 9.
void CMemoryCardDriver::Update() {
  ProbeResults result = CMemoryCardSys::IsMemoryCardInserted(mCardPort);

  if (result.mError == kCR_NOCARD) {
    if (mState != kS_NoCard) {
      NoCardFound();
    }
    CMemoryCardSys::mIsCardBusy = false;
    return;
  }

  if (mState == kS_CardProbe) {
    UpdateCardProbe();
    CMemoryCardSys::mIsCardBusy = false;
    return;
  }

  ECardResult resultCode = CMemoryCardSys::GetResultCode(mCardPort);
  bool cardBusy = false;

  if (IsCardBusy(mState)) {
    cardBusy = true;

    switch (mState) {
    case kS_CardProbe:
      break;
    case kS_CardMount:
      UpdateMountCard(resultCode);
      break;
    case kS_CardCheck:
      UpdateCardCheck(resultCode);
      break;
    case kS_FileDeleteBad:
      UpdateFileDeleteBad(resultCode);
      break;
    case kS_FileRead:
      UpdateFileRead(resultCode);
      break;
    case kS_FileCreate:
      UpdateFileCreate(resultCode);
      break;
    case kS_FileWrite:
      UpdateFileWrite(resultCode, kS_Ready, kS_FileWriteFailed);
      break;
    case kS_FileWriteTransactional:
      UpdateFileWrite(resultCode, kS_DriverClosed, kS_FileWriteTransactionalFailed);
      break;
    case kS_CardFormat:
      UpdateCardFormat(resultCode);
      break;
    default:
      break;
    }
  }

  CMemoryCardSys::mIsCardBusy = cardBusy;
}

void CMemoryCardDriver::HandleCardError(ECardResult result, EState state) {
  switch (result) {
  case kCR_BUSY:
    break;
  case kCR_WRONGDEVICE:
    mState = state;
    mError = kE_CardWrongDevice;
    break;
  case kCR_NOCARD:
    NoCardFound();
    break;
  case kCR_IOERROR:
    mState = state;
    mError = kE_CardIOError;
    break;
  case kCR_ENCODING:
    mState = state;
    mError = kE_CardWrongCharacterSet;
    break;
  default:
    break;
  }
}

void CMemoryCardDriver::UpdateMountCard(ECardResult result) {
  if (result == kCR_READY) {
    mState = kS_CardMountDone;
    StartCardCheck();
  } else if (result == kCR_BROKEN) {
    mState = kS_CardMountDone;
    mError = kE_CardBroken;
    StartCardCheck();
  } else {
    HandleCardError(result, kS_CardMountFailed);
  }
}

void CMemoryCardDriver::UpdateCardCheck(ECardResult result) {
  if (result == kCR_READY) {
    mState = kS_CardCheckDone;
    if (GetCardFreeBytes() && CMemoryCardSys::GetSerialNo(mCardPort, mCardSerial) != kCR_READY) {
      NoCardFound();
    }
  } else if (result == kCR_BROKEN) {
    mState = kS_CardCheckFailed;
    mError = kE_CardBroken;
  } else {
    HandleCardError(result, kS_CardCheckFailed);
  }
}

void CMemoryCardDriver::UpdateFileRead(ECardResult result) {
  if (result == kCR_READY) {
    ECardResult readRes = mFileInfo->PumpCardRead();
    if (readRes == kCR_READY) {
      mState = kS_Ready;
      if (IsSaveSignatureInvalid(mFileInfo->LoadedData().data())) {
        mState = kS_FileBad;
        mError = kE_FileCorrupted;
      } else {
        ReadFinished();
      }
    } else if (readRes == kCR_BUSY) {
      return;
    } else if (readRes == kCR_CRC_MISMATCH) {
      mState = kS_FileBad;
      mError = kE_FileCorrupted;
    }
  } else {
    HandleCardError(result, kS_FileBad);
  }
}

void CMemoryCardDriver::UpdateFileDeleteBad(ECardResult result) {
  if (result == kCR_READY) {
    mState = kS_CardCheckDone;
    if (GetCardFreeBytes()) {
      IndexFiles();
    }
  } else {
    HandleCardError(result, kS_FileDeleteBadFailed);
  }
}

void CMemoryCardDriver::UpdateFileCreate(ECardResult result) {
  if (result == kCR_READY) {
    mState = kS_FileCreateDone;
    StartFileWrite();
  } else {
    HandleCardError(result, kS_FileCreateFailed);
  }
}

void CMemoryCardDriver::UpdateFileWrite(ECardResult result, EState successState,
                                        EState errorState) {
  if (result == kCR_READY) {
    ECardResult xferResult = mFileInfo->PumpCardTransfer();
    if (xferResult == kCR_READY) {
      mState = successState;
      if (successState == kS_DriverClosed) {
        WriteBackupBuf();
      }
    } else if (xferResult == kCR_BUSY) {
      return;
    } else if (xferResult == kCR_IOERROR) {
      mState = kS_FileWriteFailed;
      mError = kE_CardIOError;
    } else {
      NoCardFound();
    }
  } else {
    HandleCardError(result, errorState);
  }
}

void CMemoryCardDriver::WriteBackupBuf() {
  int idx = gpGameState->SystemOptions().GetSaveIdx();
  if (!mFileSlots[idx].null()) {
    gpGameState->CopyCompressedGameState(idx, mFileSlots[idx]->mSaveBuffer.data());
  }
  gpGameState->SetCardSerial(mCardSerial);
}

void CMemoryCardDriver::UpdateCardFormat(ECardResult result) {
  if (result == kCR_READY) {
    mState = kS_CardFormatted;
  } else if (result == kCR_BROKEN) {
    mState = kS_CardFormatFailed;
    mError = kE_CardIOError;
  } else {
    HandleCardError(result, kS_CardFormatFailed);
  }
}

void CMemoryCardDriver::StartCardProbe() {
  mState = kS_CardProbe;
  mError = kE_OK;
  UpdateCardProbe();
}

void CMemoryCardDriver::UpdateCardProbe() {
  ProbeResults result = CMemoryCardSys::IsMemoryCardInserted(mCardPort);
  ECardResult error = result.mError;

  if (error == kCR_READY) {
    if (result.mSectorSize != 0x2000) {
      mState = kS_CardProbeFailed;
      mError = kE_CardNon8KSectors;
      return;
    }
  } else if (error == kCR_BUSY) {
    return;
  } else if (error == kCR_WRONGDEVICE) {
    mState = kS_CardProbeFailed;
    mError = kE_CardWrongDevice;
    return;
  } else {
    NoCardFound();
    return;
  }

  mState = kS_CardProbeDone;
  StartMountCard();
}

void CMemoryCardDriver::StartMountCard() {
  mState = kS_CardMount;
  mError = kE_OK;
  const ECardResult result = CMemoryCardSys::MountCard(mCardPort);
  if (result != kCR_READY) {
    UpdateMountCard(result);
  }
}

void CMemoryCardDriver::StartCardCheck() {
  mError = kE_OK;
  mState = kS_CardCheck;
  const ECardResult result = CMemoryCardSys::CheckCard(mCardPort);
  if (result != kCR_READY) {
    UpdateCardCheck(result);
  }
}

void CMemoryCardDriver::NoCardFound() {
  mState = kS_NoCard;
  CMemoryCardSys::mIsCardBusy = false;
}

void CMemoryCardDriver::IndexFiles() {
  mError = kE_OK;
  ECardResult result = mFileInfo->Open();
  if (result == kCR_NOFILE) {
    mError = kE_FileMissing;
    mState = kS_FileBad;
  } else if (result == kCR_READY) {
    CardStat stat;
    if (CMemoryCardSys::GetStatus(mCardPort, mFileInfo->GetFileNo(), stat) == kCR_READY) {
      if (stat.GetCommentAddr() == -1) {
        mError = kE_FileCorrupted;
        mState = kS_FileBad;
      } else {
        StartFileRead();
      }
    } else {
      NoCardFound();
    }
  } else {
    NoCardFound();
  }
}

void CMemoryCardDriver::StartFileDeleteBad() {
  mError = kE_OK;
  mState = kS_FileDeleteBad;
  const ECardResult result = CMemoryCardSys::FastDeleteFile(mCardPort, mFileInfo->GetFileNo());
  if (result != kCR_READY) {
    UpdateFileDeleteBad(result);
  }
}

void CMemoryCardDriver::StartFileRead() {
  mError = kE_OK;
  mState = kS_FileRead;
  const ECardResult result = mFileInfo->StartRead();
  if (result != kCR_READY) {
    UpdateFileRead(result);
  }
}

void CMemoryCardDriver::StartFileCreate() {
  mError = kE_OK;
  mState = kS_FileCreate;
  BuildSaveBuffer();
  ECardResult result = mFileInfo->CreateFile();
  // Retail 0x8017B368..0x8017B398: after the `kCR_READY` test, `cmpwi r4,-9 / beq` and
  // `cmpwi r4,-8 / bne` **both** branch to the same body - `li r3,15 / li r0,5 / stw / stw`, i.e.
  // `kS_FileCreateFailed` + `kE_CardFull` - and everything else falls into `UpdateFileCreate`.
  // So the two codes share one arm, and the `kCR_INSSPACE` test comes first: that is what the `||`
  // compiles to, and it is not the same code as an `else if` chain (measured: 86.30% -> 100%).
  if (result != kCR_READY) {
    if (result == kCR_INSSPACE || result == kCR_NOENT) {
      mState = kS_FileCreateFailed;
      mError = kE_CardFull;
    } else {
      UpdateFileCreate(result);
    }
  }
}

void CMemoryCardDriver::StartFileWrite() {
  mError = kE_OK;
  mState = kS_FileWrite;
  const ECardResult result = mFileInfo->WriteFile();
  if (result != kCR_READY) {
    UpdateFileWrite(result, kS_Ready, kS_FileWriteFailed);
  }
}

void CMemoryCardDriver::StartFileWriteTransactional() {
  mError = kE_OK;
  mState = kS_FileWriteTransactional;
  BuildSaveBuffer();
  const ECardResult result = mFileInfo->WriteFile();
  if (result != kCR_READY) {
    UpdateFileWrite(result, kS_DriverClosed, kS_FileWriteTransactionalFailed);
  }
}

void CMemoryCardDriver::StartCardFormat() {
  mError = kE_OK;
  mState = kS_CardFormat;
  const ECardResult result = CMemoryCardSys::FormatCard(mCardPort);
  if (result != kCR_READY) {
    UpdateCardFormat(result);
  }
}

// Retail 0x8017B0C8.
extern "C" void fn_80142BA4(SGameStateBlock* self, int count, const unsigned char* src);

// Guessed name
void CMemoryCardDriver::BuildSaveBuffer() {
  ExportPersistentOptions();
  ExportGameOptions();

  rstl::vector< uchar >& saveBuffer = mFileInfo->SaveBuffer();
  fn_80142BA4(reinterpret_cast< SGameStateBlock* >(&saveBuffer), 8184, &lbl_80418533);

  CMemoryStreamOut w(saveBuffer.data(), 8184);
  SSaveHeader header(GetSaveSignature(), mSaveIdx);
  for (int i = 0; i < 3; ++i) {
    // The `uchar` cast is a codegen nudge, not a change of value: retail closes the boolean
    // normalisation with `rlwinm r0,r0,27,24,31` (0x8017B154) where an uncast `bool` closes it
    // with `srwi r0,r0,5`. Both compute `>> 5` of a `cntlzw` result; only the masked form matches
    // (measured 98.68% -> 100%).
    header.mSavePresent[i] = static_cast< uchar >(mFileSlots[i].null() == false);
  }
  w.Put(header);
  w.Put(mSystemData.data(), mSystemData.capacity());
  for (int i = 0; i < 3; ++i) {
    w.Put(mGameOptionsData[i].data(), 32);
  }
  w.Put(mGlobalGameOptionsData.data(), 32);

rstl::auto_ptr< SGameFileSlot >* it = mFileSlots.data();
  for (; it != mFileSlots.data() + mFileSlots.size(); ++it) {
    if (!it->null()) {
      w.Put(**it);
    }
  }
}

// Retail 0x8017AE68. `SSaveHeader`'s stream constructor and the four `CInputStream::Get` calls are
// the 0x1F8-byte save buffer: 192 bytes of system options, three 32-byte option buffers, one more
// 32-byte global buffer, then one 0xA38 save per present slot.
void CMemoryCardDriver::ReadFinished() {
  CardStat stat;
  if (CMemoryCardSys::GetStatus(mCardPort, mFileInfo->GetFileNo(), stat) != kCR_READY) {
    NoCardFound();
    return;
  }

  mFileTime = stat.GetTime();

  CMemoryInStream r(mFileInfo->LoadedData().data(), 8184);
  SSaveHeader header(r);
  mSaveIdx = header.mSaveIdx;
  r.Get(mSystemData.data(), mSystemData.capacity());

  for (int i = 0; i < mGameOptionsData.capacity(); ++i) {
    r.Get(mGameOptionsData[i].data(), mGameOptionsData[i].capacity());
  }
  r.Get(mGlobalGameOptionsData.data(), mGlobalGameOptionsData.capacity());

  for (int i = 0; i < mFileSlots.capacity(); ++i) {
    if (header.mSavePresent[i]) {
      mFileSlots[i] = rs_new SGameFileSlot(r);
    } else {
      mFileSlots[i] = nullptr;
    }
  }

  if (mImportPersistent) {
    ImportPersistentOptions();
    ImportGameOptions();
  }
}

// Retail 0x8017AD28. `CGameOptions` declares a destructor, so a local of that type would make
// mwcceppc call `__dt__12CGameOptionsFv` at scope exit - a name retail's symbol table does not
// carry. Retail's is the same function at the same address under the unnamed `fn_80004D84`, so
// the local is a POD mirror and the destructor is called by hand; `fn_80003D00` is retail's
// `CGameOptions` copy assignment under the same kind of name. `CMainResetGameState.cpp` sets out
// the same arrangement for its own `SGameOptionsCopy`.
struct SGameOptionsMirror {
  u8 x00[sizeof(CGameOptions)];
};
CHECK_SIZEOF(SGameOptionsMirror, 0x44)

extern "C" void __ct__12CGameOptionsFv(CGameOptions* self);
extern "C" void fn_80003D00(CGameOptions* self, const CGameOptions* src);
extern "C" void fn_80004D84(CGameOptions* self, int flag);

void CMemoryCardDriver::EraseFileSlot(int idx) {
  mFileSlots[idx] = nullptr;

  SGameOptionsMirror opts;
  __ct__12CGameOptionsFv(reinterpret_cast< CGameOptions* >(&opts));
  {
    CMemoryStreamOut w(mGameOptionsData[idx].data(), mGameOptionsData[idx].capacity());
    CBitStreamWriter writer(w);
    reinterpret_cast< CGameOptions* >(&opts)->PutTo(writer);
  }
  gpGameState->CopyCompressedGameOptions(idx, mGameOptionsData[idx].data());

  if (gpGameState->SystemOptions().GetSaveIdx() == idx) {
    fn_80003D00(&gpGameState->GameOptions(), reinterpret_cast< const CGameOptions* >(&opts));
  }
  fn_80004D84(reinterpret_cast< CGameOptions* >(&opts), -1);
}

// Retail 0x8017AB30: copy the slot object through a stream over the source's save buffer, then
// hand the source slot's compressed game options to the destination index.
void CMemoryCardDriver::CopyFileSlot(int from, int to) {
  {
    CMemoryInStream r(mFileSlots[from]->mSaveBuffer.data(), mFileSlots[from]->mSaveBuffer.capacity());
    mFileSlots[to] = rs_new SGameFileSlot(r);
  }
  gpGameState->CopyCompressedGameOptions(to, gpGameState->CompressedGameOptionsAt(from).data());
  mGameOptionsData[to] = mGameOptionsData[from];
}

// Retail 0x8017A99C. The three-element loop republishes every slot into `gpGameState`'s compressed
// game-state buffers, then the system options are read back out of `mSystemData` and re-imported.
void CMemoryCardDriver::BuildNewFileSlot(int idx) {
  rstl::auto_ptr< SGameFileSlot >& slot = mFileSlots[idx];
  if (slot.null()) {
    slot = rs_new SGameFileSlot();
  }

  for (int i = 0; i < mFileSlots.capacity(); ++i) {
    if (!mFileSlots[i].null()) {
      gpGameState->CopyCompressedGameState(i, mFileSlots[i]->mSaveBuffer.data());
    } else {
      gpGameState->ClearCompressedGameState(i);
    }
  }

  {
    CMemoryInStream r(mSystemData.data(), mSystemData.capacity());
    gpGameState->ReadSystemOptions(r);
  }

  gpGameState->SystemOptions().SetSaveIdx(idx);
  ImportPersistentOptions();
  ImportGameOptions();
  gpGameState->SetCardSerial(mCardSerial);
}

// Retail 0x8017A708. The loop reads each of `gpGameState`'s three compressed game-state buffers
// (`+0x118 + i*16` is the element count, `+0x120 + i*16` the data pointer) and rebuilds the slot
// from it, which is the mirror image of `BuildNewFileSlot`'s publish loop.
void CMemoryCardDriver::BuildExistingFileSlot(int idx) {
  for (int i = 0; i < mFileSlots.capacity(); ++i) {
    if (gpGameState->CompressedGameStatesAt(i).size() != 0) {
      CMemoryInStream r(gpGameState->CompressedGameStatesAt(i).data(), sSaveSlotSize);
      mFileSlots[i] = rs_new SGameFileSlot(r);
    } else {
      mFileSlots[i] = nullptr;
    }
  }

  ExportGameOptions();
  gpGameState->SystemOptions().SetSaveIdx(idx);

  if (mFileSlots[idx].null()) {
    mFileSlots[idx] = rs_new SGameFileSlot();
  } else {
    mFileSlots[idx]->InitializeFromGameState();
  }

  {
    CMemoryStreamOut w(mSystemData.data(), mSystemData.capacity());
    gpGameState->WriteSystemOptions(w);
  }
  mSaveIdx = gpGameState->SystemOptions().GetSaveIdx();
}

void CMemoryCardDriver::ImportPersistentOptions() {
  CMemoryInStream r(mSystemData.data(), mSystemData.capacity());
  CBitStreamReader reader(r);
  CPersistentOptions state(reader);
  gpGameState->SetSystemOptions(state);
}

// Guessed name
void CMemoryCardDriver::ImportGameOptions() {
  for (int i = 0; i < mGameOptionsData.capacity(); ++i) {
    gpGameState->CopyCompressedGameOptions(i, mGameOptionsData[i].data());
  }
  gpGameState->CopyCompressedMultiplayerOptions(mGlobalGameOptionsData.data());
}

void CMemoryCardDriver::ExportPersistentOptions() {
  CMemoryInStream r(mSystemData.data(), mSystemData.capacity());
  CBitStreamReader reader(r);
  CPersistentOptions state(reader);
  gpGameState->ExportPersistentOptions(state);
  mSaveIdx = state.GetSaveIdx();

  CMemoryStreamOut w(mSystemData.data(), mSystemData.capacity());
  CBitStreamWriter writer(w);
  state.PutTo(writer);
}

// Guessed name
void CMemoryCardDriver::ExportGameOptions() {
  for (int i = 0; i < mGameOptionsData.capacity(); ++i) {
    CMemoryStreamOut w(mGameOptionsData[i].data(), mGameOptionsData[i].capacity());
    w.Put(gpGameState->CompressedGameOptionsAt(i).data(), gpGameState->CompressedGameOptionsAt(i).size());
  }
  CMemoryStreamOut w(mGlobalGameOptionsData.data(), mGlobalGameOptionsData.capacity());
  w.Put(gpGameState->CompressedMultiplayerOptions().data(),
        gpGameState->CompressedMultiplayerOptions().size());
}

// Guessed name
bool CMemoryCardDriver::IsRepairingHeader() const {
  return mFileInfo->IsRepairingHeader();
}

SSaveHeader::SSaveHeader(uint signature, int saveIdx) : mSignature(signature), mSaveIdx(saveIdx) {}

SSaveHeader::SSaveHeader(CInputStream& in) : mSignature(in.ReadInt32()), mSaveIdx(in.ReadInt32()) {
  for (int i = 0; i < 3; ++i) {
    mSavePresent[i] = in.ReadBool();
  }
  in.ReadInt32(); // Trailing SAVH marker.
}

void SSaveHeader::PutTo(COutputStream& out) const {
  out.WriteUint32(mSignature);
  out.WriteInt32(mSaveIdx);
  for (int i = 0; i < 3; ++i) {
    out.WriteBool(mSavePresent[i]);
  }
  out.WriteUint32('SAVH');
}

SGameFileSlot::SGameFileSlot() : mSaveBuffer(uchar(0)) {
  CMemoryStreamOut w(mSaveBuffer.data(), mSaveBuffer.capacity());
  CBitStreamWriter writer(w);
  CGameState::SerializeNewForCleanSlot(writer, gpGameState->GetHardModeEnabled());
}

SGameFileSlot::SGameFileSlot(CInputStream& in) : mSaveBuffer(uchar(0)) {
  in.Get(mSaveBuffer.data(), mSaveBuffer.capacity());
  mFileInfo = CGameState::LoadGameFileState(mSaveBuffer.data());
}

void SGameFileSlot::PutTo(COutputStream& out) const {
  out.Put(mSaveBuffer.data(), mSaveBuffer.capacity());
}

void SGameFileSlot::InitializeFromGameState() {
  {
    CMemoryStreamOut w(mSaveBuffer.data(), mSaveBuffer.capacity());
    CBitStreamWriter writer(w);
    gpGameState->PutTo(writer);
  }
  mFileInfo = CGameState::LoadGameFileState(mSaveBuffer.data());
}

const CGameState::GameFileStateInfo* CMemoryCardDriver::GetGameFileStateInfo(int idx) {
  return mFileSlots[idx].null() ? nullptr : &mFileSlots[idx]->mFileInfo;
}

bool CMemoryCardDriver::GetCardFreeBytes() {
  if (CMemoryCardSys::GetNumFreeBytes(mCardPort, mCardFreeBytes, mCardFreeFiles) != kCR_READY) {
    NoCardFound();
    return false;
  }
  return true;
}
