
#include "MetroidPrime/ScriptLoaderRel.hpp"
#include "REL/REL_Setup.h"

#include "Kyoto/Alloc/CMemory.hpp"
#include "MetroidPrime/Tweaks/CTweakContents.hpp"
#include "MetroidPrime/Tweaks/CTweakGame.hpp"
#include "MetroidPrime/Tweaks/CTweakPlayer.hpp"

#include "dolphin/types.h"

STweaks_FuncPtrs REL_loader_Tweaks;

CTweakContents::CTweakContents() {}

CTweakContents::~CTweakContents() {}

void DecodeAnyTweak(uint instanceId, CInputStream& input) {
  switch (instanceId) {

  case 0x5457414d:
    LoadTypedefSLdrTweakAutoMapper(gpTweakContents->TweakAutoMapper, input);
    break;
  case 0x5457424c:
    LoadTypedefSLdrTweakBall(gpTweakContents->TweakBall, input);
    break;
  case 0x54574342:
    LoadTypedefSLdrTweakCameraBob(gpTweakContents->TweakCameraBob, input);
    break;
  case 0x5457474d:
    LoadTypedefSLdrTweakGame(gpTweakContents->TweakGame, input);
    break;
  case 0x54574755:
    LoadTypedefSLdrTweakGui(gpTweakContents->TweakGui, input);
    break;
  case 0x54574743:
    LoadTypedefSLdrTweakGuiColors(gpTweakContents->TweakGuiColors, input);
    break;
  case 0x54575041:
    LoadTypedefSLdrTweakParticle(gpTweakContents->TweakParticle, input);
    break;
  case 0x5457504c:
    LoadTypedefSLdrTweakPlayer(gpTweakContents->TweakPlayer, input);
    break;
  case 0x54575032:
    LoadTypedefSLdrTweakPlayer(gpTweakContents->TweakPlayer2, input);
    break;
  case 0x54575043:
    LoadTypedefSLdrTweakPlayerControls(gpTweakContents->TweakPlayerControls, input);
    break;
  case 0x54574332:
    LoadTypedefSLdrTweakPlayerControls(gpTweakContents->TweakPlayerControls2, input);
    break;
  case 0x54575047:
    LoadTypedefSLdrTweakPlayerGun(gpTweakContents->TweakPlayerGun, input);
    break;
  case 0x5457504d:
    LoadTypedefSLdrTweakPlayerGun(gpTweakContents->TweakPlayerGunMuli, input);
    break;
  case 0x54575052:
    LoadTypedefSLdrTweakPlayerRes(gpTweakContents->TweakPlayerRes, input);
    break;
  case 0x54575353:
    LoadTypedefSLdrTweakSlideShow(gpTweakContents->TweakSlideShow, input);
    break;
  case 0x54575447:
    LoadTypedefSLdrTweakTargeting(gpTweakContents->TweakTargeting, input);
    break;
    default:
      break;
  }
}

void REL_LoadTweaks(CInputStream& input) {
  if ((uint)input.ReadInt32() == 0x4e54574b && input.ReadInt8() == 1) {
    gpTweakContents = new CTweakContents();

    for (int instanceCount = input.ReadInt32(); instanceCount != 0; --instanceCount) {
      uint instanceType = (uint)input.ReadInt32();
      u16 instanceSize = input.ReadUint16();
      input.ReadInt32(); // skip instance id
      instanceSize -= 6;

      for (int connectionCount = input.ReadInt32(); connectionCount != 0; connectionCount--) {
        instanceSize -= 0xc;
        input.ReadInt32();
        input.ReadInt32();
        input.ReadInt32();
      }

      // Record current position of input
      DecodeAnyTweak(instanceType, input);
      // Read instanceSize - (current position - old position)
    }
  }
}

#ifdef TARGET_PC
void REL_CreateTweakGlobals() {}

// The real body is below, in the #else. It cannot be compiled by the
// host build: it uses mwcceppc's three-argument `new T(file, line)` and names
// thirteen DOL .sbss slots (`lbl_80418F28` .. `lbl_80418F64`) that exist only in
// main.dol. Nothing calls this function on the host - it is reachable only through
// STweaks_FuncPtrs::CreateGlobals, which TweaksInit assigns and nothing invokes -
// so the empty body is not a behaviour change on PC, and it is the same arrangement
// main.cpp uses for CMain::RsMain. See docs/research/tweak_globals.md.
#else
// ---------------------------------------------------------------------------
// REL_CreateTweakGlobals
// ---------------------------------------------------------------------------
//
// Retail: Tweaks.rel .text:0x00000508, 0x5AC bytes.  The full store-by-store map,
// including the three unnamed object classes and the two allocations retail
// never stores anywhere, is docs/research/tweak_globals.md.  What follows is the
// shape, not a matching body: the statement order below *is* retail's, but the
// per-slot object types for the 52-, 248- and 604-byte objects are unmodelled, so
// those are allocated raw and handed to retail's own constructors by name.
//
// The member offsets are retail's, measured from __ct__14CTweakContentsFv /
// __dt__14CTweakContentsFv in this module and from `li r3, 0x31F4` in
// REL_LoadTweaks.  **They are also the header's numbers now.**  This comment used
// to say the opposite: that CTweakContents.hpp declared the sixteen members in the
// right order but sizeof()'d them at 0x37D0 against retail's 0x31F4, so
// `&gpTweakContents->TweakPlayer` compiled to +0x1220 instead of +0x10E8 and the
// enum below was carrying the difference.  The whole 0x5DC was one struct -
// SLdrTweakPlayerRes_AutoMapperIcons declared fourteen rstl::strings where retail
// has nine, so it was 0xE0 against 0x90 and every member after it in both classes
// sat 0x50 too high.  Measured with mwcceppc after the fix
// (tools/size_probe_tweaks.cpp, `objdump -s`): sizeof(CTweakContents) == 0x31F4,
// TweakSlideShow == 0x2EB0, TweakTargeting == 0x2F28, TweakPlayerRes == 0x29B8 -
// retail's, all four.  The enum is kept because rewriting this function to use the
// members would move its bytes and it is 68.29% as it stands; it is no longer
// load-bearing, it is belt-and-braces.

enum {
  kTweakAutoMapper = 0x0000,
  kTweakBall = 0x0174,
  kTweakCameraBob = 0x03f0,
  kTweakGame = 0x0438,
  kTweakGui = 0x0534,
  kTweakGuiColors = 0x0c5c,
  kTweakParticle = 0x10a8,
  kTweakPlayer = 0x10e8,
  kTweakPlayer2 = 0x1464,
  kTweakPlayerControls = 0x17e0,
  kTweakPlayerControls2 = 0x1934,
  kTweakPlayerGun = 0x1a88,
  kTweakPlayerGunMuli = 0x2220,
  kTweakPlayerRes = 0x29b8,
  kTweakSlideShow = 0x2eb0,
  kTweakTargeting = 0x2f28,
};

// What the thirteen four-byte slots hold.  fn_800BC024 (0x800BC024) loads
// *lbl_80418F28 and calls three float accessors straight on it, so word 0 is the
// tweak struct used as `this`, not a pointer to free.  The registered destructor
// for these slots (fn_80032BE8 and its eleven siblings) is `Free(obj[0]);
// Free(obj)`, which is why the two lines below are a Free of the *slot* and then a
// store into it rather than an assignment.
struct STweakHolder {
  void* tweak;
};

// The 52-, 248- and 604-byte objects.  Retail constructs them with fn_82_C4C
// (.text:0xC4C, 56 bytes), fn_802150EC (0x802150EC) and fn_82_AB4 (.text:0xAB4,
// 148 bytes); none of the three is modelled, so the sizes are retail's literals
// and the constructors are called by retail's own symbols.
extern "C" void* fn_82_C4C(void* self, void* tweak);
extern "C" void* fn_82_AB4(void* self, void* tweak);
extern "C" void* fn_802150EC(void* self, void* tweak);

// "Destroy the old value at this slot, then store 0 into it" - the two helpers
// retail emits in place of Free()+store for the slots whose objects have real
// destructors.  fn_82_480 is .text:0x480 (0x88) and walks three rstl::strings;
// fn_82_304 is .text:0x304 (0x48) and walks five SLdrSplines.
extern "C" void fn_82_480(void** slot, void* value);
extern "C" void fn_82_304(void** slot, void* value);

// The .rodata object every allocation passes as its "file name": the 13-character
// string "Standard.NTWK" with four bytes of padding after the terminator.  Retail
// passes base+14, i.e. a pointer into that padding.
extern "C" const char lbl_82_section4_3F0[];

extern "C" void* __nw__FUlPCcPCc(unsigned long size, const char* file, const char* line);
extern "C" void fn_80216D84(void* tweak);
extern "C" void fn_800BC024();

// The thirteen further slots, all in the DOL's .sbss.  gpTweakGame,
// gpTweakPlayerA and gpTweakPlayerB are the three with names in
// config/G2ME01/symbols.txt; the rest are dtk names, and the DOL's .ctors entry 4
// (fn_800324A4) registers a destructor for each at 0x800326C8..0x80032BE8.
extern "C" void* lbl_80418F28;
extern "C" void* lbl_80418F2C;
extern "C" void* lbl_80418F34;
extern "C" void* lbl_80418F38;
extern "C" void* lbl_80418F3C;
extern "C" void* lbl_80418F48;
extern "C" void* lbl_80418F4C;
extern "C" void* lbl_80418F50;
extern "C" void* lbl_80418F54;
extern "C" void* lbl_80418F58;
extern "C" void* lbl_80418F5C;
extern "C" void* lbl_80418F60;
extern "C" void* lbl_80418F64;

void REL_CreateTweakGlobals() {
  // TweakAutoMapper.  Retail's initialiser is a plain `lwz` of gpTweakContents,
  // not an `addi` of +0, because the member is the first one.
  STweakHolder* a = new (lbl_82_section4_3F0 + 0xe, nullptr) STweakHolder;
  if (a) {
    a->tweak = (void*)gpTweakContents;
  }
  CMemory::Free(lbl_80418F28);
  lbl_80418F28 = a;

  // TweakBall
  a = new (lbl_82_section4_3F0 + 0xe, nullptr) STweakHolder;
  if (a) {
    a->tweak = (char*)(void*)gpTweakContents + kTweakBall;
  }
  CMemory::Free(lbl_80418F2C);
  lbl_80418F2C = a;

  // TweakGame
  a = new (lbl_82_section4_3F0 + 0xe, nullptr) STweakHolder;
  if (a) {
    a->tweak = (char*)(void*)gpTweakContents + kTweakGame;
  }
  CMemory::Free(gpTweakGame);
  gpTweakGame = (CTweakGame*)a;

  // TweakGui
  a = new (lbl_82_section4_3F0 + 0xe, nullptr) STweakHolder;
  if (a) {
    a->tweak = (char*)(void*)gpTweakContents + kTweakGui;
  }
  CMemory::Free(lbl_80418F34);
  lbl_80418F34 = a;

  // TweakGuiColors
  a = new (lbl_82_section4_3F0 + 0xe, nullptr) STweakHolder;
  if (a) {
    a->tweak = (char*)(void*)gpTweakContents + kTweakGuiColors;
  }
  CMemory::Free(lbl_80418F38);
  lbl_80418F38 = a;

  // TweakParticle, 52 bytes.  Retail builds the object and then calls
  // fn_82_480(&lbl_80418F3C, 0), which destroys the old value and stores 0 - it
  // never stores the new one, so the slot is null on return and the object is
  // unreachable.  Reproduced as measured rather than as intended.
  void* particle = __nw__FUlPCcPCc(0x34, lbl_82_section4_3F0 + 0xe, nullptr);
  if (particle) {
    particle = fn_82_C4C(particle, (char*)(void*)gpTweakContents + kTweakParticle);
  }
  fn_82_480(&lbl_80418F3C, 0);

  // TweakPlayer2
  a = new (lbl_82_section4_3F0 + 0xe, nullptr) STweakHolder;
  if (a) {
    a->tweak = (char*)(void*)gpTweakContents + kTweakPlayer2;
  }
  CMemory::Free(gpTweakPlayerB);
  gpTweakPlayerB = (CTweakPlayer*)a;

  // TweakPlayer.  This is the store boot_path.md's wall turns on: the very next
  // thing the CGameArchitectureSupport constructor does is call GetRightAnalogMax
  // and GetLeftAnalogMax through this pointer.
  a = new (lbl_82_section4_3F0 + 0xe, nullptr) STweakHolder;
  if (a) {
    a->tweak = (char*)(void*)gpTweakContents + kTweakPlayer;
  }
  CMemory::Free(gpTweakPlayerA);
  gpTweakPlayerA = (CTweakPlayer*)a;

  // TweakPlayerControls2
  a = new (lbl_82_section4_3F0 + 0xe, nullptr) STweakHolder;
  if (a) {
    a->tweak = (char*)(void*)gpTweakContents + kTweakPlayerControls2;
  }
  CMemory::Free(lbl_80418F48);
  lbl_80418F48 = a;

  // TweakPlayerControls
  a = new (lbl_82_section4_3F0 + 0xe, nullptr) STweakHolder;
  if (a) {
    a->tweak = (char*)(void*)gpTweakContents + kTweakPlayerControls;
  }
  CMemory::Free(lbl_80418F4C);
  lbl_80418F4C = a;

  // TweakPlayerGunMuli, 248 bytes: { void* tweak; int count; ... }, and
  // fn_802150EC builds the rest out of the tweak's +0xB8/+0xBC/+0xD0.
  void* gun = __nw__FUlPCcPCc(0xf8, lbl_82_section4_3F0 + 0xe, nullptr);
  if (gun) {
    *(void**)gun = (char*)(void*)gpTweakContents + kTweakPlayerGunMuli;
    *(int*)((char*)gun + 4) = 0;
    gun = fn_802150EC(gun, (char*)(void*)gpTweakContents + kTweakPlayerGunMuli);
  }
  CMemory::Free(lbl_80418F50);
  lbl_80418F50 = gun;

  // TweakPlayerGun, same class, and retail keeps the previous lbl_80418F54 in
  // lbl_80418F64 - after having freed it, which is why lbl_80418F64 dangles if
  // this function is ever called twice.
  void* next = __nw__FUlPCcPCc(0xf8, lbl_82_section4_3F0 + 0xe, nullptr);
  if (next) {
    *(void**)next = (char*)(void*)gpTweakContents + kTweakPlayerGun;
    *(int*)((char*)next + 4) = 0;
    next = fn_802150EC(next, (char*)(void*)gpTweakContents + kTweakPlayerGun);
  }
  CMemory::Free(lbl_80418F54);
  lbl_80418F54 = next;
  lbl_80418F64 = lbl_80418F54;

  // TweakPlayerRes, 604 bytes: nine zeroed words, five SLdrSplines and the tweak
  // at +0x258.  As with TweakParticle, retail never stores the result - it calls
  // fn_82_304(&lbl_80418F58, 0), which destroys the old value and stores 0.
  void* res = __nw__FUlPCcPCc(0x25c, lbl_82_section4_3F0 + 0xe, nullptr);
  if (res) {
    res = fn_82_AB4(res, (char*)(void*)gpTweakContents + kTweakPlayerRes);
  }
  fn_82_304(&lbl_80418F58, 0);

  // TweakSlideShow
  a = new (lbl_82_section4_3F0 + 0xe, nullptr) STweakHolder;
  if (a) {
    a->tweak = (char*)(void*)gpTweakContents + kTweakSlideShow;
  }
  CMemory::Free(lbl_80418F5C);
  lbl_80418F5C = a;

  // TweakTargeting
  a = new (lbl_82_section4_3F0 + 0xe, nullptr) STweakHolder;
  if (a) {
    a->tweak = (char*)(void*)gpTweakContents + kTweakTargeting;
  }
  CMemory::Free(lbl_80418F60);
  lbl_80418F60 = a;

  // TweakCameraBob is the one member with no slot: fn_80216D84 copies fifteen
  // floats out of it into the DOL's .sdata2 at 0x8041A6B0, and fn_800BC024 then
  // runs the auto-mapper over *lbl_80418F28.
  fn_80216D84((char*)(void*)gpTweakContents + kTweakCameraBob);
  fn_800BC024();
}

#endif  // !TARGET_PC

void REL_FreeTweaks() {
  delete gpTweakGame;
  gpTweakGame = nullptr;
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
