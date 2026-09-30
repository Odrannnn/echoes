#ifndef _CTWEAKGAME
#define _CTWEAKGAME

#include "rstl/single_ptr.hpp"
#include "rstl/string.hpp"

struct SLdrTweakGame;

class CTweakGame {
public:
  explicit CTweakGame(const SLdrTweakGame& data) : mData(&data) {}

  // Retail 0x80216D5C returns `rstl::string` **by value**, and the bytes say so: the body is
  // `lwz r4,0(r4) / addi r4,r4,16 / b __ct__string` with `r3` never read, which is the sret
  // shape (r3 = the caller's return slot, r4 = `this`) of a by-value class return. Its only
  // caller, `CMain::AddWorldPaks` (0x80005974), proves it from the other side: it sets
  // `addi r3,r1,92` *before* the call, copy-constructs the return slot into its own local
  // (`addi r3,r1,124 / addi r4,r1,92 / b __ct__string`) and then destroys the temporary with
  // `addi r3,r1,92 / b internal_dereference`. A `const rstl::string&` return has no temporary
  // and emits none of those, which is the whole of the frame-size difference in that function.
  // Nothing in the tree defines this function (the port reaches a stub), so the return type
  // cannot move a definition anywhere.
  rstl::string GetPakFile();
  bool GetSplashScreensDisabled();
  int GetTotalPercentage();
  float GetHardModeDamageMultiplier() const;
  float GetHardModeWeaponMultiplier() const;

private:
  const SLdrTweakGame* mData;
};
CHECK_SIZEOF(CTweakGame, 0x4)

extern rstl::single_ptr< CTweakGame > gpTweakGame;

#endif // _CTWEAKGAME
