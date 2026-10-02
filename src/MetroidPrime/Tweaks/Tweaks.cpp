
#include "MetroidPrime/ScriptLoaderRel.hpp"
#include "REL/REL_Setup.h"

#include "Kyoto/Alloc/CMemory.hpp"
#include "MetroidPrime/CMappableObject.hpp"
#include "MetroidPrime/Player/CPlayerCameraBob.hpp"
#include "MetroidPrime/Tweaks/CTweakAutoMapper.hpp"
#include "MetroidPrime/Tweaks/CTweakBall.hpp"
#include "MetroidPrime/Tweaks/CTweakContents.hpp"
#include "MetroidPrime/Tweaks/CTweakGame.hpp"
#include "MetroidPrime/Tweaks/CTweakGui.hpp"
#include "MetroidPrime/Tweaks/CTweakGuiColors.hpp"
#include "MetroidPrime/Tweaks/CTweakParticle.hpp"
#include "MetroidPrime/Tweaks/CTweakPlayer.hpp"
#include "MetroidPrime/Tweaks/CTweakPlayerControls.hpp"
#include "MetroidPrime/Tweaks/CTweakPlayerGun.hpp"
#include "MetroidPrime/Tweaks/CTweakPlayerRes.hpp"
#include "MetroidPrime/Tweaks/CTweakSlideShow.hpp"
#include "MetroidPrime/Tweaks/CTweakTargeting.hpp"

#include "dolphin/types.h"

// Retail's `operator new` placement string in this module is **not** mwcceppc's per-TU
// `@stringBase0` literal. The module's own `.rodata` (`.rodata:0x3F0..0x405`, split out as
// `Tweaks/auto_03_000003F0_rodata`) is a 0x15-byte string pool holding `"Standard.NTWK\0"` at
// offset 0 and the `"?\?(\?\?)"` tag at offset 0x0E, and all 16 `__nw__FUlPCcPCc` call sites in
// this unit materialise the tag as `lis <pool>; addi r4,r4,0; addi r4,r4,14`
// (`build/G2ME01/Tweaks/asm/auto_03_000003F0_rodata.s`, and the `R_PPC_ADDR16_HA/LO` pair
// against `lbl_82_section4_3F0` plus that `addi` in
// `build/G2ME01/Tweaks/obj/MetroidPrime/Tweaks/Tweaks.o`).
//
// A literal only enters the shared pool if something in the translation unit uses it, and the
// only use that costs no instruction is a `static const char*` initialiser - so this declares
// the 14-byte module name that leads retail's pool. Nothing reads it, and MWCC emits no code
// for it; the pool it forces is byte-for-byte retail's (`objdump -s -j .rodata` on our object
// now reads `Standard.NTWK\0??(??)\0`, 0x15 bytes). Without it every `__nw__` site is four
// bytes short and `REL_CreateTweakGlobals` and `REL_LoadTweaks` score 86.50% and 99.25%
// instead of 100%.
static const char* kTweakModuleName = "Standard.NTWK";

STweaks_FuncPtrs REL_loader_Tweaks;

CTweakContents::CTweakContents() {}

CTweakContents::~CTweakContents() {}

void DecodeAnyTweak(uint instanceId, CInputStream& input) {
  switch (instanceId) {

  case 0x5457414d:
    LoadTypedefTweakAutoMapper(gpTweakContents->TweakAutoMapper, input);
    break;
  case 0x5457424c:
    LoadTypedefTweakBall(gpTweakContents->TweakBall, input);
    break;
  case 0x54574342:
    LoadTypedefTweakCameraBob(gpTweakContents->TweakCameraBob, input);
    break;
  case 0x5457474d:
    LoadTypedefTweakGame(gpTweakContents->TweakGame, input);
    break;
  case 0x54574755:
    LoadTypedefTweakGui(gpTweakContents->TweakGui, input);
    break;
  case 0x54574743:
    LoadTypedefTweakGuiColors(gpTweakContents->TweakGuiColors, input);
    break;
  case 0x54575041:
    LoadTypedefTweakParticle(gpTweakContents->TweakParticle, input);
    break;
  case 0x5457504c:
    LoadTypedefTweakPlayer(gpTweakContents->TweakPlayer, input);
    break;
  case 0x54575032:
    LoadTypedefTweakPlayer(gpTweakContents->TweakPlayer2, input);
    break;
  case 0x54575043:
    LoadTypedefTweakPlayerControls(gpTweakContents->TweakPlayerControls, input);
    break;
  case 0x54574332:
    LoadTypedefTweakPlayerControls(gpTweakContents->TweakPlayerControls2, input);
    break;
  case 0x54575047:
    LoadTypedefTweakPlayerGun(gpTweakContents->TweakPlayerGun, input);
    break;
  case 0x5457504d:
    LoadTypedefTweakPlayerGun(gpTweakContents->TweakPlayerGunMuli, input);
    break;
  case 0x54575052:
    LoadTypedefTweakPlayerRes(gpTweakContents->TweakPlayerRes, input);
    break;
  case 0x54575353:
    LoadTypedefTweakSlideShow(gpTweakContents->TweakSlideShow, input);
    break;
  case 0x54575447:
    LoadTypedefTweakTargeting(gpTweakContents->TweakTargeting, input);
    break;
  default:
    break;
  }
}

void REL_LoadTweaks(CInputStream& input) {
  if (static_cast< uint >(input.ReadInt32()) == 0x4e54574b && input.ReadUint8() == 1) {
    gpTweakContents = rs_new CTweakContents();
    int instanceCount = input.ReadInt32();
    while (instanceCount--) {
      const uint instanceType = input.ReadInt32();
      const u16 serializedSize = input.ReadUint16();
      input.ReadInt32(); // Instance ID.
      uint instanceSize = serializedSize - 6;

      ushort connectionCount = input.ReadUint16();
      while (connectionCount--) {
        instanceSize -= 12;
        input.ReadInt32();
        input.ReadInt32();
        input.ReadInt32();
      }

      const uint position = input.GetReadPosition();
      input.ReadInt32(); // Root property ID and size precede its field count.
      input.ReadUint16();
      DecodeAnyTweak(instanceType, input);
      instanceSize -= input.GetReadPosition() - position;
      if (instanceSize != 0) {
        uint remaining = instanceSize;
        while (remaining--) {
          input.ReadUint8();
        }
      }
    }
  }
}

void REL_CreateTweakGlobals() {
  gpTweakAutoMapper = rs_new CTweakAutoMapper(gpTweakContents->TweakAutoMapper);
  gpTweakBall = rs_new CTweakBall(gpTweakContents->TweakBall);
  gpTweakGame = rs_new CTweakGame(gpTweakContents->TweakGame);
  gpTweakGui = rs_new CTweakGui(gpTweakContents->TweakGui);
  gpTweakGuiColors = rs_new CTweakGuiColors(gpTweakContents->TweakGuiColors);
  gpTweakParticle = rs_new CTweakParticle(gpTweakContents->TweakParticle);
  gpTweakPlayerB = rs_new CTweakPlayer(gpTweakContents->TweakPlayer2);
  gpTweakPlayerA = rs_new CTweakPlayer(gpTweakContents->TweakPlayer);
  gpTweakPlayerControlsB = rs_new CTweakPlayerControls(gpTweakContents->TweakPlayerControls2);
  gpTweakPlayerControlExpert = rs_new CTweakPlayerControls(gpTweakContents->TweakPlayerControls);
  gpTweakPlayerGunMulti = rs_new CTweakPlayerGun(gpTweakContents->TweakPlayerGunMuli);
  gpTweakPlayerGunSingle = rs_new CTweakPlayerGun(gpTweakContents->TweakPlayerGun);
  gpTweakPlayerRes = rs_new CTweakPlayerRes(gpTweakContents->TweakPlayerRes);
  gpTweakSlideShow = rs_new CTweakSlideShow(gpTweakContents->TweakSlideShow);
  gpTweakTargeting = rs_new CTweakTargeting(gpTweakContents->TweakTargeting);

  gpTweakPlayerGun = gpTweakPlayerGunSingle.get();
  CPlayerCameraBob::ReadTweaks(gpTweakContents->TweakCameraBob);
  CMappableObject::ReadAutomapperTweaks();
}

void REL_FreeTweaks() {
  delete gpTweakContents;
  gpTweakContents = nullptr;
  gpTweakAutoMapper = nullptr;
  gpTweakBall = nullptr;
  gpTweakGame = nullptr;
  gpTweakGui = nullptr;
  gpTweakGuiColors = nullptr;
  gpTweakParticle = nullptr;
  gpTweakPlayerB = nullptr;
  gpTweakPlayerA = nullptr;
  gpTweakPlayerControlsB = nullptr;
  gpTweakPlayerControlExpert = nullptr;
  gpTweakPlayerGunMulti = nullptr;
  gpTweakPlayerGunSingle = nullptr;
  gpTweakPlayerRes = nullptr;
  gpTweakSlideShow = nullptr;
  gpTweakTargeting = nullptr;
}

void TweaksInit() {
  REL_loader_Tweaks.Loader = REL_LoadTweaks;
  REL_loader_Tweaks.CreateGlobals = REL_CreateTweakGlobals;
  REL_loader_Tweaks.FreeTweaks = REL_FreeTweaks;
  SetTweaks_FuncPtrs(&REL_loader_Tweaks);
}

// On the cube mwldeppc's linker script calls RELMain/RELExit to build this
// module's prolog and epilog. Tweaks, CannonBall and ForgottenObject each define
// them, which is correct there: they are three separate modules. A flat host link
// cannot hold three symbols with one name, so on the host each gets a distinct
// name and platform/compiled_modules.cpp registers them by module name and runs
// them before the game's entry. MWCC still compiles RELMain/RELExit, so the
// GameCube object is unchanged.
#ifdef __MWERKS__
#define MP_TWEAKS_MAIN RELMain
#define MP_TWEAKS_EXIT RELExit
#else
#define MP_TWEAKS_MAIN mp_relmain_tweaks
#define MP_TWEAKS_EXIT mp_relexit_tweaks
#endif

extern "C" void MP_TWEAKS_MAIN() { TweaksInit(); }

extern "C" void MP_TWEAKS_EXIT() { SetTweaks_FuncPtrs(nullptr); }
