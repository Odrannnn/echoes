#ifndef _CANIMATIONMANAGER
#define _CANIMATIONMANAGER

#include "Kyoto/Animation/CAnimSysContext.hpp"
#include "Kyoto/TToken.hpp"

class CAnimationDatabase;

class CAnimationManager {
public:
  CAnimationManager(TToken< CAnimationDatabase > animDB, const CAnimSysContext& sysCtx)
  : mAnimDB(animDB), mSysCtx(sysCtx) {}
  ~CAnimationManager();

  // Guessed names. Both are needed by the two database lookups in `src/Kyoto/Animation/CAnimation.cpp`
  // (`fn_8028CA5C` and `fn_8028CAE4`), which read the database through the token and then hand the
  // `CAnimSysContext` on to `IMetaAnim::GetAnimationTree`.
  const TToken< CAnimationDatabase >& GetAnimationDatabase() const { return mAnimDB; }
  const CAnimSysContext& GetSysContext() const { return mSysCtx; }

private:
  TToken< CAnimationDatabase > mAnimDB;
  CAnimSysContext mSysCtx;
};
CHECK_SIZEOF(CAnimationManager, 0x20)

#endif // _CANIMATIONMANAGER
