#ifndef _CTWEAKPLAYERCONTROLS
#define _CTWEAKPLAYERCONTROLS

#include "types.h"

#include "MetroidPrime/CControlMapper.hpp"
#include "rstl/single_ptr.hpp"

struct SLdrTweakPlayerControls;

// Echoes keeps one control tweak per control scheme; names are inferred.
class CTweakPlayerControls {
public:
  explicit CTweakPlayerControls(const SLdrTweakPlayerControls& data) : mData(&data) {}

  CControlMapper::EFunctionList GetMapping(CControlMapper::ECommands command) const;
  // Retail's `fn_80215860` (0x80215860) is `lwz r3,0(r3)` / `lbz r3,319(r3)` / `blr`, i.e. it
  // dereferences this class's only member and reads one `bool` out of the struct behind it.
  // That body has no compiled home - 0x80215860 is in an unclaimed auto-split range - so
  // `src/MetroidPrime/PortCTweakPlayerControls.cpp` defines it and needs the pointer.
  // No layout change: an inline reader of the existing member.
  const SLdrTweakPlayerControls* GetData() const { return mData; }

private:
  const SLdrTweakPlayerControls* mData;
};
CHECK_SIZEOF(CTweakPlayerControls, 0x4)

extern rstl::single_ptr< CTweakPlayerControls > gpTweakPlayerControlExpert;
extern rstl::single_ptr< CTweakPlayerControls > gpTweakPlayerControlsB;

#endif // _CTWEAKPLAYERCONTROLS
