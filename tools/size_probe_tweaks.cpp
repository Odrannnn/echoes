// Size/offset probe, measured with mwcceppc's own flags (tools/probe_cc.sh). Never the host:
// the host is 64-bit and its numbers say nothing about MWCC's. Read back with objdump -s.
#include "MetroidPrime/ScriptLoader/SLdrTweakSlideShow.hpp"
#include "MetroidPrime/ScriptLoader/SLdrTweakPlayerRes.hpp"
#include "MetroidPrime/Tweaks/CTweakContents.hpp"
#include "Kyoto/Graphics/CColor.hpp"

extern "C" {

unsigned int probe[] = {
  (unsigned int)sizeof(CColor),
  (unsigned int)sizeof(rstl::string),
  (unsigned int)sizeof(SLdrTweakPlayerRes_AutoMapperIcons),
  (unsigned int)sizeof(SLdrTweakPlayerRes_MapScreenIcons),
  (unsigned int)sizeof(SLdrTGunResources),
  (unsigned int)sizeof(SLdrTBallTransitionResources),
  (unsigned int)sizeof(SLdrTweakPlayerRes),
  (unsigned int)offsetof(SLdrTweakPlayerRes, autoMapperIcons),
  (unsigned int)offsetof(SLdrTweakPlayerRes, mapScreenIcons),
  (unsigned int)offsetof(SLdrTweakPlayerRes, ballTransitionResources),
  (unsigned int)offsetof(SLdrTweakPlayerRes, cinematicResources),
  (unsigned int)offsetof(SLdrTweakPlayerRes, unknown_0x36ad9d19),
  (unsigned int)sizeof(SLdrTweakSlideShow),
  (unsigned int)offsetof(SLdrTweakSlideShow, fontColor),
  (unsigned int)offsetof(SLdrTweakSlideShow, fontOutlineColor),
  (unsigned int)offsetof(SLdrTweakSlideShow, helpFrameColor),
  (unsigned int)offsetof(SLdrTweakSlideShow, stringResName),
  (unsigned int)sizeof(SLdrTweakCameraBob),
  (unsigned int)sizeof(SLdrTweakPlayerGun),
  (unsigned int)sizeof(SLdrTweakTargeting),
  (unsigned int)sizeof(CTweakContents),
  (unsigned int)offsetof(CTweakContents, TweakSlideShow),
  (unsigned int)offsetof(CTweakContents, TweakTargeting),
  (unsigned int)offsetof(CTweakContents, TweakPlayerRes),
};
}
