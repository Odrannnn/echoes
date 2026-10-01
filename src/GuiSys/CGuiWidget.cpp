// mwcceppc's string pool for this unit, spelled out byte for byte as retail has it at 0x803AE7F8
// (`config/G2ME01/symbols.txt:17686`, `lbl_803AE7F8 = .rodata:0x803AE7F8; ... size:0x140`). Both
// of this unit's literals live in it: `ParseBaseInfo` warns at +0xE9 (233) and `Create` /
// `CreateGroup` hand `operator new` the "??(??" placement string at +0x132 (306). The eight
// `kGUIModelDrawFlags_*` names ahead of them belong to the same block but are referenced by
// nothing in this unit, so they are not interned from source; see the `CMEMORY_NEW_FILE` branch
// in `Kyoto/Alloc/CMemory.hpp` and the same treatment in `GuiSys/CGuiCamera.cpp`. Must precede
// every include: `CMEMORY_NEW_FILE` is expanded by `rs_new` wherever the header chain puts one.
extern "C" const char lbl_803AE7F8[];
#define CMEMORY_NEW_FILE (lbl_803AE7F8 + 0x132)

#include "GuiSys/CGuiWidget.hpp"

#include "GuiSys/CGuiFrame.hpp"
#include "Kyoto/Math/CMatrix3f.hpp"
#include "Kyoto/Streams/CInputStream.hpp"
#include <stdio.h>

extern "C" const char lbl_803AE7F8[] = {
    'k', 'G', 'U', 'I', 'M', 'o', 'd', 'e', 'l', 'D', 'r', 'a', 'w', 'F', 'l', 'a',
    'g', 's', '_', 'N', 'o', 'n', 'e', '\0', 'k', 'G', 'U', 'I', 'M', 'o', 'd', 'e',
    'l', 'D', 'r', 'a', 'w', 'F', 'l', 'a', 'g', 's', '_', 'R', 'G', 'B', 'M', 'o',
    'd', 'u', 'l', 'a', 't', 'e', '\0', 'k', 'G', 'U', 'I', 'M', 'o', 'd', 'e', 'l',
    'D', 'r', 'a', 'w', 'F', 'l', 'a', 'g', 's', '_', 'A', 'l', 'p', 'h', 'a', 'B',
    'l', 'e', 'n', 'd', '\0', 'k', 'G', 'U', 'I', 'M', 'o', 'd', 'e', 'l', 'D', 'r',
    'a', 'w', 'F', 'l', 'a', 'g', 's', '_', 'A', 'd', 'd', 'i', 't', 'i', 'v', 'e',
    'A', 'l', 'p', 'h', 'a', '\0', 'k', 'G', 'U', 'I', 'M', 'o', 'd', 'e', 'l', 'D',
    'r', 'a', 'w', 'F', 'l', 'a', 'g', 's', '_', 'T', 'w', 'o', 'P', 'a', 's', 's',
    'A', 'd', 'd', 'A', 'n', 'd', 'B', 'l', 'e', 'n', 'd', 'A', 'l', 'p', 'h', 'a',
    '\0', 'k', 'G', 'u', 'i', 'M', 'o', 'd', 'e', 'l', 'D', 'r', 'a', 'w', 'F', 'l',
    'a', 'g', 's', '_', 'D', 'r', 'a', 'w', 'T', 'o', 'A', 'l', 'p', 'h', 'a', 'B',
    'u', 'f', 'f', 'e', 'r', '\0', 'k', 'G', 'u', 'i', 'M', 'o', 'd', 'e', 'l', 'D',
    'r', 'a', 'w', 'F', 'l', 'a', 'g', 's', '_', '2', 'x', 'M', 'o', 'd', 'u', 'l',
    'a', 't', 'e', 'S', 'o', 'l', 'i', 'd', '\0', 'W', 'a', 'r', 'n', 'i', 'n', 'g',
    ':', ' ', 'D', 'i', 's', 'c', 'a', 'r', 'd', 'i', 'n', 'g', ' ', 'u', 's', 'e',
    'l', 'e', 's', 's', ' ', 'w', 'o', 'r', 'k', 'e', 'r', ' ', 'i', 'd', '.', ' ',
    ' ', 'P', 'a', 'r', 'e', 'n', 't', ' ', 'i', 's', ' ', 'n', 'o', 't', ' ', 'a',
    ' ', 'c', 'o', 'm', 'p', 'o', 'u', 'n', 'd', ' ', 'w', 'i', 'd', 'g', 'e', 't',
    '.', '\0', '?', '?', '(', '?', '?', ')', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
};
CGuiWidget::CGuiWidgetParms::CGuiWidgetParms(CGuiFrame* frame, short selfId, short parentId,
                                             const CColor& color, EGuiModelDrawFlags drawFlags,
                                             bool cullFaces, bool defaultVisible,
                                             bool defaultActive, bool depthTest, bool depthWrite,
                                             bool depthGreater)
: mFrame(frame)
, mSelfId(selfId)
, mParentId(parentId)
, mColor(color)
, mDrawFlags(drawFlags)
, mCullFaces(cullFaces)
, mDefaultVisible(defaultVisible)
, mDefaultActive(defaultActive)
, mDepthTest(depthTest)
, mDepthWrite(depthWrite)
, mDepthGreater(depthGreater) {}

CGuiWidget* CGuiWidget::Create(CGuiFrame* frame, CInputStream& in, CSimplePool* pool,
                               uint version) {
  const CGuiWidgetParms parms = ReadWidgetHeader(frame, in);
  CGuiWidget* widget = rs_new CGuiWidget(parms);
  widget->ParseBaseInfo(frame, in, parms, version);
  return widget;
}

CGuiWidget* CGuiWidget::CreateGroup(CGuiFrame* frame, CInputStream& in, CSimplePool* pool,
                                    uint version) {
  const CGuiWidgetParms parms = ReadWidgetHeader(frame, in);
  in.ReadInt16();
  in.ReadBool();

  CGuiWidget* widget = rs_new CGuiWidget(parms);
  widget->ParseBaseInfo(frame, in, parms, version);
  return widget;
}

// The two spellings below are what mwcceppc emits retail's instructions for, and both are
// measured, not guessed (unit 94.14% -> 99.08% fuzzy, 21/25 -> 23/25 functions matched):
//   - `selfId`/`parentId` are plain `short`, not `const short`. The const spelling makes
//     mwcceppc materialise each id with an `extsh` it does not emit for retail (which just
//     `mr`s the returned value straight into the argument register): 97.69% -> 57.13%.
//   - the three flags are read as `in.ReadUint8() ? true : false` rather than through
//     `in.ReadBool()`. `ReadBool` is `ReadUint8() != 0`, whose `bool` result mwcceppc keeps as a
//     byte and sign-extends (`clrlwi x,y,24`) at each use, deferring the `neg/or` normalisation
//     past the `CColor` constructor call; retail normalises each one at the read site and never
//     sign-extends. 97.69% -> 63.36%.
CGuiWidget::CGuiWidgetParms CGuiWidget::ReadWidgetHeader(CGuiFrame* frame, CInputStream& in) {
  rstl::string name(in);
  short selfId = frame->WidgetIdDB().AddWidget(name);
  rstl::string parent(in);
  short parentId = frame->WidgetIdDB().AddWidget(parent);

  in.ReadUint8();
  bool visible = in.ReadUint8() ? true : false;
  bool active = in.ReadUint8() ? true : false;
  bool cullFaces = in.ReadUint8() ? true : false;
  CColor color(in);
  EGuiModelDrawFlags drawFlags = static_cast< EGuiModelDrawFlags >(in.ReadInt32());
  return CGuiWidgetParms(frame, selfId, parentId, color, drawFlags, cullFaces, visible, active,
                         true, false, false);
}

CGuiWidget::CGuiWidget(const CGuiWidgetParms& parms)
: mSelfId(parms.mSelfId)
, mParentId(parms.mParentId)
, mTransform(CTransform4f::Identity())
, mColor(parms.mColor)
, mColor2(mColor)
, mDrawFlags(parms.mDrawFlags)
, mFrame(parms.mFrame)
, mWorkerId(-1)
, mIsVisible(parms.mDefaultVisible)
, mIsActive(parms.mDefaultActive)
, mIsSelectable(true)
, mEventLock(false)
, mCullFaces(parms.mCullFaces)
, mDepthGreater(parms.mDepthGreater)
, mDepthTest(parms.mDepthTest)
, mDepthWrite(parms.mDepthWrite)
, xbb_24_(true) {
  RecalcWidgetColor(kTM_Single);
}

CGuiWidget::~CGuiWidget() {}

void CGuiWidget::ParseBaseInfo(CGuiFrame* frame, CInputStream& in, const CGuiWidgetParms& parms,
                               uint version) {
  CGuiWidget* parent = frame->FindWidget(parms.mParentId);
  bool isWorker = in.ReadBool();
  if (isWorker) {
    mWorkerId = in.ReadInt16();
  }

  CVector3f translation(in);
  CMatrix3f orientation(in);
  SetIdleXform(CTransform4f(orientation, translation));
  if (version < 2) {
    CVector3f unused(in);
    ReadUnusedThing(in);
    in.ReadInt16();
  }

  if (parent != nullptr) {
    if (isWorker && !parent->AddWorkerWidget(this)) {
      // At +0xE9 of the pool spelled out at the top of this file; spelling the address keeps the
      // literal in the block retail's object names, which is what this `printf` call's
      // `R_PPC_ADDR16_HA/LO` pair is relocated against (93.35% -> 94.62% for this function).
      printf(lbl_803AE7F8 + 0xE9);
      mWorkerId = -1;
    }
    parent->AddChildWidget(this, false, true);
  }
}

void CGuiWidget::ReadUnusedThing(CInputStream& in) { in.ReadInt32(); }

void CGuiWidget::Draw(const CGuiWidgetDrawParms& parms) const {}

void CGuiWidget::ProcessUserInput(const CFinalInput& input) {}

void CGuiWidget::Update(float dt) {}

void CGuiWidget::DispatchInitialize() {
  Initialize();
  if (ChildObject() != nullptr) {
    static_cast< CGuiWidget* >(ChildObject())->DispatchInitialize();
  }
  if (NextSibling() != nullptr) {
    static_cast< CGuiWidget* >(NextSibling())->DispatchInitialize();
  }
}

CGuiWidget* CGuiWidget::FindWidget(short id) {
  if (mSelfId == id) {
    return this;
  }
  if (ChildObject() != nullptr) {
    CGuiWidget* found = static_cast< CGuiWidget* >(ChildObject())->FindWidget(id);
    if (found != nullptr) {
      return found;
    }
  }
  if (NextSibling() != nullptr) {
    CGuiWidget* found = static_cast< CGuiWidget* >(NextSibling())->FindWidget(id);
    if (found != nullptr) {
      return found;
    }
  }
  return nullptr;
}

void CGuiWidget::SetColor(const CColor& color) {
  if (!(mColor == color)) {
    mColor = color;
    RecalcWidgetColor(kTM_Children);
  }
}

void CGuiWidget::RecalcWidgetColor(ETraversalMode mode) {
  CGuiWidget* parent = static_cast< CGuiWidget* >(Parent());
  if (parent != nullptr) {
    mColor2 = CColor::Modulate(mColor, parent->GetModifiedColor());
  } else {
    mColor2 = mColor;
  }

  switch (mode) {
  case kTM_Single:
    break;
  case kTM_ChildrenAndSiblings:
    if (NextSibling() != nullptr) {
      static_cast< CGuiWidget* >(NextSibling())->RecalcWidgetColor(kTM_ChildrenAndSiblings);
    }
  case kTM_Children:
    if (ChildObject() != nullptr) {
      static_cast< CGuiWidget* >(ChildObject())->RecalcWidgetColor(kTM_ChildrenAndSiblings);
    }
    break;
  }
}

void CGuiWidget::SetVisibility(const bool visible, ETraversalMode mode) {
  switch (mode) {
  case kTM_Single:
    break;
  case kTM_Children:
    if (ChildObject() != nullptr) {
      static_cast< CGuiWidget* >(ChildObject())->SetVisibility(visible, kTM_ChildrenAndSiblings);
    }
    break;
  case kTM_ChildrenAndSiblings:
    if (ChildObject() != nullptr) {
      static_cast< CGuiWidget* >(ChildObject())->SetVisibility(visible, kTM_ChildrenAndSiblings);
    }
    if (NextSibling() != nullptr) {
      static_cast< CGuiWidget* >(NextSibling())->SetVisibility(visible, kTM_ChildrenAndSiblings);
    }
    break;
  }
  SetIsVisible(visible);
}

void CGuiWidget::AddChildWidget(CGuiWidget* widget, bool makeWorldLocal, bool atEnd) {
  AddChildObject(widget, makeWorldLocal, atEnd);
}

CVector3f CGuiWidget::GetIdlePosition() const { return mTransform.GetTranslation(); }

void CGuiWidget::ReapplyXform() {
  RotateReset();
  SetLocalPosition(CVector3f::Zero());
  MultiplyO2P(mTransform);
}

void CGuiWidget::SetIsVisible(bool visible) {
  mIsVisible = visible;
  OnVisible();
}

void CGuiWidget::SetIsActive(const bool active) {
  if (mIsActive != active) {
    mIsActive = active;
    OnActivate();
  }
}

void CGuiWidget::OnVisible() {}

void CGuiWidget::OnActivate() {}

void CGuiWidget::SetIdleXform(const CTransform4f& xf, bool reapply) {
  mTransform = xf;
  if (reapply) {
    ReapplyXform();
  }
}

CGuiWidget* CGuiWidget::GetWorkerWidget(int workerId) {
  const CGuiWidget* widget = static_cast< const CGuiWidget* >(GetChildObject());
  while (widget != nullptr) {
    if (widget->GetWorkerId() == workerId) {
      break;
    }
    widget = static_cast< const CGuiWidget* >(widget->GetNextSibling());
  }
  return const_cast< CGuiWidget* >(widget);
}
