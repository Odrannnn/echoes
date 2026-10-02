#ifndef _CTEXTEXECUTEBUFFER
#define _CTEXTEXECUTEBUFFER

#include "rstl/list.hpp"
#include "rstl/rc_ptr.hpp"
#include "rstl/string.hpp"

#include "Kyoto/Text/CSaveableState.hpp"

class CInstruction;
class CBlockInstruction;
class CLineInstruction;
class CFontImageDef;
class CTextRenderBuffer;
class CVector2i;

class CTextExecuteBuffer {
  typedef rstl::list< rstl::ncrc_ptr< CInstruction > > InstList;

public:
  CTextExecuteBuffer();

  CTextRenderBuffer BuildRenderBuffer() const;
  rstl::list< CTextRenderBuffer > BuildRenderBufferPages(const CVector2i& extent) const;
  rstl::vector< CToken > GetAssets() const;

  void AddFont(const TToken< CRasterFont >& font);
  void AddImage(const CFontImageDef& image);
  void AddColor(EColorType type, const CTextColor& color);
  void AddColorOverride(int index, const CTextColor& color);
  void AddRemoveColorOverride(int index);
  void AddLineSpacing(float spacing);
  void AddLineExtraSpace(int spacing);
  void AddCharacterExtraSpace(int spacing); // Guessed name
  void AddJustification(EJustification justification);
  void AddVerticalJustification(EVerticalJustification justification);
  void AddWordWrapping(bool wrap) { mState.SetWordWrapping(wrap); }
  void AddPushState();
  void AddPopState();
  void AddString(const wchar_t* str, int len);
  void AddString(const rstl::wstring& str) { AddString(str.data(), str.size()); }

  void BeginBlock(int x, int y, int width, int height, bool imageBaseline, ETextDirection direction,
                  EJustification justification, EVerticalJustification verticalJustification);
  void EndBlock();
  void Clear();

private:
  static CTextRenderBuffer BuildRenderBufferPage(InstList::const_iterator start,
                                                 InstList::const_iterator pageStart,
                                                 InstList::const_iterator pageEnd);
  // `rstl::advance` and `rstl::advance_iterator` are not the same shape to mwceppc, and retail
  // 0x802B791C (112 bytes) uses the second: three live-by-value copies of `begin()` (a dead one at
  // 0x14(r1), a dead one at 0x18(r1) and the returned one at 0x10(r1)) around one out-of-line
  // `rstl::__advance` call, and a 0x30 frame. `rstl::advance` takes the iterator by reference, so
  // only the last copy exists and the function is 23 instructions in a 0x20 frame (92.18%). The
  // `inline` on `advance_iterator` in rstl/iterator.hpp is what lets mwceppc inline it into this
  // body while still leaving `__advance` out of line; without it `advance_iterator` itself is the
  // call. Measured spellings: `advance` 92.18, `advance_iterator(it, -1)` discarded 88.00,
  // `it = advance_iterator(it, -1)` 87.96, `advance_iterator(begin(), -1)` returned directly
  // 78.71, `it = advance_iterator(begin(), -1)` 88.00, `rstl::__advance` called directly 92.07,
  // an extra named copy of the iterator 92.18.
  InstList::iterator Add(const rstl::ncrc_ptr< CInstruction >& instruction) {
    mInstructions.push_back(instruction);
    InstList::iterator it = rstl::advance_iterator(mInstructions.begin(), -1);
    return it;
  }
  void AddStringFragment(const wchar_t* str, int len);
  int WrapOneLTR(const wchar_t* str, int len);
  void MoveWordLTR();
  void StartNewLine();
  void StartNewWord();
  void TerminateLine(bool lastLine);
  void TerminateLineLTR(bool lastLine);

  InstList mInstructions;
  CSaveableState mState;
  CBlockInstruction* mCurrentBlock;
  CLineInstruction* mCurrentLine;
  InstList::iterator mCurrentWord;
  int mCurrentY;
  int mCurrentX;
  int mCurrentWordX;
  int mCurrentWordY;
  int mSpaceDistance;
  bool mImageBaseline;
  rstl::list< CSaveableState > mStateStack;
};

CHECK_SIZEOF(CTextExecuteBuffer, 0xe0)

#endif // _CTEXTEXECUTEBUFFER
