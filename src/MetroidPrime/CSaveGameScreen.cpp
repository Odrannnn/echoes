#include "MetroidPrime/CSaveGameScreen.hpp"

#include "GuiSys/CGuiFrame.hpp"
#include "GuiSys/CGuiTableGroup.hpp"
#include "GuiSys/CGuiTextPane.hpp"
#include "GuiSys/CGuiWidget.hpp"
#include "GuiSys/CGuiWidgetDrawParms.hpp"
#include "Kyoto/Audio/CSfxManager.hpp"
#include "Kyoto/CResFactory.hpp"
#include "Kyoto/Graphics/CGraphics.hpp"
#include "Kyoto/Text/CStringTable.hpp"
#include "MetroidPrime/CMemoryCard.hpp"
#include "MetroidPrime/CMemoryCardDriver.hpp"
#include "MetroidPrime/CMain.hpp"
#include "rstl/string.hpp"

static const char* const skSaveBanner = "TXTR_SaveBanner";
static const char* const skSaveIcon0 = "TXTR_SaveIcon0";
static const char* const skSaveIcon1 = "TXTR_SaveIcon1";
static const char* const skMemoryCardStrings = "STRG_MemoryCard";
static const char* const skGenericMenu = "FRME_GenericMenu";
// The five above are named objects; the seven widget names below are literals used at their
// point of call, which is what puts them in .rodata right after the five and makes PumpLoad
// materialise each address with lis/addi instead of loading a pointer through r0.

// Retail passes a shared, statically built draw-parms object rather than a temporary.
static const CGuiWidgetDrawParms sDrawParms(1.f, CVector3f::Zero());

CSaveGameScreen::EUIType CSaveGameScreen::SelectUIType() const {
  const EState state = mCardDriver->GetState();
  const CMemoryCardDriver::EError error = mCardDriver->GetError();
  if (state == kS_NoCard) {
    return kUIT_NoCardFound;
  }
  if (mUiType == kUIT_ProgressWillBeLost || mUiType == kUIT_AllDataWillBeLost ||
      mUiType == kUIT_NotOriginalCard) {
    return mUiType;
  }
  if (CMemoryCardDriver::IsCardBusy(state)) {
    if (mCardDriver->IsRepairingHeader()) {
      return kUIT_BusyWriting;
    }
    if (state == kS_FileWrite || state == kS_FileCreate) {
      return kUIT_BusyWritingInitial;
    }
    return CMemoryCardDriver::IsCardReading(state) ? kUIT_BusyReading : kUIT_BusyWriting;
  }
  if (state == kS_Ready) {
    return kUIT_SaveReady;
  }
  if (error == CMemoryCardDriver::kE_CardBroken) {
    return kUIT_NeedsFormatBroken;
  }

  if (error == CMemoryCardDriver::kE_CardWrongCharacterSet) {
    return kUIT_NeedsFormatEncoding;
  }

  if (error == CMemoryCardDriver::kE_CardWrongDevice) {
    return kUIT_WrongDevice;
  }

  if (error == CMemoryCardDriver::kE_CardFull) {
    return kUIT_InsufficientSpaceOKCheck;
  }

  if (error == CMemoryCardDriver::kE_CardNon8KSectors) {
    return kUIT_IncompatibleCard;
  }

  if (error == CMemoryCardDriver::kE_FileCorrupted) {
    return kUIT_SaveCorrupt;
  }

  if (error == CMemoryCardDriver::kE_CardIOError) {
    return kUIT_CardDamaged;
  }

  return kUIT_Empty;
}

void CSaveGameScreen::SetUIText() {
  mUiTextDirty = false;

  // Echoes looks its strings up by name, not by table index: every literal below is a name in
  // STRG_MemoryCard, and the six locals are the string-table entry names themselves (null when
  // that pane has nothing to show). The names are written in the order they are first needed,
  // which is the order retail's .rodata holds them at 0x803A9D62 and up.
  const CStringTable& strings = *mStrgMemoryCard.GetObject();
  const char* messageA = nullptr;
  const char* message = nullptr;
  // Retail keeps the four option names in memory rather than in registers: it stores each one
  // at 184..196(r1) inside the switch arm that sets it (0x8017D89C onwards) and reads them
  // back out at the end. An array of four reproduces that; four separate locals do not.
  const char* opt[4] = { nullptr, nullptr, nullptr, nullptr };
  switch (mUiType) {
  case kUIT_BusyWriting:
    message = "StatusWriting";
    break;
  case kUIT_BusyWritingInitial:
    message = "StatusWritingInitial";
    break;
  case kUIT_NoCardFound:
    message = "NoMemoryCard";
    opt[0] = "ChoiceRetry";
    opt[1] = "ChoiceContinueWithoutSave";
    break;
  case kUIT_NeedsFormatBroken:
    message = "CorruptedCard";
    opt[0] = "ChoiceRetry";
    opt[1] = "ChoiceContinueWithoutSave";
    opt[2] = "ChoiceFormatCard";
    break;
  case kUIT_NeedsFormatEncoding:
    message = "EncodingMismatch";
    opt[0] = "ChoiceRetry";
    opt[1] = "ChoiceContinueWithoutSave";
    opt[2] = "ChoiceFormatCard";
    break;
  case kUIT_CardDamaged:
    message = "DamagedCard";
    opt[0] = "ChoiceRetry";
    opt[1] = "ChoiceContinueWithoutSave";
    break;
  case kUIT_WrongDevice:
    message = "WrongDevice";
    opt[0] = "ChoiceRetry";
    opt[1] = "ChoiceContinueWithoutSave";
    break;
  case kUIT_InsufficientSpaceOKCheck:
    message = "InsufficientSpaceMain";
    opt[0] = "ChoiceRetry";
    opt[1] = "ChoiceContinueWithoutSave";
    opt[2] = "ChoiceManageMemoryCard";
    break;
  case kUIT_IncompatibleCard:
    message = "BadSectorSize";
    opt[0] = "ChoiceRetry";
    opt[1] = "ChoiceContinueWithoutSave";
    break;
  case kUIT_SaveCorrupt:
    message = "CorruptedFile";
    opt[0] = "ChoiceRetry";
    opt[1] = "ChoiceContinueWithoutSave";
    opt[2] = "ChoiceDeleteCorruptedFile";
    break;
  case kUIT_ProgressWillBeLost:
    messageA = "TitleWarning";
    message = "IPLWarning";
    opt[0] = "ChoiceCancel";
    opt[1] = "ChoiceContinueWithWarning";
    break;
  case kUIT_NotOriginalCard:
    messageA = "TitleWarning";
    message = "ConfirmOverwrite";
    opt[0] = mSaveCtx == kSC_InGame ? "ChoiceCancel" : "ChoiceContinueWithoutSave";
    opt[1] = "ChoiceContinueWithWarning";
    break;
  case kUIT_AllDataWillBeLost:
    messageA = "TitleWarning";
    message = "ConfirmFormat";
    opt[0] = "ChoiceCancel";
    opt[1] = "ChoiceContinueWithWarning";
    break;
  case kUIT_SaveReady:
    if (mSaveCtx == kSC_InGame) {
      message = "SaveFile";
      opt[0] = "ChoiceYes";
      opt[1] = "ChoiceNo";
    }
    break;
  default:
    break;
  }

  const rstl::wstring empty = rstl::wstring_l(L"");
  const rstl::wstring messageAText =
      messageA == nullptr ? empty : rstl::wstring_l(strings.GetString(messageA));
  const rstl::wstring messageText =
      messageAText + (message == nullptr ? empty : rstl::wstring_l(strings.GetString(message)));
  mTextpaneMessage->TextSupport().SetText(messageText);
  mTextpaneChoice0->TextSupport().SetText(
      opt[0] == nullptr ? empty : rstl::wstring_l(strings.GetString(opt[0])));
  mTextpaneChoice1->TextSupport().SetText(
      opt[1] == nullptr ? empty : rstl::wstring_l(strings.GetString(opt[1])));
  mTextpaneChoice2->TextSupport().SetText(
      opt[2] == nullptr ? empty : rstl::wstring_l(strings.GetString(opt[2])));
  mTextpaneChoice3->TextSupport().SetText(
      opt[3] == nullptr ? empty : rstl::wstring_l(strings.GetString(opt[3])));
  mTextpaneChoice0->SetIsSelectable(opt[0] != nullptr);
  mTextpaneChoice1->SetIsSelectable(opt[1] != nullptr);
  mTextpaneChoice2->SetIsSelectable(opt[2] != nullptr);
  mTextpaneChoice3->SetIsSelectable(opt[3] != nullptr);
  mTablegroupChoices->SetUserSelection(0);
  mTablegroupChoices->SetIsActive(
      opt[0] != nullptr || opt[1] != nullptr || opt[2] != nullptr || opt[3] != nullptr);
  SetUIColors();
  mHasMessage = message != nullptr;
}

CMemoryCardDriver* CSaveGameScreen::ConstructCardDriver(bool importPersistent) {
  return rs_new CMemoryCardDriver(
      CMemoryCardSys::kCS_SlotA, gpResourceFactory->GetResourceIdByName(skSaveBanner)->GetId(),
      gpResourceFactory->GetResourceIdByName(skSaveIcon0)->GetId(),
      gpResourceFactory->GetResourceIdByName(skSaveIcon1)->GetId(), importPersistent);
}

CSaveGameScreen::CSaveGameScreen(ESaveContext saveContext, u64 cardSerial)
: mSaveCtx(saveContext)
, mSerial(cardSerial)
, mUiType(kUIT_Empty)
, mTxtrSaveBanner(gpSimplePool->GetObj(skSaveBanner))
, mTxtrSaveIcon0(gpSimplePool->GetObj(skSaveIcon0))
, mTxtrSaveIcon1(gpSimplePool->GetObj(skSaveIcon1))
, mStrgMemoryCard(gpSimplePool->GetObj(skMemoryCardStrings))
, mFrmeGenericMenu(gpSimplePool->GetObj(skGenericMenu))
, mLoadedFrame(nullptr)
, mCardDriver(nullptr)
, mIowRet(CIOWin::kMR_Normal)
, mNavConfirmSfx(0x5e3)
, mNavMoveSfx(0x5e1)
, mNavBackSfx(0x5e3)
, mNeedsDriverReset(false)
, mUiTextDirty(false)
, mSavingDisabled(false)
, mInGame(mSaveCtx == kSC_InGame)
, mFrontEndSfx(mSaveCtx == kSC_FrontEnd)
, mHasMessage(false) {
  mTxtrSaveBanner.Lock();
  mTxtrSaveIcon0.Lock();
  mTxtrSaveIcon1.Lock();
  mStrgMemoryCard.Lock();
  mFrmeGenericMenu.Lock();

  const rstl::vector< CMemoryCard::MemoryWorld >& worlds = gpMemoryCard->GetMemoryWorlds();
  mSaveWorlds.reserve(worlds.size());
  for (rstl::vector< CMemoryCard::MemoryWorld >::const_iterator it = worlds.begin();
       it != worlds.end(); ++it) {
    TToken< CWorldSaveGameInfo > token =
        gpSimplePool->GetObj(SObjectTag('SAVW', it->second.GetSaveWorldAssetId()));
    token.Lock();
    // The reserve above sized the vector to exactly worlds.size(), and the loop runs once per
    // world, so retail's push_back here needs no growth path (it is `construct(...); ++mCount`).
    mSaveWorlds.push_back_unsafe(token);
  }
}

CSaveGameScreen::~CSaveGameScreen() {}

void CSaveGameScreen::ResetCardDriver() {
  mSavingDisabled = false;
  mCardDriver = nullptr;
  mCardDriver = ConstructCardDriver(mSaveCtx == kSC_FrontEnd && !mNeedsDriverReset);
  mCardDriver->StartCardProbe();
  mUiType = kUIT_Empty;
  mIowRet = CIOWin::kMR_Normal;
  SetUIText();
}

bool CSaveGameScreen::PumpLoad() {
  if (mLoadedFrame != nullptr) {
    return true;
  }
  // The first three tokens are tested with the *const* TCachedToken::IsLoaded (0x8017CDC0:
  // `mItem` is read first and only then the CToken's own flag is consulted), while the two
  // below use the caching overload. Binding them to const refs picks the const one without
  // changing the member, which must stay mutable for the caching calls that follow.
  const TCachedToken< CTexture >& saveBanner = mTxtrSaveBanner;
  const TCachedToken< CTexture >& saveIcon0 = mTxtrSaveIcon0;
  const TCachedToken< CTexture >& saveIcon1 = mTxtrSaveIcon1;
  if (!saveBanner.IsLoaded() || !saveIcon0.IsLoaded() || !saveIcon1.IsLoaded() ||
      !mStrgMemoryCard.IsLoaded()) {
    return false;
  }
  for (rstl::vector< TToken< CWorldSaveGameInfo > >::const_iterator it = mSaveWorlds.begin();
       it != mSaveWorlds.end(); ++it) {
    if (!it->IsLoaded()) {
      return false;
    }
  }
  // Retail's fifth token test, 0x8017CED8: the caching TCachedToken::IsLoaded() on the frame
  // token. It is what populates mItem, so GetObject() below is only non-null once this passes.
  if (mFrmeGenericMenu.IsLoaded()) {
    mLoadedFrame = mFrmeGenericMenu.GetObject();
    mTextpaneMessage = static_cast<CGuiTextPane*>(mLoadedFrame->FindWidget("textpane_message"));
    mTablegroupChoices =
        static_cast<CGuiTableGroup*>(mLoadedFrame->FindWidget("tablegroup_choices"));
    mTextpaneChoice0 = static_cast<CGuiTextPane*>(mLoadedFrame->FindWidget("textpane_choice0"));
    mTextpaneChoice1 = static_cast<CGuiTextPane*>(mLoadedFrame->FindWidget("textpane_choice1"));
    mTextpaneChoice2 = static_cast<CGuiTextPane*>(mLoadedFrame->FindWidget("textpane_choice2"));
    mTextpaneChoice3 = static_cast<CGuiTextPane*>(mLoadedFrame->FindWidget("textpane_choice3"));
    CGuiWidget* const messageBg = mLoadedFrame->FindWidget("model_messagebg");
    if (messageBg != nullptr && mSaveCtx != kSC_FrontEnd) {
      messageBg->SetVisibility(false, kTM_Children);
    }
    mTablegroupChoices->SetMenuAdvanceCallback(
        TFunctor1FromMethod< CSaveGameScreen, CGuiTableGroup* const >::Make(
            *this, &CSaveGameScreen::DoAdvance));
    mTablegroupChoices->SetMenuSelectionChangeCallback(
        TFunctor2FromMethod< CSaveGameScreen, CGuiTableGroup* const, const int >::Make(
            *this, &CSaveGameScreen::DoSelectionChange));
  } else {
    return false;
  }
  mCardDriver = ConstructCardDriver(mSaveCtx == kSC_FrontEnd);
  if (mSaveCtx == kSC_InGame) {
    mCardDriver->StartCardProbe();
  }
  mUiType = SelectUIType();
  SetUIText();
  return true;
}

CIOWin::EMessageReturn CSaveGameScreen::Update(float dt) {
  if (!PumpLoad()) {
    return CIOWin::kMR_Normal;
  }

  mLoadedFrame->Update(dt);
  mCardDriver->Update();

  const EState state = mCardDriver->GetState();
  const CMemoryCardDriver::EError error = mCardDriver->GetError();
  if (state == kS_DriverClosed) {
    if (mNeedsDriverReset) {
      ResetCardDriver();
      mNeedsDriverReset = false;
    } else {
      mIowRet = CIOWin::kMR_Exit;
    }
  } else if (state == kS_CardCheckDone && mUiType != kUIT_NotOriginalCard) {
    const u64 cardSerial = mCardDriver->GetCardSerial();
    if (cardSerial != 0 && cardSerial != mSerial) {
      if (mInGame) {
        mUiType = kUIT_NotOriginalCard;
        mUiTextDirty = true;
      } else {
        mSerial = mCardDriver->GetCardSerial();
        mCardDriver->IndexFiles();
      }
    } else {
      mCardDriver->IndexFiles();
    }
  } else if (state == kS_Ready) {
    if (mNeedsDriverReset) {
      mCardDriver->StartFileWriteTransactional();
    }
  }

  if (mIowRet != CIOWin::kMR_Normal) {
    return mIowRet;
  }

  EUIType oldTp = mUiType;
  mUiType = SelectUIType();
  if (oldTp != mUiType || mUiTextDirty) {
    SetUIText();
  }

  if (state == kS_NoCard) {
    const ProbeResults res = CMemoryCardSys::IsMemoryCardInserted(CMemoryCardSys::kCS_SlotA);
    if (res.mError == kCR_READY || res.mError == kCR_WRONGDEVICE) {
      ResetCardDriver();
    }
  } else if (state == kS_CardFormatted) {
    ResetCardDriver();
  } else if (state == kS_FileBad && error == CMemoryCardDriver::kE_FileMissing) {
    mCardDriver->StartFileCreate();
  }

  return CIOWin::kMR_Normal;
}

void CSaveGameScreen::ProcessUserInput(const CFinalInput& input) {
  if (mLoadedFrame != nullptr) {
    mLoadedFrame->ProcessUserInput(input);
  }
}

void CSaveGameScreen::ContinueWithoutSaving() {
  mIowRet = CIOWin::kMR_RemoveIOWin;
  gpGameState->SetCardSerial(0);
}

void CSaveGameScreen::Draw() const {
  if (mLoadedFrame != nullptr && mHasMessage) {
    CGraphics::SetDepthRange(0.f, 0.001f);
    mLoadedFrame->Draw(sDrawParms);
    CGraphics::SetDepthRange(0.f, 1.f);
  }
}

const CGameState::GameFileStateInfo* CSaveGameScreen::GetGameData(int idx) const {
  return mCardDriver->GetGameFileStateInfo(idx);
}

int CSaveGameScreen::GetSaveIdx() const { return mCardDriver->GetSaveIdx(); }

void CSaveGameScreen::EraseGame(int idx) {
  mCardDriver->EraseFileSlot(idx);
  mNeedsDriverReset = true;
  mCardDriver->StartFileWriteTransactional();
}

void CSaveGameScreen::CopyGame(int from, int to) {
  mCardDriver->CopyFileSlot(from, to);
  mNeedsDriverReset = true;
  mCardDriver->StartFileWriteTransactional();
}

void CSaveGameScreen::SaveChanges() {
  if (!mSavingDisabled) {
    mNeedsDriverReset = true;
    mSerial = mCardDriver->GetCardSerial();
    mCardDriver->StartFileWriteTransactional();
  }
}

void CSaveGameScreen::StartGame(int idx) {
  const bool newGame = mCardDriver->GetGameFileStateInfo(idx) == nullptr;
  gpGameState->SystemOptions().SetSaveIdx(idx);
  mCardDriver->ExportPersistentOptions();
  mCardDriver->ExportGameOptions();
  mCardDriver->BuildNewFileSlot(idx);
  if (newGame) {
    mCardDriver->StartFileWriteTransactional();
  } else {
    mIowRet = CIOWin::kMR_Exit;
  }
}

void CSaveGameScreen::DoAdvance(CGuiTableGroup* caller) {
  // The selection is read from mTablegroupChoices, not from the parameter: retail's prologue is
  // `lwz r4,88(r3)` then `lwz r5,200(r4)` (0x8017C618/0x8017C620), i.e. it loads the member and
  // indexes that, so the `caller` argument is never read. `sfx` stays -1 unless an arm sets it,
  // which is why the SfxStart at the end is guarded by `sfx >= 0`. The two sound ids are
  // 132 (mNavConfirmSfx) for the arms that act and 140 (mNavBackSfx) for the arms that back
  // out - mNavMoveSfx at 136 is only used by DoSelectionChange.
  const int userSel = mTablegroupChoices->GetUserSelection();
  int sfx = -1;
  switch (mUiType) {
  // These four arms do nothing but must be written out: retail's dispatch is a 16-entry jump
  // table indexed by mUiType (0x8017C628, `cmplwi r0,15` then `lwzx`), with entries 0..3 all
  // pointing at the same end-of-switch address. Leave them out and MWCC range-checks instead
  // and emits a 12-entry table based at case 4.
  case kUIT_Empty:
  case kUIT_BusyReading:
  case kUIT_BusyWriting:
  case kUIT_BusyWritingInitial:
    break;
  case kUIT_NoCardFound:
  case kUIT_CardDamaged:
  case kUIT_WrongDevice:
  case kUIT_IncompatibleCard:
    if (userSel == 1) {
      if (mSaveCtx == kSC_InGame) {
        mIowRet = CIOWin::kMR_RemoveIOWinAndExit;
      } else {
        ContinueWithoutSaving();
      }
      sfx = mNavBackSfx;
    } else if (userSel == 0) {
      ResetCardDriver();
      sfx = mNavConfirmSfx;
    }
    break;
  case kUIT_NeedsFormatBroken:
  case kUIT_NeedsFormatEncoding:
    if (userSel == 1) {
      if (mSaveCtx == kSC_InGame) {
        mIowRet = CIOWin::kMR_RemoveIOWinAndExit;
      } else {
        ContinueWithoutSaving();
      }
      sfx = mNavBackSfx;
    } else if (userSel == 0) {
      ResetCardDriver();
      sfx = mNavConfirmSfx;
    } else if (userSel == 2) {
      mUiType = kUIT_AllDataWillBeLost;
      mUiTextDirty = true;
      sfx = mNavConfirmSfx;
    }
    break;
  case kUIT_InsufficientSpaceOKCheck:
    if (userSel == 1) {
      if (mSaveCtx == kSC_InGame) {
        mIowRet = CIOWin::kMR_RemoveIOWinAndExit;
      } else {
        ContinueWithoutSaving();
      }
      sfx = mNavBackSfx;
    } else if (userSel == 0) {
      ResetCardDriver();
      sfx = mNavConfirmSfx;
    } else if (userSel == 2) {
      if (mSaveCtx == kSC_InGame) {
        mUiType = kUIT_ProgressWillBeLost;
        mUiTextDirty = true;
        sfx = mNavConfirmSfx;
      } else {
        gpMain->SetManageCard(true);
      }
    }
    break;
  case kUIT_SaveCorrupt:
    if (userSel == 2) {
      mCardDriver->StartFileDeleteBad();
      sfx = mNavConfirmSfx;
    } else if (userSel == 1) {
      if (mSaveCtx == kSC_InGame) {
        mIowRet = CIOWin::kMR_RemoveIOWinAndExit;
      } else {
        ContinueWithoutSaving();
      }
      sfx = mNavBackSfx;
    } else if (userSel == 0) {
      ResetCardDriver();
      sfx = mNavConfirmSfx;
    }
    break;
  case kUIT_ProgressWillBeLost:
    if (userSel == 1) {
      gpMain->SetManageCard(true);
    } else if (userSel == 0) {
      mIowRet = CIOWin::kMR_RemoveIOWinAndExit;
      sfx = mNavBackSfx;
    }
    break;
  case kUIT_NotOriginalCard:
    if (userSel == 1) {
      mSerial = mCardDriver->GetCardSerial();
      mUiType = kUIT_Empty;
      mCardDriver->IndexFiles();
      sfx = mNavConfirmSfx;
    } else if (userSel == 0) {
      mIowRet = CIOWin::kMR_RemoveIOWinAndExit;
      sfx = mNavBackSfx;
    }
    break;
  case kUIT_AllDataWillBeLost:
    if (userSel == 1) {
      mCardDriver->StartCardFormat();
      mUiType = kUIT_Empty;
      sfx = mNavConfirmSfx;
    } else if (userSel == 0) {
      ResetCardDriver();
      sfx = mNavBackSfx;
    }
    break;
  case kUIT_SaveReady:
    if (mSaveCtx != kSC_FrontEnd) {
      if (userSel == 0) {
        mCardDriver->BuildExistingFileSlot(gpGameState->SystemOptions().GetSaveIdx());
        mCardDriver->StartFileWriteTransactional();
        sfx = mNavConfirmSfx;
      } else if (userSel == 1) {
        mIowRet = CIOWin::kMR_RemoveIOWinAndExit;
        sfx = mNavBackSfx;
      }
    }
    break;
  default:
    break;
  }
  if (sfx >= 0) {
    CSfxManager::SfxStart(sfx, 0x7f, 0x40, mFrontEndSfx ? 0 : CSfxManager::kAllAreas, mFrontEndSfx,
                          false, CSfxManager::kMedPriority);
  }
}

void CSaveGameScreen::DoSelectionChange(CGuiTableGroup* caller, int oldSelection) {
  SetUIColors();
  CSfxManager::SfxStart(mNavMoveSfx, 0x7f, 0x40, mFrontEndSfx ? 0 : CSfxManager::kAllAreas,
                        mFrontEndSfx, false, CSfxManager::kMedPriority);
}

void CSaveGameScreen::SetUIColors() {
  const CColor selected(0xffffffff);
  const CColor unselected(uchar(160), uchar(160), uchar(160), uchar(200));
  mTablegroupChoices->SetColors(selected, unselected);
}
