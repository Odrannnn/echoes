#ifndef _CINGAMETWEAKMANAGER
#define _CINGAMETWEAKMANAGER

#include "Kyoto/SObjectTag.hpp"
#include "rstl/string.hpp"

class CTweakValue {
public:
  struct Audio {
    float GetFadeIn() const { return x0_fadeIn; }
    float GetFadeOut() const { return x4_fadeOut; }
    float GetVolume() const { return x8_volume; }
    const rstl::string& GetFileName() const { return xc_fileName; }

  private:
    float x0_fadeIn;
    float x4_fadeOut;
    float x8_volume;
    rstl::string xc_fileName;
    CAssetId x1c_resourceId;
  };

  const Audio& GetAudio() const { return x24_audio; }

private:
  uint x0_type;
  rstl::string x4_key;
  rstl::string x14_text;
  Audio x24_audio;
  uint x44_value;
};

NESTED_CHECK_SIZEOF(CTweakValue, Audio, 0x20)
CHECK_SIZEOF(CTweakValue, 0x48)

// Retail's default constructor, `fn_8016C230` (0x8016C230, `size:0x14`, unnamed in
// `symbols.txt`). It is four instructions - `li r0,0` and three stores at +0x04, +0x08 and
// +0x0C - and `CGameGlobalObjects`'s constructor reaches it through `new(16)` at 0x80008508-0x80008518,
// which is the only measurement of the object's size: **0x10**. `new` does not zero, and the
// constructor zeroes three of the four words, so the first word is whatever was on the heap.
//
// The four words are here so the size is right; the ctor body is the one call, and it is inlined
// so the `bl` lands at retail's address. Nothing reads them.
class CInGameTweakManager;

extern "C" CInGameTweakManager* fn_8016C230(CInGameTweakManager* self);

class CInGameTweakManager {
public:
  u32 x0_unk;
  u32 x4_unk;
  u32 x8_unk;
  u32 xc_unk;
  CInGameTweakManager() { fn_8016C230(this); }  // the result is unused here

  bool HasTweakValue(const rstl::string& key) const;
  const CTweakValue* GetTweakValue(const rstl::string& key) const;
  static rstl::string GetIdentifierForMusicEvent(CAssetId areaId, const rstl::string& name);
};

extern CInGameTweakManager* gpTweakManager;

#endif // _CINGAMETWEAKMANAGER
