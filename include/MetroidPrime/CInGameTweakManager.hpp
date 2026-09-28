#ifndef _CINGAMETWEAKMANAGER
#define _CINGAMETWEAKMANAGER

#include "Kyoto/SObjectTag.hpp"
#include "rstl/string.hpp"

class CTweakValue {
public:
  struct Audio {
    float GetFadeIn() const { return mFadeIn; }
    float GetFadeOut() const { return mFadeOut; }
    float GetVolume() const { return mVolume; }
    const rstl::string& GetFileName() const { return mFileName; }

  private:
    float mFadeIn;
    float mFadeOut;
    float mVolume;
    rstl::string mFileName;
    CAssetId mResourceId;
  };

  const Audio& GetAudio() const { return mAudio; }

private:
  uint mType;
  rstl::string mKey;
  rstl::string mText;
  Audio mAudio;
  uint mValue;
};

NESTED_CHECK_SIZEOF(CTweakValue, Audio, 0x20)
CHECK_SIZEOF(CTweakValue, 0x48)

class CInGameTweakManager;

// Port: retail's default constructor for this object, `fn_8016C230` (0x8016C230, 0x14 bytes,
// `li r0,0` + three stores at +0x04/+0x08/+0x0C, unnamed in symbols.txt). Upstream has no such
// function and no fields, so the object's size is unconstrained here; retail's is **0x10**, and
// `new(16)` at 0x80008508 in `CGameGlobalObjects`'s constructor is the only measurement of it. The
// four words exist so the PC port allocates and clears the same 16 bytes. TARGET_PC only: nothing
// in the matching build constructs a `CInGameTweakManager`, so the retail size there is 1 and
// stays 1. The definition is in `src/MetroidPrime/CInGameTweakManagerCtor.cpp`.
extern "C" CInGameTweakManager* fn_8016C230(CInGameTweakManager* self);

class CInGameTweakManager {
public:
  // Retail zeroes +0x04, +0x08 and +0x0C and leaves +0x00 as the heap had it. The names carry the
  // offset because nothing reads them.
  uint mUnk0;
  uint mUnk4;
  uint mUnk8;
  uint mUnkC;

  bool HasTweakValue(const rstl::string& key) const;
  const CTweakValue* GetTweakValue(const rstl::string& key) const;
  static rstl::string GetIdentifierForWorldDefaultMusic(CAssetId world);
  static rstl::string GetIdentifierForMusicEvent(CAssetId areaId, const rstl::string& name);
};

extern CInGameTweakManager* gpTweakManager;

#endif // _CINGAMETWEAKMANAGER
