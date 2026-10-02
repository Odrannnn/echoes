#include "Kyoto/Text/CTextExecuteBuffer.hpp"

#include "Kyoto/Math/CVector2i.hpp"
#include "Kyoto/Text/CBlockInstruction.hpp"
#include "Kyoto/Text/CCharacterExtraSpaceInstruction.hpp"
#include "Kyoto/Text/CColorInstruction.hpp"
#include "Kyoto/Text/CColorOverrideInstruction.hpp"
#include "Kyoto/Text/CFontInstruction.hpp"
#include "Kyoto/Text/CImageInstruction.hpp"
#include "Kyoto/Text/CLineExtraSpaceInstruction.hpp"
#include "Kyoto/Text/CLineInstruction.hpp"
#include "Kyoto/Text/CLineSpacingInstruction.hpp"
#include "Kyoto/Text/CPopStateInstruction.hpp"
#include "Kyoto/Text/CPushStateInstruction.hpp"
#include "Kyoto/Text/CRemoveColorOverrideInstruction.hpp"
#include "Kyoto/Text/CFontRenderState.hpp"
#include "Kyoto/Text/CTextInstruction.hpp"
#include "Kyoto/Text/CTextRenderBuffer.hpp"
#include "Kyoto/Text/CWordBreakTables.hpp"
#include "Kyoto/Text/CWordInstruction.hpp"
#include "rstl/math.hpp"

// 110, not the project's 125 (configure.py:283). It is what mwceppc's inliner measures a callee
// against, and retail's own threshold for this unit is lower: only `create_node` crosses it.
//
// `rstl::list<rstl::ncrc_ptr<CInstruction> >::create_node` is an out-of-line retail symbol at
// 0x802B7A24, 120 bytes, and `do_insert_before` (0x802B79B4) *calls* it - `bl` at 0x802B79DC into
// a 112-byte body. At 125 mwceppc inlines it instead, so our `do_insert_before` is 176 bytes
// against retail's 112 (33.29%) and the 120-byte symbol is absent from our object altogether
// (`fuzzy_match_percent` is missing from the report entry, which is not the same as 0%). At 110 it
// is emitted out of line and called: `do_insert_before` 33.29% -> 100.00%, `create_node` absent ->
// 100.00%, and the unit goes 40/46 -> 42/46.
//
// Measured, whole unit, the other 42 functions unmoved in every row: 125 (the project default) and
// 120, 119, 118, 117, 116, 115, 114, 113, 112, 111 -> 40/46; 110, 105, 100, 95, 90, 85, 80, 75, 70,
// 65 -> 42/46; 60 -> 36/46. So mwceppc's estimate for `create_node` falls between 110 and 111, and
// every value from 65 to 110 keeps it out of `do_insert_before` without moving anything else. 110 is
// the one taken because it is the least perturbation of the project's 125; below 65 a second
// callee's estimate crosses too, and 60 is already worse.
//
// This is a file-scoped pragma, which mwceppc takes as the file's value (see the same lever in
// `src/MetroidPrime/CGameCollision.cpp:30`), so it can only move this translation unit - and the
// unit is `NonMatching`, so its object is not in the link. It is the scoping that matters: the
// equivalent change in `include/rstl/list.hpp`'s `create_node` (four `RSTL_PRECONDITION`s, which
// are `((void)0)` and emit nothing but count toward the same estimate; measured in
// docs/goal-notes/progress-prime1-ctextexecutebuffer.md) gets both symbols byte-for-byte and moves
// *every* `rstl::list` in the tree, which breaks `Kyoto/Particles/CParticleDataFactory.cpp` - a
// `Matching` unit where retail *inlines* `create_node` for `list<CElementAllocationChunk>` - with
// "87 computed checksum(s) did NOT match".
#pragma inline_max_size(110)

CTextExecuteBuffer::CTextExecuteBuffer()
: mCurrentBlock(nullptr)
, mCurrentLine(nullptr)
, mCurrentWord(mInstructions.end())
, mCurrentWordX(0)
, mCurrentWordY(0)
, mSpaceDistance(0)
, mImageBaseline(false) {}

void CTextExecuteBuffer::Clear() {
  mInstructions.clear();
  mState = CSaveableState();
  mCurrentBlock = nullptr;
  mCurrentLine = nullptr;
  mCurrentWord = mInstructions.end();
  mCurrentWordX = 0;
  mCurrentWordY = 0;
  mSpaceDistance = 0;
}

void CTextExecuteBuffer::BeginBlock(int x, int y, int width, int height, bool imageBaseline,
                                    ETextDirection direction, EJustification justification,
                                    EVerticalJustification verticalJustification) {
  mImageBaseline = imageBaseline;
  const rstl::ncrc_ptr< CInstruction > instruction = rs_new CBlockInstruction(
      x, y, width, height, direction, justification, verticalJustification);
  mCurrentBlock = static_cast< CBlockInstruction* >(instruction.GetPtr());
  if (mState.IsFinishedLoading()) {
    mCurrentBlock->TestLargestFont(mState.GetFont()->GetMonoWidth(),
                                   mState.GetFont()->GetCarriageAdvance(),
                                   mState.GetFont()->GetBaseLine());
  }
  Add(instruction);
  mState.GetOptions().SetTextDirection(direction);
  mState.SetJustification(justification);
  mState.SetVerticalJustification(verticalJustification);
}

void CTextExecuteBuffer::EndBlock() {
  if (mCurrentLine) {
    TerminateLine(true);
  }
  mCurrentLine = nullptr;
  mCurrentBlock = nullptr;
}

void CTextExecuteBuffer::AddFont(const TToken< CRasterFont >& font) {
  const rstl::ncrc_ptr< CInstruction > instruction = rs_new CFontInstruction(font);
  Add(instruction);
  mState.SetFont(font);
  if (font.IsLoaded()) {
    if (mCurrentBlock) {
      mCurrentBlock->TestLargestFont(mState.GetFont()->GetMonoWidth(),
                                     mState.GetFont()->GetCarriageAdvance(),
                                     mState.GetFont()->GetBaseLine());
    }
    if (mCurrentLine) {
      mCurrentLine->TestLargestFont(mState.GetFont()->GetMonoWidth(),
                                    mState.GetFont()->GetCarriageAdvance(),
                                    mState.GetFont()->GetBaseLine());
    }
  }
}

// Retail compiles these two CFontImageDef accessors into this translation unit (0x802B8920 and
// 0x802B889C, between AddFont and AddImage), so they are defined here rather than in
// Kyoto/Text/CFontImageDef.cpp, whose object is already in the link and byte-exact.
int CFontImageDef::GetWidth() const {
  TToken< CTexture > tex = mTextures[0];
  return tex->GetWidth() * mCropFactor.GetX();
}

int CFontImageDef::GetHeight() const {
  TToken< CTexture > tex = mTextures[0];
  return tex->GetHeight() * mCropFactor.GetY();
}

void CTextExecuteBuffer::AddImage(const CFontImageDef& image) {
  if (!mCurrentLine) {
    StartNewLine();
  }
  if (mCurrentBlock && image.IsLoaded()) {
    // Retail keeps two bools here: `tooWide` for the width test and `wrap` for the word count
    // test, and only calls StartNewLine on the second.
    bool wrap = false;
    bool tooWide = wrap;
    if (mState.IsWordWrapping() &&
        mCurrentLine->GetWidth() + image.GetWidth() > mCurrentBlock->GetOutputWidth()) {
      tooWide = true;
    }
    if (tooWide && mCurrentLine->GetWordCount() > 0) {
      wrap = true;
    }
    if (wrap) {
      StartNewLine();
    }
    mCurrentLine->TestLargestImage(image.GetMonoWidth(), image.GetHeight(),
                                   image.CalculateBaseline());
    if (mCurrentBlock->GetTextDirection() == kTD_Horizontal) {
      mCurrentLine->AddWidth(image.GetWidth());
      if (mCurrentLine->GetWidth() > image.GetWidth()) {
        mCurrentBlock->SetWidth(mCurrentLine->GetWidth());
      }
    }
  }
  const rstl::ncrc_ptr< CInstruction > instruction = rs_new CImageInstruction(image);
  Add(instruction);
}

void CTextExecuteBuffer::AddColor(EColorType type, const CTextColor& color) {
  Add(rs_new CColorInstruction(type, color));
}

void CTextExecuteBuffer::AddColorOverride(int index, const CTextColor& color) {
  Add(rs_new CColorOverrideInstruction(index, color));
}

void CTextExecuteBuffer::AddRemoveColorOverride(int index) {
  Add(rs_new CRemoveColorOverrideInstruction(index));
}

void CTextExecuteBuffer::AddLineSpacing(float spacing) {
  const rstl::ncrc_ptr< CInstruction > instruction = rs_new CLineSpacingInstruction(spacing);
  Add(instruction);
  mState.SetLineSpacing(spacing);
}

void CTextExecuteBuffer::AddLineExtraSpace(int spacing) {
  const rstl::ncrc_ptr< CInstruction > instruction = rs_new CLineExtraSpaceInstruction(spacing);
  Add(instruction);
  mState.SetLineExtraSpace(spacing);
}

void CTextExecuteBuffer::AddCharacterExtraSpace(int spacing) {
  const rstl::ncrc_ptr< CInstruction > instruction =
      rs_new CCharacterExtraSpaceInstruction(spacing);
  Add(instruction);
  mState.GetOptions().SetCharacterExtraSpace(spacing);
}

void CTextExecuteBuffer::AddJustification(EJustification justification) {
  mState.SetJustification(justification);
  if (mCurrentLine && mCurrentLine->GetWidth() == 0) {
    mCurrentLine->SetJustification(justification);
  }
}

void CTextExecuteBuffer::AddVerticalJustification(EVerticalJustification justification) {
  mState.SetVerticalJustification(justification);
  if (mCurrentLine && mCurrentLine->GetWidth() == 0) {
    mCurrentLine->SetVerticalJustification(justification);
  }
}

void CTextExecuteBuffer::AddPushState() {
  const rstl::ncrc_ptr< CInstruction > instruction = rs_new CPushStateInstruction();
  Add(instruction);
  mStateStack.push_front(mState);
}

void CTextExecuteBuffer::AddPopState() {
  const rstl::ncrc_ptr< CInstruction > instruction = rs_new CPopStateInstruction();
  Add(instruction);
  mState = mStateStack.front();
  mStateStack.pop_front();
  if (mCurrentLine->GetWidth() == 0) {
    mCurrentLine->SetJustification(mState.GetJustification());
    mCurrentLine->SetVerticalJustification(mState.GetVerticalJustification());
  }
}

void CTextExecuteBuffer::TerminateLineLTR(bool lastLine) {
  if (mCurrentLine->GetY() == 0 && mState.IsFinishedLoading()) {
    mCurrentLine->SetHeight(
        rstl::max_val(mState.GetFont()->GetCarriageAdvance(), mCurrentLine->GetHeight()));
  }
  mCurrentBlock->AddHeight(
      mCurrentBlock->GetVerticalJustification() == kVerticalJustification_Full || lastLine
          ? mCurrentLine->GetY()
          : mState.GetLineExtraSpacing() +
                static_cast< int >(mCurrentLine->GetY() * mState.GetLineSpacing()));
}

void CTextExecuteBuffer::TerminateLine(bool lastLine) {
  if (mCurrentBlock->GetTextDirection() == kTD_Horizontal) {
    TerminateLineLTR(lastLine);
  }
}

void CTextExecuteBuffer::StartNewWord() {
  const rstl::ncrc_ptr< CInstruction > instruction = rs_new CWordInstruction();
  mCurrentWord = Add(instruction);
  mCurrentX = 0;
  mCurrentY = 0;
  mCurrentWordX = mCurrentLine->GetWidth();
  mCurrentWordY = mCurrentLine->GetY();
  mCurrentLine->IncWords();
}

void CTextExecuteBuffer::StartNewLine() {
  if (mCurrentLine) {
    TerminateLine(false);
  }
  // Two spellings here are not redundant, both measured against retail's bytes:
  // `rstl::ncrc_ptr<CInstruction>(...)` is what makes retail's copy of the temporary appear
  // (without it this function is 87.76% rather than 100%), and reading mImageBaseline through a
  // dereferenced pointer is what loads it *first* among the six constructor arguments. A bare
  // `mImageBaseline` is loaded last (75.66% / 90.20% before the copy fix), and any explicit
  // conversion - `static_cast<bool>`, `? true : false`, `!= 0`, `!!` - loads it first but
  // normalises it with `neg/or/srwi`, which retail does not do.
  const rstl::ncrc_ptr< CInstruction > instruction =
      rstl::ncrc_ptr< CInstruction >(rs_new CLineInstruction(
          0, 0, 0, mState.GetJustification(), mState.GetVerticalJustification(),
          *static_cast< const bool * >(&mImageBaseline)));
  mCurrentWord = Add(instruction);
  mCurrentLine = static_cast< CLineInstruction* >(instruction.GetPtr());
  mSpaceDistance = 0;
  StartNewWord();
  mCurrentBlock->IncLines();
}

CLineInstruction::CLineInstruction(int words, int width, int height, EJustification justification,
                                   EVerticalJustification verticalJustification,
                                   bool imageBaseline)
  : mWordCount(words)
  , mCurrentX(width)
  , mCurrentY(height)
  , mLargestFontHeight(0)
  , mLargestFontWidth(0)
  , mLargestFontBaseline(0)
  , mLargestImageHeight(0)
  , mLargestImageWidth(0)
  , mLargestImageBaseline(0)
  , mJustification(justification)
  , mVerticalJustification(verticalJustification)
  , mImageBaseline(imageBaseline) {}

void CTextExecuteBuffer::MoveWordLTR() {
  mCurrentLine->SubWidth(mCurrentX + mSpaceDistance);
  if (mCurrentLine->GetY() > mCurrentWordY) {
    mCurrentLine->SetHeight(mCurrentWordY);
  }
  mSpaceDistance = 0;
  mCurrentLine->DecWords();
  TerminateLineLTR(false);

  const rstl::ncrc_ptr< CInstruction > instruction =
      rs_new CLineInstruction(1, mCurrentX, mCurrentY, mState.GetJustification(),
                              mState.GetVerticalJustification(),
                              *static_cast< const bool * >(&mImageBaseline));
  mCurrentLine = static_cast< CLineInstruction* >(instruction.GetPtr());
  mInstructions.insert(mCurrentWord, instruction);
  mInstructions.insert(mCurrentWord, rs_new CWordInstruction());
  mCurrentBlock->IncLines();
}

int CTextExecuteBuffer::WrapOneLTR(const wchar_t* str, int len) {
  int rem = len;
  if (mState.IsFinishedLoading()) {
    int width, height;
    mState.GetFont()->GetSize(mState.GetOptions(), width, height, str, len);
    if (mState.IsWordWrapping()) {
      if (width + mCurrentLine->GetWidth() > mCurrentBlock->GetOutputWidth() &&
          mCurrentLine->GetWordCount() > 1 && mCurrentX + width < mCurrentBlock->GetOutputWidth()) {
        MoveWordLTR();
      }
      if (width + mCurrentLine->GetWidth() > mCurrentBlock->GetOutputWidth() && len > 1) {
        // Retail starts the search at an estimate of how many characters still fit in the
        // remaining space rather than at `len`, so a long string does not measure glyph by glyph.
        rem = rstl::min_val(
            len, (mCurrentBlock->GetOutputWidth() - mCurrentLine->GetWidth()) /
                     mState.GetFont()->GetMonoWidth() * 2);
        rem = rstl::max_val(1, rem);
        int rank = 5;
        do {
          --rem;
          int endRank = rem > 1 ? CWordBreakTables::GetEndRank(str[rem - 1]) : 4;
          int beginRank = CWordBreakTables::GetBeginRank(str[rem]);
          if (endRank < rank && endRank <= beginRank) {
            rank = endRank;
          } else if (beginRank < rank && beginRank <= endRank) {
            rank = endRank;
          } else {
            mState.GetFont()->GetSize(mState.GetOptions(), width, height, str, rem);
          }
        } while (width + mCurrentLine->GetWidth() > mCurrentBlock->GetOutputWidth() && rem > 1);
      }
    }
    if (mState.GetFont()->GetCarriageAdvance() > mCurrentY) {
      mCurrentY = mState.GetFont()->GetCarriageAdvance();
    }
    mCurrentLine->TestLargestFont(mState.GetFont()->GetMonoWidth(),
                                 mState.GetFont()->GetCarriageAdvance(),
                                 mState.GetFont()->GetBaseLine());
    mCurrentLine->AddWidth(width);
    if (mCurrentLine->GetWidth() > mCurrentBlock->GetLineX()) {
      mCurrentBlock->SetWidth(mCurrentLine->GetWidth());
    }
    mCurrentX += width;
    const rstl::ncrc_ptr< CInstruction > instruction = CTextInstruction::Create(str, rem);
    Add(instruction);
    if (rem != len) {
      StartNewLine();
    }
  }
  return rem;
}

void CTextExecuteBuffer::AddStringFragment(const wchar_t* str, int len) {
  int consumed = 0;
  if (mCurrentBlock->GetTextDirection() == kTD_Horizontal) {
    while (consumed != len) {
      consumed += WrapOneLTR(str + consumed, len - consumed);
    }
  }
}

void CTextExecuteBuffer::AddString(const wchar_t* str, int len) {
  if (!mCurrentLine) {
    StartNewLine();
  }
  int wordStart = 0;
  int i = 0;
  for (; str[i] && (i < len || len == -1); ++i) {
    if (str[i] == L'\n' || str[i] == L' ') {
      AddStringFragment(str + wordStart, i - wordStart);
      wordStart = i + 1;
      if (str[i] == L'\n') {
        StartNewLine();
      } else {
        StartNewWord();
        int width = 0;
        int height = 0;
        if (mState.IsFinishedLoading()) {
          wchar_t space = L' ';
          mState.GetFont()->GetSize(mState.GetOptions(), width, height, &space, 1);
        }
        if (mCurrentBlock->GetTextDirection() == kTD_Horizontal) {
          mCurrentLine->AddWidth(width);
          mSpaceDistance = width;
        } else {
          mCurrentLine->AddHeight(height);
          mSpaceDistance = height;
        }
      }
    }
  }
  if (i > wordStart) {
    AddStringFragment(str + wordStart, i - wordStart);
  }
}

rstl::vector< CToken > CTextExecuteBuffer::GetAssets() const {
  int count = 0;
  for (InstList::const_iterator it = mInstructions.begin(); it != mInstructions.end(); ++it) {
    count += (*it)->GetAssetCount();
  }

  rstl::vector< CToken > assets;
  if (count > 0) {
    assets.reserve(count);
    for (InstList::const_iterator it = mInstructions.begin(); it != mInstructions.end(); ++it) {
      (*it)->GetAssets(assets);
    }
  }
  return assets;
}

CTextRenderBuffer CTextExecuteBuffer::BuildRenderBuffer() const {
  CTextRenderBuffer buffer(CTextRenderBuffer::kM_AllocTally);
  {
    CFontRenderState state;
    for (InstList::const_iterator it = mInstructions.begin(); it != mInstructions.end(); ++it) {
      (*it)->Invoke(state, &buffer);
    }
  }
  buffer.SetMode(CTextRenderBuffer::kM_BufferFill);
  {
    CFontRenderState state;
    for (InstList::const_iterator it = mInstructions.begin(); it != mInstructions.end(); ++it) {
      (*it)->Invoke(state, &buffer);
    }
  }
  return buffer;
}

CTextRenderBuffer CTextExecuteBuffer::BuildRenderBufferPage(InstList::const_iterator start,
                                                            InstList::const_iterator pageStart,
                                                            InstList::const_iterator pageEnd) {
  CTextRenderBuffer buffer(CTextRenderBuffer::kM_AllocTally);
  {
    CFontRenderState state;
    for (InstList::const_iterator it = start; it != pageStart; ++it) {
      (*it)->PageInvoke(state, &buffer);
    }
    for (InstList::const_iterator it = pageStart; it != pageEnd; ++it) {
      (*it)->Invoke(state, &buffer);
    }
  }
  buffer.SetMode(CTextRenderBuffer::kM_BufferFill);
  {
    CFontRenderState state;
    for (InstList::const_iterator it = start; it != pageStart; ++it) {
      (*it)->PageInvoke(state, &buffer);
    }
    for (InstList::const_iterator it = pageStart; it != pageEnd; ++it) {
      (*it)->Invoke(state, &buffer);
    }
  }
  return buffer;
}

rstl::list< CTextRenderBuffer >
CTextExecuteBuffer::BuildRenderBufferPages(const CVector2i& extent) const {
  rstl::list< CTextRenderBuffer > pages;
  InstList::const_iterator it = mInstructions.begin();
  while (it != mInstructions.end()) {
    CTextRenderBuffer buffer(CTextRenderBuffer::kM_AllocTally);
    {
      CFontRenderState state;
      for (InstList::const_iterator it2 = mInstructions.begin(); it2 != mInstructions.end();
           ++it2) {
        (*it2)->Invoke(state, &buffer);
      }
    }
    buffer.SetMode(CTextRenderBuffer::kM_BufferFill);
    CFontRenderState state;
    InstList::const_iterator pageEnd = it;
    bool seeking = true;
    for (InstList::const_iterator it2 = mInstructions.begin(); it2 != mInstructions.end(); ++it2) {
      if (it2 == it) {
        seeking = false;
      }
      if (seeking) {
        (*it2)->PageInvoke(state, &buffer);
      } else {
        (*it2)->Invoke(state, &buffer);
        if ((*it2)->IsLineInstruction() && state.GetY() > extent.GetY()) {
          break;
        }
        ++pageEnd;
      }
    }
    pages.push_back(BuildRenderBufferPage(mInstructions.begin(), it, pageEnd));
    it = pageEnd;
  }
  return pages;
}
