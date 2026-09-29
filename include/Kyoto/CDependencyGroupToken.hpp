#ifndef _CDEPENDENCYGROUPTOKEN
#define _CDEPENDENCYGROUPTOKEN
#include "Kyoto/TToken.hpp"
#include "rstl/vector.hpp"
class CDependencyGroup;
class IObjectStore;

// Guessed name. Owns the group token and its dependency tokens.
class CDependencyGroupToken {
public:
  CDependencyGroupToken(const TToken< CDependencyGroup >& group, IObjectStore& store);

  void Lock();
  void Unlock();
  bool IsLocked() const;
  bool IsLoaded();

private:
  TToken< CDependencyGroup > mGroup;
  rstl::vector< CToken > mDependencies;
  // `mutable` on the last two members is what makes the implicit copy constructor
  // (0x8033ECCC, 88 bytes) byte-exact: without it mwcceppc's -O4,p pipeliner interleaves the
  // word and byte copies and gives the word a second temporary register, and retail stores the
  // word before it loads the byte. It changes no layout and no behaviour - no `const` member
  // function writes either member. See docs/RUNNING_THE_DECOMP.md, "`mutable` on the members is
  // what stops a copy constructor's tail from being pipelined".
  mutable uint mLockCount;
  mutable bool mLoaded : 1;
};
CHECK_SIZEOF(CDependencyGroupToken, 0x20)
#endif // _CDEPENDENCYGROUPTOKEN
