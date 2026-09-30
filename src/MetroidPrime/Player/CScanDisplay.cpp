#include "MetroidPrime/Player/CScanDisplay.hpp"

#include "GuiSys/CGuiTextPane.hpp"
#include "Kyoto/Audio/CSfxManager.hpp"
#include "Kyoto/Basics/CBasics.hpp"
#include "Kyoto/Math/CMath.hpp"
#include "Kyoto/Text/CStringTable.hpp"
#include "MetroidPrime/CEntity.hpp"
#include "MetroidPrime/CModelData.hpp"
#include "MetroidPrime/CStateManager.hpp"
#include "MetroidPrime/Player/CGameState.hpp"
#include "MetroidPrime/TCastTo.hpp"

class CScriptPointOfInterest;

// Structure-first scaffold. GUI, hint suppression and model presentation remain incomplete.

bool CScanDisplay::CScanTargetPredicate::IsValid(const CStateManager& mgr, TUniqueId id) const {
  if (id != mObject) {
    return false;
  }
  const CEntity* entity = mgr.GetObjectById(id);
  return TCastToConstPtr< CScriptPointOfInterest >(entity) != nullptr && entity->GetActive();
}

void CScanDisplay::SetScanMessageTypeEffect(CGuiTextPane* pane, bool type) {
  if (type) {
    pane->TextSupport().SetTypeWriteEffectOptions(true, 0.1f, 60.f);
  } else {
    pane->TextSupport().SetTypeWriteEffectOptions(false, 0.f, 0.f);
  }
}

CScanDisplay::CScanDisplay(const CGuiFrame* selHud)
: mDataDotTexture(gpSimplePool->GetObj("TXTR_DataDot"))
, mState(kSS_Inactive)
, mObject(kInvalidUniqueId)
, mSelHud(selHud)
, mTextGroup(nullptr)
, mMessage(nullptr)
, mScrollMessage(nullptr)
, mXMark(nullptr)
, mAButton(nullptr)
, mDash(nullptr)
, mHistoryRoot(nullptr)
, mHistoryRight(nullptr)
, mStartButton(nullptr)
, mPressStart(nullptr)
, mModelObject(kInvalidUniqueId)
, mModelBounds(CAABox::Identity())
, mStartPosition(CVector3f::Zero())
, mEndPosition(CVector3f::Zero())
, mStartRotation(CQuaternion::NoRotation())
, mEndRotation(CQuaternion::NoRotation())
, mModelRotation(CQuaternion::NoRotation())
, mStartScale(CVector3f::Zero())
, mEndScale(CVector3f::Zero())
, mModelTransition(0.f)
, mXAlpha(0.f)
, mBodyAlpha(0.f)
, mPageCounter(0)
, mAPulse(1.f)
, mModelYaw(0.f)
, mAPulseCount(0)
, mScanComplete(false)
, mHintsSuppressed(false)
, mPreparePending(false)
, mCanOpenLogbook(false) {}

CScanDisplay::~CScanDisplay() {
  // Retail 0x80116144: while the hints are suppressed, release the game state's scan-display byte
  // that StartScan 0x80115830 sets to 1, then drop our own flag. Update 0x80114B64 is the same
  // test, and `gpGameState` +0xD9 is the byte both of them write.
  if (mHintsSuppressed) {
    reinterpret_cast< char* >(gpGameState)[0xD9] = 0;
    mHintsSuppressed = false;
  }
}

void CScanDisplay::StartScan(TUniqueId uid, const CScannableObjectInfo& info, CGuiTextPane* message,
                             CGuiTextPane* scrollMessage, CGuiWidget* textGroup, CGuiWidget* xMark,
                             CGuiWidget* aButton, CGuiWidget* dash, CGuiWidget* startButton,
                             CGuiTextPane* pressStart, CGuiWidget* historyRoot,
                             CGuiWidget* historyRight,
                             const rstl::vector< SScanHierarchyNode >& history,
                             const rstl::vector< SScanHistoryWidgets >& widgets, float scanTime,
                             bool showText, const CStateManager& mgr) {
  mScanComplete = scanTime >= info.GetTotalDownloadTime();
  mObject = uid;
  mScannableInfo = info;
  mState = kSS_Downloading;
  mPageCounter = 0;
  mXAlpha = 0.f;
  mAPulseCount = 0;
  mCanOpenLogbook = false;
  mMessage = message;
  mScrollMessage = scrollMessage;
  mTextGroup = textGroup;
  mXMark = xMark;
  mAButton = aButton;
  mDash = dash;
  mStartButton = startButton;
  mPressStart = pressStart;

  if (info.GetStringTableId() != kInvalidAssetId) {
    mScanString = TCachedToken< CStringTable >(
        gpSimplePool->GetObj(SObjectTag('STRG', info.GetStringTableId())));
    mScanString->Lock();
  }
  if (!mgr.fn_80036F10()) {
    mHistoryRoot = historyRoot;
    mHistoryRight = historyRight;
    mHistory = history;
    mHistoryWidgets = widgets;
    mCategoryName = rstl::wstring_l(L"");
    mHistoryStrings.clear();
    mHistoryStrings.reserve(history.size());
    for (int i = 0; i < history.size(); ++i) {
      mHistoryStrings.push_back(TCachedToken< CStringTable >(
          gpSimplePool->GetObj(SObjectTag('STRG', history[i].mStringTable))));
    }
    for (int i = 0; i < mHistoryStrings.size(); ++i) {
      mHistoryStrings[i].Lock();
    }
    if (info.GetScanTextureId() != kInvalidAssetId) {
      mScanTexture = TCachedToken< CTexture >(
          gpSimplePool->GetObj(SObjectTag('TXTR', info.GetScanTextureId())));
    }
    if (info.UsesScanModel() && info.GetStaticModelId(0) != kInvalidAssetId) {
      mScanModelToken = TCachedToken< CModel >(
          gpSimplePool->GetObj(SObjectTag('CMDL', info.GetStaticModelId(0))));
      mScanModelToken->Lock();
    }
    // TODO: configure history widgets.
  }
  // TODO: apply showText, initialize GUI colors and acquire hint suppression.
}

void CScanDisplay::StopScan() {
  // Retail 0x801156D0 tests mState with `return` in every arm, which is what puts the two dead
  // `blr`s after the `bge`; `break` spells produce `bltlr` and an 8-byte-shorter body.
  switch (mState) {
  case kSS_Inactive:
  case kSS_Done:
    return;
  case kSS_Downloading:
  case kSS_DownloadComplete:
  case kSS_ViewingScan:
    mState = kSS_Done;
    return;
  default:
    return;
  }
}

void CScanDisplay::UpdateAPulse(float dt) {
  mAPulse += dt * (mAPulseCount < 3 ? 4.f : 2.f);
  if (mAPulse > 1.f) {
    mAPulse -= 2.f;
    if (mAPulseCount < 1) {
      CSfxManager::SfxStart(0x5aa, 127, 63);
    }
    ++mAPulseCount;
  }
}

void CScanDisplay::Update(float dt, float scanningTime, const CStateManager& mgr) {
  if (mState == kSS_Inactive) {
    mDataDotTexture.Unlock();
    mScanTexture.clear();
    mScanModelToken.clear();
    mScanModel = rstl::auto_ptr< CModelData >();
    return;
  }

  // Retail 0x80114CB4-0x80114D4C walks mHistoryStrings (one TCachedToken<CStringTable> per
  // history node) and resolves each token in turn: an already-cached token counts as resolved,
  // otherwise it is locked and the object's load state decides. The walk only falls through to
  // the text below when the last element resolved, so an unfinished token skips a frame's text.
  bool stringsLoaded = true;
  for (int i = 0; i < mHistoryStrings.size(); ++i) {
    TCachedToken< CStringTable >& token = mHistoryStrings[i];
    if (token.GetObject() != nullptr) {
      continue;
    }
    token.Lock();
    if (!token.GetToken().IsLoaded()) {
      stringsLoaded = false;
    }
  }

  if (stringsLoaded) {
    // Retail 0x80114D50: the line separator comes from the string table, then each history
    // widget's text pane takes its node's name and its number pane the completion percentage.
    mCategoryName = gpStringTable->GetString("LogbookLineSpacing");
    const int widgetCount = mHistoryWidgets.size();
    for (int i = 0; i < widgetCount && i < mHistory.size(); ++i) {
      CGuiTextPane* history = mHistoryWidgets[i].mHistory;
      if (history != nullptr) {
        history->TextSupport().SetText(mHistory[i].mName, false);
      }
      CGuiTextPane* number = mHistoryWidgets[i].mNumber;
      if (number != nullptr) {
        // Retail 0x80114C48 / 0x80115388: completed scans over total, as a percentage.
        const int total = mHistory[i].mTotalScans;
        const int completed = mHistory[i].mCompletedScans;
        const int percent = total != 0 ? completed * 100 / total : 0;
        number->TextSupport().SetText(rstl::string(CBasics::Stringize("%d%%", percent)), false);
      }
    }
  }

  // Retail 0x80114B64, the kSS_DownloadComplete arm at 0x8011524C: the scan finished
  // downloading rather than being read in full, so the message pane is rebuilt from the string
  // table's "DownloadedLogBookMsgLeftPart" entry, the category name and
  // "DownloadedLogBookMsgRightPart", then typed on and a logbook-open sound plays.
  if (mState == kSS_DownloadComplete) {
    rstl::wstring message(gpStringTable->GetString("DownloadedLogBookMsgLeftPart"), -1);
    message.append(mCategoryName);
    message.append(gpStringTable->GetString("DownloadedLogBookMsgRightPart"), -1);
    mMessage->TextSupport().SetText(message, false);
    SetScanMessageTypeEffect(mMessage, true);
    CSfxManager::SfxStart(0xdb0, 127, 64);
  }

  // TODO: update cached assets, scan/history colours, fades and model transitions; release
  // completed scans.
  (void)dt;
  (void)scanningTime;
  (void)mgr;
}

void CScanDisplay::ProcessInput(const CFinalInput&) {
  // TODO: advance typewriter text/pages and refresh A/Start/dash prompts through shared GUI types.
}

void CScanDisplay::Draw(const CStateManager&) const {
  // TODO: interpolate presentation transforms and draw world, actor or resource scan geometry.
}

float CScanDisplay::GetTotalDownloadTime() const {
  return mScannableInfo ? mScannableInfo->GetTotalDownloadTime() : 0.f;
}

void CScanDisplay::RequestScanDisplay() { mPreparePending = true; }

CScanDisplay::CScanTargetPredicate::~CScanTargetPredicate() {}

void CScanDisplay::PrepareScanDisplay(const CStateManager&, int) {
  // TODO: resolve scan geometry while soft-paused, accumulate its bounds and prepare camera
  // endpoints.
}

float CScanDisplay::GetDownloadStartTime(int historyIndex) const {
  const float duration = GetTotalDownloadTime();
  return mHistory.empty() ? 0.f : historyIndex * duration / mHistory.size();
}

float CScanDisplay::GetDownloadFraction(int historyIndex, float time) const {
  return CMath::Clamp(0.f, (time - GetDownloadStartTime(historyIndex)) * mHistory.size(), 1.f);
}
