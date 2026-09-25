// The LoadTypedef of SLdrTweakCameraBob on its own, as its own decomp unit.
//
// Retail lays the Tweaks module's per-struct functions out as three contiguous
// entries - LoadTypedef<T>, ~T, T - and a `Matching` unit has to reproduce exactly
// the range it claims.  `__ct__18SLdrTweakCameraBobFv` is at 57.67%, so the triple
// cannot be claimed; the LoadTypedef is at 100% and can be.  The other two stay in
// SLdrTweakCameraBob.cpp, which claims 0x1A18..0x1B3C and stays NonMatching.
//
// Range claimed: Tweaks .text 0x00001768..0x00001A18 (0x2B0 = 688 bytes).
#include "MetroidPrime/ScriptLoader/SLdrTweakCameraBob.hpp"

void LoadTypedefSLdrTweakCameraBob(SLdrTweakCameraBob& data, CInputStream& input) {
  const int propertyCount = input.ReadUint16();
  for (int i = 0; i < propertyCount; ++i) {
    const uint propertyId = input.ReadInt32();
    const u16 propertySize = input.ReadUint16();
    switch (propertyId) {
    case 0x7fda1466: {
      data.instanceName = rstl::string(input);
      break;
    }
    case 0xe2a0b6f1: {
      data.cameraBobExtentX = input.ReadFloat();
      break;
    }
    case 0x29fc6554: {
      data.cameraBobExtentY = input.ReadFloat();
      break;
    }
    case 0x149d7339: {
      data.cameraBobPeriod = input.ReadFloat();
      break;
    }
    case 0xa27bb5a7: {
      data.orbitBobScale = input.ReadFloat();
      break;
    }
    case 0xe3580b2b: {
      data.maxOrbitBobScale = input.ReadFloat();
      break;
    }
    case 0xb05dade7: {
      data.slowSpeedPeriodScale = input.ReadFloat();
      break;
    }
    case 0x6dc5d440: {
      data.targetMagnitudeTrackingRate = input.ReadFloat();
      break;
    }
    case 0xd16539a7: {
      data.landingBobSpringConstant = input.ReadFloat();
      break;
    }
    case 0xadbb0a42: {
      data.viewWanderRadius = input.ReadFloat();
      break;
    }
    case 0xe7f8f11b: {
      data.viewWanderSpeedMin = input.ReadFloat();
      break;
    }
    case 0x01985efa: {
      data.viewWanderSpeedMax = input.ReadFloat();
      break;
    }
    case 0xef19ba33: {
      data.viewWanderRollVariation = input.ReadFloat();
      break;
    }
    case 0x7f59be96: {
      data.gunBobMagnitude = input.ReadFloat();
      break;
    }
    case 0x38a82ac1: {
      data.helmetBobMagnitude = input.ReadFloat();
      break;
    }
    default:
      input.ReadBytes(nullptr, propertySize);
      break;
    }
  }
}
