/**
 * PC-side definitions of the retail globals the decompilation can only declare,
 * and of the small mechanical members a PC link needs that a `Matching` unit
 * cannot supply.
 *
 * For the DOL every symbol in the globals section below is defined by a
 * *retail* object, so the decompilation is right to write `extern` and let the
 * linker bind them - that is what `config/G2ME01/symbols.txt` and the auto_*
 * objects describe, and the matching build reproduces the retail bytes without
 * these definitions existing. A PC link has no retail objects, so each one
 * needs a real definition somewhere, holding the value the retail binary holds.
 * This file is that somewhere.
 *
 * The same reasoning covers two more groups that `tools/link_gap.py` measures
 * and that no port TU defines: the static data members of classes whose `.cpp`
 * is not in the port build, and eight `TypesMatch` overrides whose bodies live
 * in `MetroidPrime/TypesMatch.cpp` - a file that is *not* in the port build and
 * cannot be put there (see the note above `CEntity::TypesMatch` below).
 *
 * It is deliberately NOT a unit in `configure.py`. A definition inside a
 * `Matching` unit would collide with the retail object's own definition at DOL
 * link time. A definition inside a `NonMatching` unit links, but it perturbs
 * that unit: a new small-data symbol shifts every SDA offset in the object, so
 * unrelated functions in it lose instruction matches. Measured, in
 * `MetroidPrime/main.cpp`: adding only `BuildTime` below moved
 * `__ct__CGameArchitectureSupport` 84.51% -> 81.54% and `AddWorldPaks` 96.00%
 * -> 95.97%, which `tools/gate.sh` reports as a regression. A unit that
 * configure.py never claims is invisible to all of that.
 *
 * Every address, width and value below is measured out of `build/G2ME01/main.elf`
 * (a linked ELF) and cross-checked against `main.dol` and
 * `config/G2ME01/symbols.txt`. The DOL's small-data bases are
 * `_SDA_BASE_ = 0x8041FD80` (used as r13) and `_SDA2_BASE_ = 0x804223C0`
 * (used as r2).
 */

#include "Kyoto/Basics/RAssertDolphin.hpp"
#include "MetaRender/CCubeRenderer.hpp"

#include "MetroidPrime/TGameTypes.hpp"
#include "MetroidPrime/Tweaks/CTweakContents.hpp"
#include "MetroidPrime/Tweaks/CTweakGame.hpp"
#include "MetroidPrime/Tweaks/CTweakPlayer.hpp"
#include "MetroidPrime/Tweaks/CTweakPlayerGun.hpp"

#include "Kyoto/Alloc/CCallStack.hpp"
#include "Kyoto/Audio/CAudioSys.hpp"
#include "Kyoto/Audio/CSfxManager.hpp"
#include "Kyoto/CARAMManager.hpp"
#include "Kyoto/Graphics/CGraphics.hpp"
#include "Kyoto/Streams/CBitStreamReader.hpp"
#include "Kyoto/Streams/CBitStreamWriter.hpp"
#include "MetroidPrime/CActorLights.hpp"
#include "MetroidPrime/CWorld.hpp"
#include "MetroidPrime/Player/CPlayerState.hpp"
#include "MetroidPrime/Weapons/CGunWeapon.hpp"
#include "rstl/pair.hpp"
#include "rstl/rc_ptr.hpp"
#include "rstl/string.hpp"

#include "MetroidPrime/CActor.hpp"
#include "MetroidPrime/CEntity.hpp"
#include "MetroidPrime/CPhysicsActor.hpp"
#include "MetroidPrime/ScriptLoader/SLdrTweakPlayer.hpp"
#include "MetroidPrime/ScriptObjects/CScriptForgottenObject.hpp"
#include "MetroidPrime/ScriptObjects/CScriptPickup.hpp"
#include "MetroidPrime/ScriptObjects/CScriptSequenceTimer.hpp"
#include "MetroidPrime/ScriptObjects/CScriptSpawnPoint.hpp"
#include "MetroidPrime/ScriptObjects/CScriptStreamedMusic.hpp"

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------

// The three game-wide sentinels. All of them, and one unnamed fourth, are
// initialised by a single static initialiser in the DOL, fn_800E9BF4
// (.text:0x800E9BF4, 0x20 bytes), which is the only writer of any of them
// anywhere in the image - not one `stw`/`sth` to these addresses outside it. It
// is registered as an initialiser by being the second word of `.ctors` entry 13
// (.ctors is at .data:0x803A54A0, 8 bytes per entry, so 0x803A5508 = fn_800E8B48
// then fn_800E9BF4):
//
//   800e9bf4:  3c 60 00 01  lis    r3,1             ; r3 = 0x00010000
//   800e9bf8:  38 80 ff ff  li     r4,-1            ; r4 = 0xFFFFFFFF
//   800e9bfc:  38 03 ff ff  addi   r0,r3,-1         ; r0 = 0x0000FFFF
//   800e9c00:  90 8d 93 a0  stw    r4,-27744(r13)   ; 0x80419120 kInvalidEditorId
//   800e9c04:  b0 0d 93 a4  sth    r0,-27740(r13)   ; 0x80419124 kInvalidUniqueId
//   800e9c08:  90 8d 93 a8  stw    r4,-27736(r13)   ; 0x80419128 kInvalidAreaId
//   800e9c0c:  90 8d 93 ac  stw    r4,-27732(r13)   ; 0x8041912C (unnamed, also -1)
//   800e9c10:  4e 80 00 20  blr
//
// The map agrees on the widths: kInvalidUniqueId is `size:0x2`, the other two
// `size:0x4`, and the `sth` is the only halfword store of the three.
//
// TUniqueId has no one-argument constructor, so 0xFFFF is spelled the way the
// layout computes it: (0x3F << 10) | 0x3FF.
//
// Note that kInvalidUniqueId has two declarations in the tree with the same link
// name and different C++ types: `const TUniqueId` in TGameTypes.hpp and
// `extern "C" const unsigned short` in CScriptWallCrawler.cpp:5, whose object does
// carry `U kInvalidUniqueId`. Both are 2 bytes at offset 0 so the value is the
// same, and the C spelling cannot be changed - that file is a Matching REL unit.
// This definition satisfies both.
const TEditorId kInvalidEditorId = TEditorId(0xFFFFFFFFu);
const TUniqueId kInvalidUniqueId = TUniqueId(0x3F, 0x3FF);
const TAreaId kInvalidAreaId = TAreaId(-1);

// ---------------------------------------------------------------------------
// Globals
// ---------------------------------------------------------------------------

// gpRender - `.sbss:0x804192F8`, 4 bytes. Written exactly once in the whole DOL,
// by CGameGlobalObjects::PostInitialize at 0x80008468, immediately after
// AllocateRenderer returns in r3 and the result is stored to the object's own
// +0x148:
//
//   80008434:  bl     AllocateRenderer__FR12IObjectStoreR10COsContextR10CMemorySysR8IFactory
//   80008460:  stw    r31,328(r29)          ; CGameGlobalObjects::x148
//   80008464:  lwz    r0,328(r29)
//   80008468:  stw    r0,-27272(r13)        ; gpRender
//
// So gpRender is a *second name for the singleton CGameGlobalObjects already
// owns*, not an independent object. On PC the owner is the `renderer` member
// (an rstl unique_ptr) and CGameGlobalObjects::PostInitialize already performs
// exactly this store, so the pointer's value is not a decision this file makes.
// 449 `lwz` sites in the DOL read it; none of them writes it.
CCubeRenderer* gpRender = nullptr;

// The tweak singletons. None of them is ever assigned by DOL code: the only
// thing the DOL does with them is `fn_800324A4` (0x800324A4..0x80032670, the
// first word of `.ctors` entry 4), which stores 0 into fifteen of them and
// registers a destructor for each with `__register_global_object` (0x80344E20,
// whose whole body is a 12-byte {next, dtor, slot} record push onto a list).
//
//   800324cc:  stw    r0,-28248(r13)   ; 0x80418F28                + dtor 0x80032BE8
//   800324e0:  stw    r0,-28244(r13)   ; 0x80418F2C                + dtor 0x80032B94
//   800324fc:  stw    r0,-28240(r13)   ; 0x80418F30 gpTweakGame    + dtor 0x80032B40
//   80032518:  stw    r0,-28236(r13)   ; 0x80418F34                + dtor 0x80032AEC
//   80032534:  stw    r0,-28232(r13)   ; 0x80418F38                + dtor 0x80032A98
//   80032550:  stw    r0,-28228(r13)   ; 0x80418F3C                + dtor 0x80032A00
//   8003256c:  stw    r0,-28224(r13)   ; 0x80418F40 gpTweakPlayerB + dtor 0x800329AC
//   80032588:  stw    r0,-28220(r13)   ; 0x80418F44 gpTweakPlayerA + dtor 0x800329AC
//   ... seven more, to 0x80418F60
//
// The objects themselves are created by the Tweaks REL module, in
// `REL_CreateTweakGlobals` (Tweaks.rel .text:0x508, 0x5AC bytes) - fifteen
// "operator new(size, file, 0) -> store into the global slot" sequences, sizes
// 4 eleven times, 52, 248 twice and 604. Fifteen allocations against fifteen
// registrations, and every dtor is one of those same free-a-member-then-free-self
// shapes, so these are one tweak class per slot. gpTweakPlayerA and
// gpTweakPlayerB share a dtor, i.e. one class instantiated twice.
//
// Consumers inside the DOL: gpTweakGame has 9 load sites, gpTweakPlayerA 7,
// gpTweakPlayerB 2. gpTweakContents has *no* DOL reference at all - only the
// Tweaks module touches it, and Tweaks.cpp's `REL_LoadTweaks` already performs
// the `new CTweakContents` that retail's loader does.
//
// gpTweakPlayerGun / Multi / Single are not in the DOL's symbol table at all:
// they are Tweaks-module BSS, so no retail address exists for them. Only
// gpTweakPlayerGun has a user in the port (CPlayerGun.cpp).
CTweakContents* gpTweakContents = nullptr;
CTweakGame* gpTweakGame = nullptr;
CTweakPlayer* gpTweakPlayerA = nullptr;
CTweakPlayer* gpTweakPlayerB = nullptr;
CTweakPlayerGun* gpTweakPlayerGun = nullptr;
CTweakPlayerGun* gpTweakPlayerGunMulti = nullptr;
CTweakPlayerGun* gpTweakPlayerGunSingle = nullptr;

// ---------------------------------------------------------------------------
// CTweakPlayer's five accessors live in two units of their own now -
// MetroidPrime/Tweaks/CTweakPlayerAnalog.cpp and .../CTweakPlayerSuit.cpp - so
// that they can be `Matching`. They were briefly here, which closed the five
// undefined symbols but left them in a port-side translation unit that
// `configure.py` never claims, so none of them counted. The reasoning, the
// addresses and the mwcceppc-versus-host-compiler measurement are in those two
// files and in `docs/research/tweak_player.md`.

// ---------------------------------------------------------------------------
// Build stamp
// ---------------------------------------------------------------------------

// BuildTime - `.sdata2:0x8041D550`, holding `&BuildString`. The map's
// `size:0x8` is the distance to the next symbol, not the object's width: the
// relocation against it is `R_PPC_ADDR32` (4 bytes) and the eight bytes at the
// address are the pointer followed by a zero word. The proof of the value is a
// relocation rather than a disassembly: retail's `auto_11_8041D400_sdata2.o`
// covers `.sdata2` 0x8041D400..0x8041E250, defines `BuildTime` at section offset
// 0x150 (= 0x8041D550), and carries
//
//   00000150 R_PPC_ADDR32  BuildString
//
// against it. `BuildString` is `.rodata:0x803AC3C6`, in the unclaimed block
// `auto_06_803A9620_rodata.o` at section offset 0x2DA6, and those bytes are
//
//   "Build v1.028 10/18/2004 10:44:32\0AD\0\0\0"
//
// sitting immediately after `MetroidBuildInfo` at 0x803AC3B0 (offset 0x2D90,
// "!#$MetroidBuildInfo!#$\0"). The whole stamp is therefore one literal and
// BuildTime points 22 bytes into it. BUILD_TIME_DUMMY in RAssertDolphin.hpp -
// the port's own transcription of that literal - is the same 31 visible bytes,
// so `%s` prints exactly what retail prints. (No Matching unit emits the
// literal; it lives in that unclaimed .rodata block, so BUILD_INFO is only
// transcription, and this is the one place it becomes load-bearing.)
//
// The DOL reads BuildTime twice and writes it zero times, both reads in
// ErrorHandler (0x8028C3F4, a Matching unit):
//
//   8028c634:  lwz r4,-20080(r2)   ; 0x804223C0 - 20080 = 0x8041D550
//   8028c778:  lwz r5,-20080(r2)
//
// So the value is never a product of the DOL's own execution, and the PC one is
// entirely the port's to choose.
const char* BuildTime = BUILD_TIME_DUMMY;

// ---------------------------------------------------------------------------
// Static data members
//
// Twelve class statics that the port's own objects reference and that no
// compiled unit defines, because none of the eight classes below has a .cpp in
// `files.cmake`. Every value is read out of `build/G2ME01/main.elf` at the
// address and width `config/G2ME01/symbols.txt` gives, except where the comment
// says otherwise.
// ---------------------------------------------------------------------------

// kUnknownType__10CCallStack = .rodata:0x803AEAB8; size:0xC data:string
//
//   803aeab8  55 6e 6b 6e 6f 77 6e 54 79 70 65 00   "UnknownType\0"
//
// 12 bytes, so a `char[12]` and not a pointer; the map's `data:string` agrees.
const char CCallStack::kUnknownType[] = "UnknownType";

// kInvalidHandle__12CARAMManager = .sdata2:0x8041E93C; size:0x4 data:4byte
//
//   8041e93c  ff ff ff ff
//
// Already written as -1 in `src/Kyoto/CARAMManager.cpp`, which `files.cmake`
// does not compile (its header is a stub). Defining it here and there would be
// a duplicate the moment that file is enabled, so this is the copy that counts
// and the other one should be deleted when it is.
const int CARAMManager::kInvalidHandle = -1;

// kDefaultPositionUpdateThreshold__12CActorLights = .sdata2:0x8041B604;
// size:0x4 align:4 data:float
//
//   8041b604  3d cc cc cd
//
// 0x3DCCCCCD = 0.1f. A .sdata2 object, so the value is in the file and not
// folded into the instruction that reads it.
const float CActorLights::kDefaultPositionUpdateThreshold = 0.1f;

// kMedPriority__11CSfxManager has **no symbol** in config/G2ME01/symbols.txt,
// though Metroid Prime's does (`kMedPriority__11CSfxManager = .sdata2:0x805D0B8C,
// size:0x2`), so retail folds it as an immediate here. It was found by reading
// the priority argument of the emitter call: `fn_8029EAF4` takes
// (out, id, pos, dir, useAcoustics, looped, prio), and 33 of its call sites pass
// `lha r9,-16604(r2)`. -16604 is 0x8041E2E4, and the map types 0x8041E2E0 as four
// consecutive 2-byte objects:
//
//   8041e2e0  01 02 | 00 ff | 00 7f | ff ff | 00 00 00 00
//              ?      255     127     65535
//
// so 0x8041E2E2 = kMaxPriority (0xFF, as CSfxManager.hpp says), 0x8041E2E4 =
// kMedPriority (0x7F) and 0x8041E2E6 = kInternalInvalidSfxId (0xFFFF). All
// three are unnamed in the Echoes map, which is why they are not findable by
// name; two of them are the constants the neighbouring header comments already
// quote. `li rX,127` also appears at 147 CSfxManager call sites, but there it is
// the *volume* argument of `SfxStart(id, vol, pan, ...)` - not evidence for the
// priority - which is why the .sdata2 word is the reading that counts.
const short CSfxManager::kMedPriority = 127;

// kMaxVolume__9CAudioSys also has no Echoes symbol, and again retail keeps it in
// memory rather than folding it. `CActor::CActor` initialises `xd4_maxVol` from
// it, and the store is the only `lbz`-from-.sdata2 in that constructor:
//
//   8004e028:  lbz r7,-14840(r2)   ; 0x804223C0 - 14840 = 0x8041F018
//   ...
//   8004e090:  stb r7,312(r31)     ; this + 0x138
//
// .bss-style evidence that 0x138 is xd4_maxVol and not a bitfield: retail's
// field offsets run 0x64 above this header's x-prefixed ones, and 0x138 - 0x64 =
// 0xd4 = `xd4_maxVol`, whose predecessor 0x134 is written by a `stw` of -1 and
// 0x13c is the `xd8_nonLoopingSfxHandles` vector (`addi r3,r31,316`).
//
//   8041f018  c0 03 3a 27 | 1c 8a 2d 4b
//
// All fourteen readers of that address in the DOL are `lbz`, and none is an
// `lfs`/`lfd`, so 0x8041F018 is a one-byte object inside a pool of wider
// constants and its value is 0xC0. `fn_8016864C` is the shape that confirms it
// is a maximum rather than a coincidence:
//
//   8016864c:  lbz  r5,0x8041F018
//   80168650:  clrlwi r0,r4,24          ; r0 = the byte being clamped
//   80168654:  cmplw  r5,r0
//   80168658:  bge    +0x14
//   8016865c:  mr     r4,r5             ; r4 = min(byte, kMaxVolume)
//   80168660:  stb    r4,1377(r3)
const uchar CAudioSys::kMaxVolume = 192;

// kVolumeTable__9CAudioSys = .rodata:0x803AFC78; size:0x100
//
// 256 bytes of ushort, and the extent is proved rather than assumed: the map's
// next symbol, `lbl_803AFD78`, starts at exactly +0x100, and all 128 halfwords
// are *exactly* `(i*i*32768) / (127*127)` for i = 0..127 - a square-law
// amplitude ramp from 0 to 32768, with zero mismatches. A 256-entry table
// would break that at i = 128, so the object is 128 entries and the `size` is a
// width here rather than a distance to the next symbol.
//
// Nothing in the DOL references it, so the index range is unproven: the header
// indexes it with an `int` in `GetScaledVolume` and with a `uchar` in
// `CStaticAudioPlayer.cpp`, and both can read past 128 entries. The 7-bit domain
// the curve is exact over is the only hint that the intended index is 0..127.
const ushort CAudioSys::kVolumeTable[] = {
        0,     2,     8,    18,    32,    50,    73,    99,
      130,   164,   203,   245,   292,   343,   398,   457,
      520,   587,   658,   733,   812,   895,   983,  1074,
     1170,  1269,  1373,  1481,  1592,  1708,  1828,  1952,
     2080,  2212,  2348,  2488,  2632,  2781,  2933,  3090,
     3250,  3415,  3583,  3756,  3933,  4114,  4298,  4487,
     4680,  4877,  5079,  5284,  5493,  5706,  5924,  6145,
     6371,  6600,  6834,  7072,  7313,  7559,  7809,  8063,
     8321,  8583,  8849,  9119,  9394,  9672,  9954, 10241,
    10531, 10826, 11125, 11427, 11734, 12045, 12360, 12679,
    13002, 13329, 13660, 13995, 14335, 14678, 15025, 15377,
    15732, 16092, 16456, 16823, 17195, 17571, 17951, 18335,
    18723, 19115, 19511, 19911, 20316, 20724, 21136, 21553,
    21974, 22398, 22827, 23260, 23696, 24137, 24582, 25031,
    25484, 25941, 26402, 26868, 27337, 27810, 28288, 28769,
    29255, 29744, 30238, 30736, 31238, 31744, 32254, 32768,
};

// skMuzzleLocator__10CGunWeapon = .sdata2:0x8041D2D0; size:0x4 data:4byte
//
//   8041d2d0  80 3a ab d8        ; -> .rodata:0x803AABD8
//   803aabd8  4c 42 45 41 4d 00   "LBEAM\0"
//
// A pointer, and the string it names is the LBeam's muzzle locator, which is what
// `CPowerBeam.cpp` builds a `rstl::string_l` from. (The `??(?)` call-stack tag
// sits immediately *before* it, at 0x803AABD0, and `elbow` immediately after.)
const char* CGunWeapon::skMuzzleLocator = "LBEAM";

// mBrightness__9CGraphics = .sdata:0x80418B00; size:0x8 align:4 data:float
//
//   80418b00  3f 80 00 00 | 00 00 00 00
//
// 0x3F800000 = 1.0f; the map's 0x8 is the distance to the next symbol, and
// `CGraphics.hpp`'s `lwz r7,-30624(r2)` at 0x80089AE0 reads the one word.
float CGraphics::mBrightness = 1.f;

// mViewMatrix__9CGraphics = .bss:0x80416F44; size:0x30 align:4 data:float
//
// .bss, and the only writer in the DOL is
// `SetViewPointMatrix__9CGraphicsFRC12CTransform4f` (0x802C2534), which reaches
// it as `lis r4,-32703; addi r3,r4,28484` = 0x80416F44 and calls
// `__as__12CTransform4fFRC12CTransform4f`. No static initialiser, so the value
// at load is 48 zero bytes and that is what retail has; it is not an identity.
// `CTransform4f` has no default constructor, so the zeros are spelled with its
// inline four-vector constructor and three zero `CVector3f`s; `CVector3f()` on
// its own would leave the floats uninitialised.
CTransform4f CGraphics::mViewMatrix(CVector3f(0.f, 0.f, 0.f), CVector3f(0.f, 0.f, 0.f),
                                    CVector3f(0.f, 0.f, 0.f), CVector3f(0.f, 0.f, 0.f));

// mLastFrameUsedAbove__9CGraphics = .sbss:0x804199A4; size:0x1 data:byte
//
// .sbss, so false.
bool CGraphics::mLastFrameUsedAbove = false;

// CGraphics' five unnamed `.sbss` words in the screen-position / time-provider
// group. Retail leaves all five unnamed and `config/G2ME01/symbols.txt` gives them
// the dtk labels below, so **these are the spellings the `Matching` units use** -
// `src/Kyoto/Graphics/CGraphicsTimeProvider.cpp` and
// `src/Kyoto/Graphics/CGraphicsScreenPosition.cpp` reference them by name, because
// `CGraphics` has no `.cpp` in the tree and a reference to a C++-named static data
// member (`CGraphics::mScreenStretch` -> `_ZN9CGraphics13mScreenStretchE`) would
// have nothing to bind to in the DOL.
//
//   lbl_804199D8 = .sbss:0x804199D8; size:0x4 align:4 data:float
//   lbl_804199DC = .sbss:0x804199DC; size:0x4 data:4byte
//   lbl_804199E0 = .sbss:0x804199E0; size:0x4 data:4byte
//   lbl_804199E4 = .sbss:0x804199E4; size:0x4 data:4byte
//   lbl_804199E8 = .sbss:0x804199E8; size:0x4 data:4byte
//
// All five are `.sbss`, so all five are zero at load - which is a real answer and
// not a placeholder: `GetScreenPosition` reports "no stretch, no offset", and
// `GetSecondsMod900` returns 0.0f before any `CTimeProvider` exists.
//
// **The C++-named members in `include/Kyoto/Graphics/CGraphics.hpp:428-432` are
// deliberately left undefined.** They are the same concepts under invented
// names, and defining them here as well would create a *second* object per
// concept, which is worse than a dangling declaration: `GetSecondsMod900` would
// read the `lbl_` one and any future reader of `CGraphics::mSecondsMod900` would
// read the other. Nothing outside the header names them (measured with grep over
// `src/`, `include/` and `platform/`), so no reference exists and nothing breaks.
// If CGraphics ever grows a `.cpp`, these declarations should be *renamed* to the
// `lbl_` spellings rather than defined alongside them.
//
// **The three screen-position words are 0x804199E0/E4/E8.** Measured, not guessed:
// retail's three `lwz` fields are `9c 60`, `9c 64`, `9c 68` (DOL file offsets
// 0x2BB7B0, 0x2BB7C0, 0x2BB7D0, read with `cmp -l`), and the exact rule is
// `field = (address - _SDA_BASE_) & 0xFFFF` with `_SDA_BASE_` 0x8041FD80 - the
// **full** signed displacement, not half of it. So they are -25504, -25500 and
// -25496, and 0x804199E0/E4/E8. Getting this wrong is invisible to every
// percentage: two wrong answers here were each byte-identical objects that paired
// at 100% under objdiff and passed `unit_fit.sh`, and each broke the DOL's sha1 on
// three bytes. src/Kyoto/Graphics/CGraphicsScreenPosition.cpp has the same story
// from the code side.
extern "C" int lbl_804199E0 = 0;
extern "C" int lbl_804199E4 = 0;
extern "C" int lbl_804199E8 = 0;
// lbl_804199D8 is `.sbss:0x804199D8; data:float` and has exactly one reader in the
// DOL, `GetSecondsMod900`'s no-provider fallback at 0x802BF638, which loads it with
// `lfs`. It is not one of the screen-position words - those are E0/E4/E8 - so
// nothing type-puns it and the map's `data:float` is simply the right type.
extern "C" float lbl_804199D8 = 0.f;
extern "C" CTimeProvider* lbl_804199DC = nullptr;

// lbl_80418EC8 = .sbss:0x80418EC8; size:0x4 data:4byte - the address of `CGameGlobalObjects`'
// last member (+0x150, `CGameGlobalObjectsTail`), stored by the constructor at 0x80008558
// (`stw r0,-28344(r13)`) and read back with `lwz r3,-28344(r13)` at 0x80006078, 0x80006240 and
// 0x8000743C. It sits in `auto_10_80418EC4_sbss`, outside every claimed `.sbss` range, so no
// mwcceppc unit can define it and `src/MetroidPrime/CGameGlobalObjectsCtor.cpp` only declares
// it. `.sbss`, so null until the constructor runs.
extern "C" void* lbl_80418EC8 = nullptr;

// The guest constants `CGameState`'s default constructor and its nested constructors read by
// name - retail `.sdata`/`.sdata2`, outside every claimed range, so no mwcceppc unit defines
// them. Values read out of `build/G2ME01/main.elf` with `objdump -s`:
//
//   .sdata  0x80417D90  01 00 01 00 00 00 00 00   lbl_80417D90 = 1, lbl_80417D91 = 0,
//                                                 lbl_80417D93 = 0 (5 bytes, one is read)
//   .sdata  0x804183DC  00 00 00 00 00 01 00 01   lbl_804183DD = 0, lbl_804183DF = 0
//   .sdata2 0x8041C1A8  00000000 00000000         lbl_8041C1A8 = 0.0 (double)
//   .sdata2 0x8041C1B8  00000000                  lbl_8041C1B8 = 0.0f
//
// Readers: CGameStateMemcardCtor.cpp (D90/D91), SGameStateMemcardFill.cpp (D93),
// CGameStateSlotDefaults.cpp (83DD) and CGameStateSysOptsPutTo.cpp (83DF) take the two `.sdata`
// bytes by address as a one-byte source buffer, and CGameStateCtor.cpp loads the two `.sdata2`
// constants into `x48_time` and `x50`. They cost nothing while those units are unlisted:
// nothing asks for them.
extern "C" unsigned char lbl_80417D90 = 1;
extern "C" unsigned char lbl_80417D91 = 0;
extern "C" unsigned char lbl_80417D93 = 0;
// The third of the four, 0x80417D92 = 1 (the dump above), read by
// SGameStateMemcardBufFill.cpp's `fn_80009AC0` as the byte it fills `SGameStateMemcard`+0x00 with.
extern "C" unsigned char lbl_80417D92 = 1;
extern "C" unsigned char lbl_804183DD = 0;
extern "C" unsigned char lbl_804183DF = 0;
extern "C" const double lbl_8041C1A8 = 0.0;
extern "C" const float lbl_8041C1B8 = 0.0f;

// CWorld::skGlobalEnd / skGlobalNonConstEnd are 4-byte pointers inside
// `CGameArea::CChainIterator`, and neither is named in the map either. Both are
// read through r13, and `-28104`/`-28100` off _SDA_BASE_ 0x8041FD80 are
// 0x80418FB8 and 0x80418FBC - which the retail `CStateManager.o` slice confirms
// by name, carrying `R_PPC_EMB_SDA21 lbl_80418FB8` at .text+0xbc and
// `R_PPC_EMB_SDA21 lbl_80418FBC` at .text+0x114, the two relocations of
// `fn_80036284` and `fn_800362E0`:
//
//   80036294:  lwz  r3,5636(r3)      ; m_world
//   80036298:  lwz  r31,76(r3)      ; the area chain head
//   ...
//   800362b8:  lwz  r31,248(r31)    ; area->m_next
//   800362bc:  lwz  r0,-28104(r13)  ; skGlobalEnd
//   800362c0:  cmplw r31,r0
//   800362c4:  bne   800362a0
//
// Both addresses are in .sbss (0x80418EA0..0x8041A3A8), so they are zero at
// load, and the only writer in the whole DOL, `fn_80052870`, writes zero to
// both:
//
//   80052870:  li   r0,0
//   80052874:  stw  r0,-28104(r13)
//   80052878:  stw  r0,-28100(r13)
//
// So the chain is terminated by a null `m_next` and the sentinel is null, which
// is what `CChainIterator`'s default constructor already produces.
CGameArea::CConstChainIterator CWorld::skGlobalEnd;
CGameArea::CChainIterator CWorld::skGlobalNonConstEnd;

// ---------------------------------------------------------------------------
// rstl sentinels
// ---------------------------------------------------------------------------

// mNull__...basic_string<c,...> = .sbss:0x80419AE8; size:0x1 data:byte
// mNull__...basic_string<w,...> = .sbss:0x80419AEA; size:0x2 data:2byte
//
// Both zero. `src/rstl/rstl_strings.cpp` already *says*
// `template <> char basic_string<char>::mNull;`, but a static data member
// declared without an initialiser is a declaration and not a definition, so the
// host compiler emits nothing and the symbol stays undefined - the trap LANE.md
// records for `extern "C"`. That file is a `Matching` unit, so it is left alone
// here; if it ever grows the `= 0`, the two definitions must not both exist.
template <> char rstl::basic_string< char >::mNull = 0;
template <> wchar_t rstl::basic_string< wchar_t >::mNull = 0;

// `rstl::sNullRefCount` is the word a default-constructed `rstl::rc_ptr` points its
// `x4_refCount` at. Retail has no object for it either - it is absent from
// config/G2ME01/symbols.txt, from every dtk object under build/G2ME01/obj/, and from
// main.elf's symbol table - but retail's `ReleaseData` (0x80008FA4) has no null test on
// that pointer, so a default rc_ptr has to point at a real word or it faults on
// destruction. The count has to be large: `ReleaseData` frees as soon as
// `--*x4_refCount <= 0`, so 0 or 1 would free this static on the first default-constructed
// rc_ptr going out of scope. 0xFFFFFF is the port layer's own value for the same purpose
// in the sibling tree (../MetroidPrimePort/tests/port_buffers.cpp:30) and never approaches
// zero under balanced AddRef/DelRef. See docs/research/rc_ptr.md.
//
// The old `rstl::CRefData rstl::CRefData::sNull` that stood here is gone with `CRefData`
// itself: retail's refcount is a separate four-byte `CMemory` allocation, not a field of a
// shared control block, and keeping the control block cost an indirection on every
// dereference and a second `operator delete` on every release.
int rstl::sNullRefCount = 0x1000000 - 1;

// ---------------------------------------------------------------------------
// TypesMatch
// ---------------------------------------------------------------------------

// Eight overrides the port's own vtables reference. The bodies are retail's,
// read off the DOL, and the shape is the `TYPES_MATCH_IMPL` macro in
// src/MetroidPrime/TypesMatch.cpp:
//
//   cmpwi r4,<id>
//   bne   +8
//   b     <return this>          ; typeId == id
//   ble   <call parent>          ; typeId <  id
//   li    r3,0                  ; typeId >  id
//   b     <return>
//   bl    <parent::TypesMatch>
//
// 0x38 bytes each, except CEntity's, which is 0x10 because it has no parent:
//
//   8009cc84 <TypesMatch__7CEntityCFi>:
//   8009cc84:  cmpwi r4,0
//   8009cc88:  beqlr
//   8009cc8c:  li   r3,0
//   8009cc90:  blr
//
// The ids and the parents are the immediates and the branch targets, and all
// eight agree with include/MetroidPrime/CEntityInfo.hpp's EEntityType and with
// each class's declared base:
//
//   class                  address     size  id   parent (branch target)
//   CScriptForgottenObject  0x8009A984 0x38 160  CEntity  (0x8009cc84)
//   CScriptStreamedMusic    0x8009BA24 0x38  84  CEntity  (0x8009cc84)
//   CScriptSpawnPoint       0x8009BB3C 0x38  79  CEntity  (0x8009cc84)
//   CScriptPickup           0x8009BE14 0x38  66  CActor   (0x8009cc4c)
//   CScriptSequenceTimer    0x8009C9E4 0x38  12  CEntity  (0x8009cc84)
//   CPhysicsActor           0x8009CC14 0x38   2  CActor   (0x8009cc4c)
//   CActor                  0x8009CC4C 0x38   1  CEntity  (0x8009cc84)
//   CEntity                 0x8009CC84 0x10   0  -
//
// Six of the eight are already written in src/MetroidPrime/TypesMatch.cpp -
// CEntity, CActor, CPhysicsActor, CScriptForgottenObject, CScriptSpawnPoint and
// CScriptStreamedMusic - and are missing from the port only because that file is
// not in `files.cmake`. It cannot simply be added: it declares its throwaway
// classes with `uchar x_pad0[0x2f0 - sizeof(CPhysicsActor)]` and the host's
// CPhysicsActor is larger than retail's 0x2f0, so the subtraction underflows and
// gcc rejects the array ("size 18446744073709551592 exceeds maximum object
// size"). CScriptPickup and CScriptSequenceTimer are genuinely undefined
// everywhere, retail included - no unit in this tree writes them.
//
// So all eight are defined here, and **adding TypesMatch.cpp to `files.cmake`
// later would duplicate the six.** Either this block moves into that file (after
// the layout problem is fixed) or its five `TYPES_MATCH_IMPL` lines and its
// `CEntity::TypesMatch` body come out. `PORT_TYPES_MATCH` below is a copy of that
// file's `TYPES_MATCH_IMPL`, character for character, because a macro cannot be
// shared between two .cpp files without moving it into a header both include.
CEntity* CEntity::TypesMatch(int typeId) const {
  return typeId == kET_Entity ? const_cast< CEntity* >(this) : nullptr;
}

#define PORT_TYPES_MATCH(cls, parent, id)                                                        \
  CEntity* cls::TypesMatch(int typeId) const {                                                   \
    if (typeId == id) {                                                                          \
      return const_cast< cls* >(this);                                                           \
    }                                                                                            \
    if (typeId > id) {                                                                           \
      return nullptr;                                                                            \
    }                                                                                            \
    return parent::TypesMatch(typeId);                                                           \
  }

PORT_TYPES_MATCH(CActor, CEntity, kET_Actor)
PORT_TYPES_MATCH(CPhysicsActor, CActor, kET_PhysicsActor)
PORT_TYPES_MATCH(CScriptPickup, CActor, kET_ScriptPickup)
PORT_TYPES_MATCH(CScriptSequenceTimer, CEntity, kET_ScriptSequenceTimer)
PORT_TYPES_MATCH(CScriptSpawnPoint, CEntity, kET_ScriptSpawnPoint)
PORT_TYPES_MATCH(CScriptStreamedMusic, CEntity, kET_ScriptStreamedMusic)
PORT_TYPES_MATCH(CScriptForgottenObject, CEntity, kET_ScriptForgottenObject)

#undef PORT_TYPES_MATCH

// ---------------------------------------------------------------------------
// Retail read-only data a `Matching` unit has to name instead of writing
// ---------------------------------------------------------------------------
//
// A `Matching` object is linked into the DOL, so it cannot own a `.rodata`, `.sdata2` or `.data`
// byte that `config/G2ME01/splits.txt` does not claim for it: adding four bytes of `.sdata2` grew
// the section from 0x54C0 to 0x54E0, moved .bss, and broke the DOL's sha1 with every function in
// every unit still at 100% (measured in docs/research/frame_loop.md). So the three constants below
// are *named* in the units that need them, and defined here, with retail's values, because a PC link
// has no retail object to bind them to. The values are what the instructions read, not the guest
// addresses: a 64-bit host cannot hold 0x8041E258, and nothing should try.
//
// `lbl_8041E258` is what `CStopwatch::CSWData::Initialize` divides by (`lfs f0,-16744(r2)` against
// `__cvt_sll_flt`), and it is 0x3F800000 - 1.0f exactly. Undefined it would be a zero fill and
// `x10_timerPeriod` would be 0.0f, i.e. `GetElapsedTime()` would return 0 for the whole game.
//
// `lbl_8041E260` is what `CStopwatch::CSWData::Wait` adds and subtracts (`lfd f2,-16736(r2)`), and
// it is 0x4330000000000000 as a double, i.e. 2^52 exactly - the largest power of two below which
// adding an integer is still exact in a double.
//
// `lbl_803A60A0` is the rodata blob `CMainFlow::CMainFlow` points seven bytes into
// (`lis`/`addi`/`addi 7`), because retail's linker merged "MainFlow" with the tail of a longer
// literal. The seven bytes in front are "??" "(?(" ")" and a NUL, reproduced here so that
// `lbl_803A60A0 + 7` is the NUL-terminated "MainFlow" the constructor passes to `CIOWin`.
//
// The six `lbl_80417DE*`/`lbl_80417DF*` words are retail `.sdata` at 0x80417DE0..0x80417DF8,
// read by `CMainFlow::SetGameState` as `addi r5,r13,-32672`-style operands to `fn_80048EA4`,
// which dereferences all three. The values are out of `objdump -s -j .sdata`, not guessed:
//
//   80417de0  0000000c 0000000b  ->  12, 11   the pair kCFS_GameExit's message is built from
//   80417de8  0000000c 0000000b  ->  12, 11   the pair kCFS_PreFrontEnd's message is built from
//   80417df0  0000000a 000003e8  ->  10, 1000 the pair kCFS_Game's message is built from
//
// They are named in the unit that needs them and defined here for the same reason as the three
// above: a PC link has no retail object to bind them to, and a zero fill would be a wrong answer
// rather than a missing one. Their only reader, `fn_80048EA4`, is itself still retail-only, so
// nothing in the port reads them yet - which is exactly why they are cheap to close now and would
// have been expensive to notice later.
extern "C" const float lbl_8041E258 = 1.0f;
extern "C" const double lbl_8041E260 = 4503599627370496.0; // 2^52
extern "C" const char lbl_803A60A0[] = "??(??)\0MainFlow";
extern "C" int lbl_80417DE0 = 12;
extern "C" int lbl_80417DE4 = 11;
extern "C" int lbl_80417DE8 = 12;
extern "C" int lbl_80417DEC = 11;
extern "C" int lbl_80417DF0 = 10;
extern "C" int lbl_80417DF4 = 1000;

// `lbl_8041C398` is `.sdata2:0x8041C398`, 4 bytes, `3f800000` = 1.0f, and it is owned by no unit
// in `config/G2ME01/splits.txt` - so the DOL link gets it from dtk's
// `build/G2ME01/obj/auto_11_8041C148_sdata2.o`, and this definition is only here for the port.
// `CWorldStateCtor.cpp` declares it `extern "C" float` (not `const float`, matching the storage
// retail keeps it in) and reads it into a local: mwcceppc re-reads a non-`const` global after every
// store it cannot prove does not alias it, and a store to `this+K` is exactly that, so the direct
// spelling emits five `lfs` where retail emits two. `lbl_8041C394` and `lbl_8041C390` either side
// of it are 32.0f and 4096.0f.
extern "C" const float lbl_8041C398 = 1.0f;

// ---------------------------------------------------------------------------
// rstl free functions
// ---------------------------------------------------------------------------

// Both are 0x30 / 0x3C bytes of the same shape, and both are exactly the
// header's own `basic_string(literal_t, const _CharTp*)` constructor: store the
// pointer, null the cow block, then measure with a loop that steps by
// sizeof(_CharTp).
//
//   802ff418 <string_l__4rstlFPCc>:
//   802ff418:  stw  r4,0(r3)        ; x0_ptr = data
//   802ff41c:  li   r0,0
//   802ff420:  mr   r5,r4
//   802ff424:  stw  r0,4(r3)        ; x4_cow = nullptr
//   802ff42c:  addi r5,r5,1
//   802ff430:  lbz  r0,0(r5)
//   802ff434:  extsb. r0,r0
//   802ff438:  bne  802ff42c
//   802ff43c:  subf r0,r4,r5
//   802ff440:  stw  r0,8(r3)        ; x8_size
//
// `wstring_l` (0x802FF3DC) is the same with `addi r5,r5,2` / `lhz` and a
// divide-by-2 on the size, because retail's wchar_t is 2 bytes; the host's is 4,
// and the constructor divides by sizeof(wchar_t) on its own.
rstl::string rstl::string_l(const char* data) {
  return rstl::string(rstl::string::literal_t(), data);
}

rstl::wstring rstl::wstring_l(const wchar_t* data) {
  return rstl::wstring(rstl::wstring::literal_t(), data);
}

// `__pl__4rstlFRCQ24rstl66basic_string<c,...>RCQ24rstl66basic_string<c,...>`
//   = .text:0x80005AE8; size:0x5C scope:weak
//
// The Itanium mangling of `operator+` is `pl`, so the undefined symbol the port
// reports, `_ZN4rstlplERKNS_12basic_stringIcNS_11char_traitsIcEENS_17rmemory_allocatorEEES6_`,
// is `rstl::operator+(const string&, const string&)` - already declared at
// include/rstl/string.hpp:344 and never defined, not a separate `pl`. Copy the
// first operand, append the second, copy the result out, destroy the temporary:
//
//   80005b04:  addi r3,r1,8
//   80005b08:  bl   basic_string(const basic_string&)
//   80005b0c:  mr   r4,r31
//   80005b10:  addi r3,r1,8
//   80005b14:  bl   append(const basic_string&)
//   80005b18:  mr   r3,r30
//   80005b1c:  addi r4,r1,8
//   80005b20:  bl   basic_string(const basic_string&)
//   80005b24:  addi r3,r1,8
//   80005b28:  bl   internal_dereference()
//
// Four retail `pl` overloads are named in the map; this is the (string, string)
// one, and the only one the port references.
rstl::string rstl::operator+(const rstl::string& a, const rstl::string& b) {
  rstl::string tmp(a);
  tmp.append(b);
  return tmp;
}

// ---------------------------------------------------------------------------
// ScriptCannonBall's module constants (module 57)
//
// `lbl_57_rodata_*` are `.rodata` of `orig/G2ME01/files/RelProd/ScriptCannonBall.rel`, which
// `dtk rel info` puts at file offset 0x14E0, size 0xB4 - so the five objects are the first
// twenty bytes of the section and the sixth is the 0x1C bytes after them. Read out of the
// retail module (big-endian, so the bytes are in IEEE-754 order already):
//
//   +0x00  3f 80 00 00   1.0f     lbl_57_rodata_0
//   +0x04  00 00 00 00   0.0f     lbl_57_rodata_4
//   +0x08  3e 80 00 00   0.25f    lbl_57_rodata_8
//   +0x0C  43 7f 00 00   255.0f   lbl_57_rodata_C   (declared by the port, never referenced)
//   +0x10  40 00 00 00   2.0f     lbl_57_rodata_10
//   +0x14  3f 3f 28 3f 43 61 6e 6e 6f 6e 42 61 6c 6c 20 45 66 66 65 63 74 00 00 00 00
//                                lbl_57_rodata_14, 0x1C bytes
//
// The last one is a float followed by a string literal with no padding between them, which is
// why dtk gives it `size:0x1C` and no type: the object starts at +0x14 and runs to the next
// symbol. "CannonBall Effect" is at +0x14 + 7, which is exactly the `lbl_57_rodata_14 + 7` in
// `src/MetroidPrime/ScriptObjects/CScriptCannonBall.cpp:64` - the string the port hands to
// `CScriptEffect`'s constructor. The four floats agree with how the port already uses them:
// `_0` is the reset value of `CScriptCannonBall::m_f` and the "1.0" it passes to
// `CScriptEffect`, `_4` is the `m_f > 0` / `m_f < 0` clamp, `_8` is the `dt / 0.25f` decay
// divisor in `Think`, and `_10` is the second `CScriptEffect` scale argument.
//
// A REL module's `.rodata` cannot be claimed by a `Matching` unit without also reproducing the
// module's hash, and no unit claims it, so these are port-side definitions. Measured effect on
// the port link gap: 559 -> 554.
// ---------------------------------------------------------------------------

extern const float lbl_57_rodata_0 = 1.0f;
extern const float lbl_57_rodata_4 = 0.0f;
extern const float lbl_57_rodata_8 = 0.25f;
extern const float lbl_57_rodata_C = 255.0f;
extern const float lbl_57_rodata_10 = 2.0f;
// +0x00 is a float, +0x04 begins the string seven bytes into the object. Only the bytes at and
// after +7 are ever read, so the leading float is left as the value it has in the module.
extern const char lbl_57_rodata_14[] = "\x3f\x3f\x28\x3fCannonBall Effect";

// ---------------------------------------------------------------------------
// fn_8033D2EC - the `std::exception` RTTI accessor
//
// Retail is two instructions and 8 bytes:
//
//   8033d2ec:  lwz  r3,-28920(r13)   ; _SDA_BASE_ = 0x8041FD80 -> 0x80418C98
//   8033d2f0:  blr
//
// 0x80418C98 is `__RTTI__Q23std9exception` (config/G2ME01/symbols.txt:20243, `.sdata`, 8 bytes),
// so the function returns the address of the `std::exception` type descriptor. Its three callers
// all use the value as a pointer and none of them calls anything through it:
//
//   0x80005cac  CMain::RsMain            ; `addis r0,r3,8192; cmplwi r0,0` - tests bit 29, then
//                                         LCEnable() if clear
//   0x802c11d4  fn_802C11C0             ; stores it and four offsets from it (964, 1928, 3856,
//                                         5784) into five consecutive .sbss words at 0x80419938
//   0x80319fe8  ShouldEnableLockedCache  ; `(r3 - 0xE0000000 | 0xE0000000 - r3) >> 31`, i.e.
//                                         `r3 != 0xE0000000`
//
// The port's only reference is that third one, transcribed into CCubeMoviePlayer.cpp's local
// `ShouldEnableLockedCache()`. The address is a DOL address with no host meaning, so the
// definition returns null - which is `!= 0xE0000000` exactly as retail's 0x80418C98 is, and the
// port's behaviour is unchanged. Measured effect on the port link gap: 554 -> 553.
// ---------------------------------------------------------------------------

extern "C" void* fn_8033D2EC() {
  return nullptr;
}

// ---------------------------------------------------------------------------
// fn_802275B8 / fn_80227624 - one `CGameOptions::unk2` element, written and read
//
// `unk2` is a `reserved_vector<rstl::pair<bool, bool>, 4>` (see
// `src/MetroidPrime/Player/CGameOptions.cpp:119` and `:146`), and these two are its element
// serialisers. Both are small, straight-line, and call only already-named retail functions, so
// the whole body is the call list:
//
//   802275b8 <fn_802275B8>:                        ; (pair<bool,bool>* p, CBitStreamWriter& out)
//   802275c0:  li   r5,1
//   802275d0:  lbz  r4,0(r3)                        ; p->first
//   802275dc:  neg  r0,r4 ; or r0,r0,r4 ; srwi r4,r0,31   ; r4 = p->first != 0
//   802275ec:  bl   WriteBits__16CBitStreamWriterFUiUi
//   802275f0:  lbz  r4,1(r3)                        ; p->second
//   ...            the same five instructions, one more WriteBits
//
//   80227624 <fn_80227624>:                        ; (pair<bool,bool>* p, CBitStreamReader& in)
//   80227638:  li   r4,1
//   80227648:  bl   ReadBits__16CBitStreamReaderFUi
//   8022764c:  neg  r0,r3 ; or r0,r0,r3 ; srwi r0,r0,31 ; stb r0,0(r30)
//   80227664:  bl   ReadBits__16CBitStreamReaderFUi
//   ...            the same, storing to +1
//
// The `neg/or/srwi` triple is MWCC's `x != 0` and the `stb` of a bool is 0 or 1, so
// `WriteBits(p->first, 1)` and `p->first = in.ReadBits(1) != 0` are the same instructions.
// The two are each other's inverse and the only callers are `CGameOptions::PutTo` and
// `CGameOptions::CGameOptions(CBitStreamReader&)`, one per element, four elements each.
// Measured effect on the port link gap: 553 -> 551.
// ---------------------------------------------------------------------------

extern "C" void fn_802275B8(rstl::pair< bool, bool >& value, CBitStreamWriter& out) {
  out.WriteBits(value.first != 0, 1);
  out.WriteBits(value.second != 0, 1);
}

extern "C" rstl::pair< bool, bool > fn_80227624(CBitStreamReader& in) {
  rstl::pair< bool, bool > value;
  value.first = in.ReadBits(1) != 0;
  value.second = in.ReadBits(1) != 0;
  return value;
}

// `lbl_803AFAA0` is `.rodata:0x803AFAA0`, `size:0x10`, and it is the one retail read-only object
// that is *two* strings the linker merged: bytes 0..6 are the `??(??)?` that stands in for
// `__FILE__` in `Kyoto/Alloc/CMemory.hpp:33`'s throwing `operator new`, and byte 7 starts the
// `".pak"` that `CResLoader::AddPakFileAsync` (0x802FC268, a Matching unit -
// `src/Kyoto/CResLoaderAddPakFileAsync.cpp`) appends to the pak name:
//
//   803afaa0  3f 3f 28 3f 3f 29 00 2e 70 61 6b 00 00 00 00 00   "??(??)..pak"
//
// That unit names the object rather than writing the literal, because a Matching unit may not own
// a .rodata byte, and it is why the bytes have to include the `.pak`: `lbl_803AFAA0 + 7` is the
// suffix the function hands to retail's `rstl::basic_string::operator+(const char*)`. Reproduced
// whole so the +7 arithmetic is retail's arithmetic.
extern "C" const char lbl_803AFAA0[] = "??(??)..pak";

// lbl_803B0098 - the pak-format version message, and retail's "??"(??)?" 76 bytes in
// ---------------------------------------------------------------------------
//
// `.rodata:0x803B0098`, `size:0x58`, and like `lbl_803AFAA0` above it is **two** strings retail's
// linker merged into one object: the 72-character message `CPakFile::InitialHeaderLoad` hands to
// `sprintf` at 0x80323F58, and at +76 the 7-byte `"??"(??)?"` that the inlined
// `rstl::rmemory_allocator::allocate` in `Kyoto/CPakFile.cpp` passes as its `CCallStack`'s
// file-and-line text (retail's `addi r5,r5,76` at 0x80324AB4).
//
//   803b0098  25 73 3a 20 49 6e 63 6f 6d 70 61 74   "%s: Incompat"
//   803b00d8  72 65 20 75 73 69 6e 67 20 25 78 00   "re using %x\0"
//   803b00e4  3f 3f 28 3f 3f 29 00                  "??(??)\0"
//
// The matching build resolves this from dtk's retail object, which defines it as a global `R`.
// The port build has no such object, so it is defined here, next to `lbl_803AFAA0` for the same
// reason: a translation unit that names retail's read-only object needs it defined somewhere the
// linker can see, and this file is in `files.cmake` but not in `configure.py`, so it cannot
// affect `main.dol` or any REL.
extern "C" const char lbl_803B0098[] =
    "%s: Incompatible pak file version -- Current version is %x, you're using %x"
    "\0\0\0\0"
    "??(??)?";

// fn_802FC350 / fn_802FC378 - CResLoader's in-progress pak list
// ---------------------------------------------------------------------------
//
// **These two used to be transcribed here and are now gone**: `src/Kyoto/CResLoaderInsert.cpp`
// is a `Matching` DOL unit claiming retail's `.text:0x802FC350..0x802FC420` and defines both, in
// both builds, out of `rstl::list`'s own members. It could not be done before because the
// transcription could only *describe* the list - `SPakLoadList` here spelled out the head at
// +4, the tail at +8 and the count at +0x14 because a 64-bit host's `rstl::list` has none of
// those offsets - whereas the real `do_insert_before` works on the real list at any width.
//
// What the transcription was for, and what is now measured:
//
//   fn_802FC350  0x802FC350, 0x28 - forwards to the insert with the list's own `x8_end`
//   fn_802FC378  0x802FC378, 0xA8 - the insert: a 16-byte node, an 8-byte item copy guarded
//                by `addic. r5,r3,8 / beq`, the caller's flag byte cleared, the head fixup,
//                the two link stores and `++count`
//
// **The `stb r0,0(r30)` that clears the caller's flag byte is the item's copy constructor**,
// not a statement in the insert. `fn_802FC378` is `rstl::list< SPakLoadEntry >::
// do_insert_before(node*, const SPakLoadEntry&)` and nothing else: its 0xA8 bytes are
// identical, register for register, to retail's own named
// `do_insert_before<list<auto_ptr<CFilePreloadData>>>` at 0x803445DC, whose element type has
// an auto-relinquishing copy constructor. `SPakLoadEntry` in `Kyoto/CResLoader.hpp` has that
// constructor for the same reason, and `fn_802FD174` (`do_erase`) confirms the item is
// `rstl::auto_ptr< CPakFile >`: it `lbz`es the flag byte and then calls `__dt__CPakFile` on
// `*(item+4)`, which is that class's destructor.
//
// `AddPakFileAsync`'s `TARGET_PC` half used to spell the insert out itself for the same
// 64-bit reason; it now calls `fn_802FC350` like the retail side does, which is why the two
// halves of that function finally agree.

// `__dt__12CPlayerStateFv` is retail's own name for `CPlayerState`'s deleting destructor
// (0x8000939C), and `src/MetroidPrime/Player/CPlayerStateRefRelease.cpp` (`fn_8000934C`,
// `rstl::rc_ptr<CPlayerState>::ReleaseData`) calls it by that C name so that mwcceppc emits
// retail's relocation. The host's destructor is `CPlayerState::~CPlayerState()`, so this is the
// MWCC deleting-destructor convention in terms of it: a null `self` does nothing, and a positive
// `flag` frees the storage after destroying the object.
extern "C" void __dt__12CPlayerStateFv(CPlayerState* self, int flag) {
  if (self == nullptr) {
    return;
  }
  if (flag > 0) {
    delete self;
  } else {
    self->~CPlayerState();
  }
}

// ---------------------------------------------------------------------------------------
// `lbl_803A9F38` - retail's own bytes, read out of main.elf rather than written from a guess.
// ---------------------------------------------------------------------------------------
//
// `CErrorOutputWindow::CErrorOutputWindow(bool)` (retail 0x8018169C, 0xB4) stores a pointer to this
// object, and `config/G2ME01/symbols.txt:17224` describes it as `.rodata size:0x14 data:string`.
// The 0x14 bytes at 0x803A9F38 in main.elf are:
//
//   4572726f 72206f75 74707574 2077696e 646f7700   "Error output window\0"
//
// which is 19 characters and a NUL = 0x14 exactly, so the symbol's size and its type in
// `symbols.txt` agree with retail's bytes rather than with an assumption. It is a real string
// with real text in it, so a host definition can be *the same text* - there is nothing to lie
// about, which is the test every host stand-in should be held to.
//
// **`lbl_803B5910` is deliberately NOT defined here, and the reason is worth stating because the
// obvious move is wrong.** `symbols.txt:18177` calls it `.data size:0x1C`; its 7 words are
//
//   00000000 00000000 800078f8 801815f8 80181684 80181480 80049e10
//
// The first two words being zero is a vtable header (offset-to-top, RTTI) and the remaining five
// are **retail code addresses**, so this is retail's own `vtable for CErrorOutputWindow`. Writing
// those five words as host data would be a lie: they are not callable on a PC. The correct host
// definition is a *real* vtable, which means defining the class's key function - the arrangement
// `src/Kyoto/CResFactoryPortVirtuals.cpp` uses and documents.
//
// That is not done, and the reason is a measured one rather than a scheduling one:
// **`CErrorOutputWindow` cannot be `Matching` at all.** `tools/errwin`'s finding, recorded below
// and in `docs/HANDOFF.md`, is that the compiler retail used for this function emits
// `clrlwi r3,r31,24 ; cntlzw` where a different version of the same compiler emits bare `cntlzw`,
// and retail's own binary contains **both** forms 22 KB apart. So the vtable work buys a
// `NonMatching` function a working vtable, and the body is 78.56% regardless.
extern "C" const char lbl_803A9F38[] = "Error output window";
