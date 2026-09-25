/**
 * PC-side definitions of the retail globals the decompilation can only declare.
 *
 * For the DOL every symbol below is defined by a *retail* object, so the
 * decompilation is right to write `extern` and let the linker bind them - that
 * is what `config/G2ME01/symbols.txt` and the auto_* objects describe, and the
 * matching build reproduces the retail bytes without these definitions existing.
 * A PC link has no retail objects, so each one needs a real definition
 * somewhere, holding the value the retail binary holds. This file is that
 * somewhere.
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
