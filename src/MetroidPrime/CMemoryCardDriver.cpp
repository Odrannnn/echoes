#include "MetroidPrime/CMemoryCardDriver.hpp"

#include "Kyoto/Streams/CBitStreamReader.hpp"
#include "Kyoto/Streams/CBitStreamWriter.hpp"
#include "Kyoto/Streams/CMemoryInStream.hpp"
#include "Kyoto/Streams/CMemoryStreamOut.hpp"
#include "Kyoto/Streams/CInputStream.hpp"
#include "Kyoto/Streams/COutputStream.hpp"
#include "MetroidPrime/Player/CPersistentOptions.hpp"

// This TU is a scaffold. Save serialization and option synchronization remain incomplete.
static bool sDriverExists; // Guessed name

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
  // TODO: Create the card-file object, timestamp its comment, and prepare its
  // banner/icon header. Ownership is held by mFileInfo, not Prime's two-file array.
}

void CMemoryCardDriver::Update() {
  // TODO: Probe card removal, dispatch the active operation, and publish card-busy state.
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
  if (result != kCR_READY) {
    if (result == kCR_NOENT) {
      mState = kS_FileCreateFailed;
      mError = kE_CardFull;
    } else if (result == kCR_INSSPACE) {
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

// Guessed name
void CMemoryCardDriver::BuildSaveBuffer() {
  // TODO: Export options, then write the save header, option buffers and occupied slots.
}

void CMemoryCardDriver::ReadFinished() {
  // TODO: Record file time and deserialize the header, option buffers and present
  // game slots; import global options when mImportPersistent is set.
}

void CMemoryCardDriver::EraseFileSlot(int idx) {
  // TODO: Release the slot, reset its game options and begin the appropriate card write.
}

// Guessed name
void CMemoryCardDriver::CopyFileSlot(int from, int to) {
  // TODO: Copy both slot contents and their game options, then start the card write.
}

void CMemoryCardDriver::BuildNewFileSlot(int idx) {
  // TODO: Allocate a clean game slot and serialize its current per-game options.
}

void CMemoryCardDriver::BuildExistingFileSlot(int idx) {
  // TODO: Refresh the selected slot from the current game and serialize its options.
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
    w.Put(gpGameState->CompressedGameOptions()[i].data(), gpGameState->CompressedGameOptions()[i].size());
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
