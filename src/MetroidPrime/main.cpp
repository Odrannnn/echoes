#include "MetroidPrime/CMain.hpp"

#include "Kyoto/Alloc/CMemory.hpp"
#include "Kyoto/Audio/CDSPStreamManager.hpp"
#include "Kyoto/Audio/CSfxManager.hpp"
#include "Kyoto/Audio/CStreamAudioManager.hpp"
#include "Kyoto/Basics/CBasics.hpp"
#include "Kyoto/Basics/RAssertDolphin.hpp"
#include "Kyoto/CFrameDelayedKiller.hpp"
#include "Kyoto/CPakFile.hpp"
#include "Kyoto/CResFactory.hpp"
#include "Kyoto/CSimplePool.hpp"
#include "Kyoto/Math/CloseEnough.hpp"
#include "Kyoto/Text/CStringTable.hpp"
// Retail's out-of-line `TReservedAverage<float, 4>` members (0x800069AC / 0x80006954) come from the
// instantiations of this header, not from hand-written copies here - see the note near the bottom.
#include "Kyoto/TReservedAverage.hpp"
#include "dolphin/ar.h"
#include "dolphin/arq.h"
#include "dolphin/os.h"
#include "dolphin/os/OSCache.h"
#include "dolphin/os/OSThread.h"

// `<stdint.h>` is where `uintptr_t` comes from, and `CMain::ShutdownSubsystems` below casts
// through it four times. `dolphin/types.h` guards its own `<stdint.h>` behind TARGET_PC, and
// mwcceppc does not define TARGET_PC, so it is named directly.
#include <stdint.h>
#include <stdio.h>

#include "MetaRender/CCubeRenderer.hpp"
#include "MetaRender/IRenderer.hpp"

#include "MetroidPrime/CAudioStateWin.hpp"
#include "MetroidPrime/CArchitectureQueue.hpp"
#include "MetroidPrime/CConsoleOutputWindow.hpp"
#include "MetroidPrime/Decode.hpp"
#include "MetroidPrime/CErrorOutputWindow.hpp"
#include "MetroidPrime/CGameArchitectureSupport.hpp"
#include "MetroidPrime/CGameGlobalObjects.hpp"
#include "MetroidPrime/CMainFlow.hpp"
#include "MetroidPrime/CMapWorldInfo.hpp"
#include "MetroidPrime/CMemoryCard.hpp"
#include "MetroidPrime/CEnvFxManager.hpp"
#include "MetroidPrime/CInGameTweakManager.hpp"
#include "MetroidPrime/CWorldTransManagerView.hpp"
#include "MetroidPrime/CWorldLayerState.hpp"
#include "MetroidPrime/Player/CGameState.hpp"
#include "MetroidPrime/Player/CGameStateBlocks.hpp"
#include "MetroidPrime/Player/CGameOptions.hpp"
#include "MetroidPrime/Player/CPersistentOptions.hpp"
#include "Kyoto/Streams/CBitStreamReader.hpp"
#include "Kyoto/Streams/CMemoryInStream.hpp"
#include "MetroidPrime/Player/CPlayerState.hpp"
#include "MetroidPrime/Tweaks/CTweakGame.hpp"
#include "MetroidPrime/Tweaks/CTweakPlayer.hpp"

#include "Kyoto/Alloc/LockedCache.hpp"
#include "MetroidPrime/CDamageVulnerability.hpp"
#include "MetroidPrime/CSaveRegion.hpp"
#include "MetroidPrime/ScriptLoaderRel.hpp"
#include "Kyoto/CARAMManager.hpp"
#include "Kyoto/CARAMToken.hpp"
#include "Kyoto/Particles/CElementGen.hpp"
#include "Kyoto/Streams/CBitStreamWriter.hpp"
#include "Kyoto/Streams/CMemoryStreamOut.hpp"
#include "MetroidPrime/CAnimData.hpp"
#include "MetroidPrime/CDecalManager.hpp"
#include "MetroidPrime/CRELFileToken.hpp"
#include "MetroidPrime/CSplashScreen.hpp"
#include "MetroidPrime/Player/CWorldTransManager.hpp"
#include "dolphin/ai.h"
#include "dolphin/base/PPCArch.h"
#include "dolphin/dvd.h"
#include "dolphin/os/OSMemory.h"
#include "dolphin/pad.h"
#include "dolphin/vi.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"

CFactoryFnReturn FModelFactory(const SObjectTag&, const rstl::auto_ptr< uchar >&, int,
                               const CVParamTransfer&);
CFactoryFnReturn FTextureFactory(const SObjectTag&, CInputStream&, const CVParamTransfer&);
CFactoryFnReturn FSkinRulesFactory(const SObjectTag&, CInputStream&, const CVParamTransfer&);
CFactoryFnReturn AnimSourceFactory(const SObjectTag&, CInputStream&, const CVParamTransfer&);
CFactoryFnReturn FCharLayoutInfo(const SObjectTag&, CInputStream&, const CVParamTransfer&);
CFactoryFnReturn FAnimCharacterSet(const SObjectTag&, CInputStream&, const CVParamTransfer&);
CFactoryFnReturn FCollisionResponseDataFactory(const SObjectTag&, CInputStream&,
                                               const CVParamTransfer&);
CFactoryFnReturn FParticleSwooshDataFactory(const SObjectTag&, CInputStream&,
                                            const CVParamTransfer&);
CFactoryFnReturn FParticleFactory(const SObjectTag&, CInputStream&, const CVParamTransfer&);
CFactoryFnReturn FParticleElectricDataFactory(const SObjectTag&, CInputStream&,
                                              const CVParamTransfer&);
// Guessed names, supported by native resource tags and constructed/parsed types.
CFactoryFnReturn FSpawnParticleSystemDataFactory(const SObjectTag&, CInputStream&,
                                                 const CVParamTransfer&);
CFactoryFnReturn FSortedParticleSystemDataFactory(const SObjectTag&, CInputStream&,
                                                  const CVParamTransfer&);
CFactoryFnReturn FProjectileWeaponDataFactory(const SObjectTag&, CInputStream&,
                                              const CVParamTransfer&);
CFactoryFnReturn RGuiFrameFactoryInGame(const SObjectTag&, CInputStream&, const CVParamTransfer&);
CFactoryFnReturn FRasterFontFactory(const SObjectTag&, CInputStream&, const CVParamTransfer&);
CFactoryFnReturn FScannableObjectInfoFactory(const SObjectTag&, CInputStream&,
                                             const CVParamTransfer&);
CFactoryFnReturn FAiFiniteStateMachineFactory(const SObjectTag&, CInputStream&,
                                              const CVParamTransfer&);
CFactoryFnReturn FAiStateMachine2Factory(const SObjectTag&, CInputStream&, const CVParamTransfer&);
CFactoryFnReturn FAudioGroupSetLocDataFactory(const SObjectTag&, const rstl::auto_ptr< uchar >&,
                                              int, const CVParamTransfer&);
CFactoryFnReturn FCollidableOBBTreeGroupFactory(const SObjectTag&, CInputStream&,
                                                const CVParamTransfer&);
CFactoryFnReturn FDecalDataFactory(const SObjectTag&, CInputStream&, const CVParamTransfer&);
CFactoryFnReturn FAudioTranslationTableFactory(const SObjectTag&, CInputStream&,
                                               const CVParamTransfer&);
CFactoryFnReturn FPathFindAreaFactory(const SObjectTag&, const rstl::auto_ptr< uchar >&, int,
                                      const CVParamTransfer&);
CFactoryFnReturn FMapWorldFactory(const SObjectTag&, CInputStream&, const CVParamTransfer&);
CFactoryFnReturn FMapAreaFactory(const SObjectTag&, CInputStream&, const CVParamTransfer&);
CFactoryFnReturn FMapUniverseFactory(const SObjectTag&, CInputStream&, const CVParamTransfer&);
CFactoryFnReturn FMidiDataFactory(const SObjectTag&, CInputStream&, const CVParamTransfer&);
CFactoryFnReturn FSaveWorldFactory(const SObjectTag&, CInputStream&, const CVParamTransfer&);
CFactoryFnReturn FHintFactory(const SObjectTag&, CInputStream&, const CVParamTransfer&);
CFactoryFnReturn FSpatialPrimitivesFactory(const SObjectTag&, CInputStream&,
                                           const CVParamTransfer&);
CFactoryFnReturn FPortalAreaDataFactory(const SObjectTag&, CInputStream&, const CVParamTransfer&);
CFactoryFnReturn FStringListFactory(const SObjectTag&, CInputStream&, const CVParamTransfer&);
CFactoryFnReturn FEditorGeometryToStaticGeometryFactory(const SObjectTag&, CInputStream&,
                                                        const CVParamTransfer&);
CFactoryFnReturn FRuleSetFactory(const SObjectTag&, CInputStream&, const CVParamTransfer&);

extern "C" BOOL __PADDisableRecalibration(BOOL);
extern "C" void OSSetSaveRegion(void*, void*);
extern "C" void sndQuit();
extern "C" BOOL DVDCheckDisk();

class CCharacterFactoryBuilder;
class CGameState;
class CMemoryCard;
class CInGameTweakManager;

extern "C" void fn_8029EFCC();
extern "C" void fn_8033CEE8();
IRenderer* AllocateRenderer(IObjectStore& store, COsContext& mOsContext, CMemorySys& mMemorySys, IFactory& resFactory);

extern "C" {
// Retail `.rodata` 0x803A56C0, 0x1C0 bytes - retail's own string pool, and the pak names, the
// resource names and the two printf formats all live in it. **Declared, never defined**: a
// literal of our own would be routed through mwcceppc's per-unit `@stringBase0` pool and emit
// three instructions that name *that* pool instead of retail's, which is the whole difference
// between 99.94% and 100% on `CMain::FillInAssetIDs`. Retail reaches each as
// `lis rN, lbl_803A56C0@ha / addi rN,rN, lbl_803A56C0@l / addi rN,rN,<offset>`; naming the pool
// and indexing it reproduces the triple.
extern const char lbl_803A56C0[];
// Retail `.sdata2` 0x8041A420, `data:float`, 0x41200000 = **10.0f**. `InfiniteLoopAlarm` is its
// only reader in this range and retail loads it as a relocation against this symbol, so the bare
// literal `10.f` would come out as a reference to our own `@1260` instead.
extern const float lbl_8041A420;
// Retail `.sdata2`, all three read by `CMain::CMain` and by nothing else in this range:
// `lbl_8041A3D8` = **0.0f**, `lbl_8041A3DC` = **1.0f**, `lbl_8041A3F0` = **0.0 as a double**
// (`data:double`, 8 bytes at 0x8041A3F0). Retail loads `f1` from `lbl_8041A3D8` once and stores it
// four times - `stfs f1,0x40/0x44/0x4C/0x50` - and `f0` from `lbl_8041A3DC` once for
// `stfs f0,0x5C` (0x800088B8-0x800088D4). Declared, never defined: `0.0f`/`1.0f` spelled as
// literals come out as relocations against our own `@N` pool entries, and objdiff cannot tell
// those agree with retail's. All three resolve from
// `build/G2ME01/obj/auto_11_8041A3C0_sdata2.o` at DOL link time.
extern const float lbl_8041A3D8;
extern const float lbl_8041A3DC;
extern const double lbl_8041A3F0;
// Retail `.sdata` 0x80417D84, `data:4byte`, **0x000F4240** - retail's spelling of the value
// `rstl::reserved_vector<uint, 10>`'s one-argument constructor fills its ten slots with, which
// the constructor reaches as ten `lwz r0,lbl_80417D84@sda21 ; stw r0,0x64+N*4(r3)` pairs
// (0x800088F4-0x80008924). The literal `0xF4240` is the same number and came out as ten loads of
// our own `@634`, which objdiff cannot tell agree with retail's.
extern const uint lbl_80417D84;
// Retail `.sbss` 0x80418EC4: `&mIoWinMgr`, published by `CGameArchitectureSupport`'s constructor
// (0x80007F80) and cleared by its destructor (0x80007E28). Four bytes, declared only - the port
// defines it in `src/MetroidPrime/PortGlobals.cpp` and this unit is `NonMatching`.
extern CIOWinManager* lbl_80418EC4;
// Retail `.sbss` 0x80419300, the `IController*` published by `CGameArchitectureSupport`'s
// constructor (0x80007FD4) and named `gpController` in `config/G2ME01/symbols.txt:20698`. Four
// bytes, declared only - the port defines it in `src/MetroidPrime/PortGlobals.cpp`.
extern IController* gpController;
// Retail `.sdata` 0x8033CDA0 = `CDSPStreamManager::Shutdown`, called with no argument setup
// between `CGameArchitectureSupport`'s `UnloadAudio()` and `~CIOWinManager` (0x80007E30).
void fn_8033CDA0();
} // extern "C"

CResFactory* gpResourceFactory;
CSimplePool* gpSimplePool;
CCharacterFactoryBuilder* gpCharacterFactoryBuilder;
CStringTable* gpStringTable;
CMain* gpMain;
CGameState* gpGameState;
CMemoryCard* gpMemoryCard;
CInGameTweakManager* gpTweakManager;
float sInfiniteLoopTime;

extern CIOWinManager* gpIOWinManager;
extern CRELFileManager* gpRelFileManager;
extern bool sProgressiveModePrompt; // Prime-correlated name; shared with CSplashScreen.
#define UNUSED_STACK_VAL 0x7337D00D
static uchar sMainSpace[sizeof(CMain)];

// mwcceppc 2.7's parser rejects a `typedef` whose template argument list names two arguments
// (`typedef rstl::vector<TPair, rmemory_allocator> TVec;` is a syntax error here), and rejects the
// same type spelled out in a parameter list, so both typedefs below go through the one-argument
// form. `rmemory_allocator` is `vector`'s default second parameter, and the mangled name that
// comes out is the retail one - measured, not assumed.
typedef rstl::pair< uint, uint > TUiPair;
typedef rstl::vector< TUiPair > TUiPairVec;
typedef rstl::pair< uint, TEditorId > TUiEditorIdPair;
typedef rstl::vector< TUiEditorIdPair > TUiEditorIdPairVec;

// Retail 0x80008DE8 and 0x80008E94: two copies of one 0xAC = 172-byte body, one per element type.
// 0x80008DE8 is `rstl::vector<rstl::pair<Ui,Ui>, rmemory_allocator>::reserve` (`Ui` is `uint`, and
// `rmemory_allocator` is `vector`'s default allocator, so this is `pair<uint,uint>`) and dtk carries
// its mangled name. **0x80008E94 is the same template instantiated for
// `rstl::pair<Ui,9TEditorId>`, identified from its caller and not guessed**: retail's call at
// 0x80142244 is inside `SetCinematicState__18CPersistentOptionsFQ24rstl19pair<Ui,9TEditorId>b`,
// which reserves `r30+24` for `r9+1` elements and then writes the next slot two words wide
// (`lwz r3,0(r31) ; lwz r0,4(r31) ; stw r3,0(r4) ; stw r0,4(r4)`). dtk named that one nothing, so
// `config/G2ME01/symbols.txt` now carries the mangled name in place of `fn_80008E94` and objdiff has
// something to pair on - the mangling is MWCC's own, read out of `nm` on the object (63 and 19 are
// its length prefixes, the same rule as the ten `ReleaseData` entries the file already had).
//
// **The header's own body is retail's, and it was measured rather than reconstructed.**
// `include/rstl/vector.hpp:158`'s `reserve` compiled for this instantiation with this unit's
// exact `build.ninja` flags is **43 of retail's 43 instructions, mnemonic and operand
// identical**, including the four stores at `r1+0x10/0x08/0x0C/0x14` that look like dead writes:
// they are `uninitialized_copy(begin(), end(), newData)`'s two eight-byte `pointer_iterator`
// temporaries, each holding its pointer twice. The 8-byte copy loop, the `slwi ...,3` strides and
// the `cmpw r30,r0 / ble` capacity test are all the template's, unchanged. So the only thing
// that was missing was the *instantiation*: nothing in this unit's 0x800053B8-0x80009880 range
// calls either address (retail's callers of the first are 0x80003E9C, 0x800562F4, 0x80160D24,
// 0x80176E3C and of the second 0x80003FB0, 0x80142244, 0x801EFA88 - all other units), so mwcceppc
// never emitted either COMDAT. The two calls below are what emit them.
//
// **The `TEditorId` instantiation additionally needs `include/MetroidPrime/TGameTypes.hpp` to say
// `pair<uint, TEditorId>` is trivially destructible**, which that header already says for
// `pair<TEditorId, bool>`. Without it the same source emits **184** bytes with `uninitialized_copy`
// outlined into a function of its own instead of 172 bytes inlined, and does not match. Measured
// both ways.
//
// The two 32-byte thunks are the one part of this that is not retail's: nothing in this unit calls
// either `reserve`, so a call has to be forced, and MWCC 2.7 rejects explicit instantiation of a
// member, so it is a real call inside a generated function. Neither name is in `symbols.txt`, so
// objdiff ignores both and they cost 32 bytes of unclaimed `.text` each.
extern "C" void reserve_pair_ui(TUiPairVec* self, int n) { self->reserve(n); }
extern "C" void reserve_pair_ui_editor_id(TUiEditorIdPairVec* self, int n) { self->reserve(n); }

// Retail 0x80008C28-0x80008D68, three functions and 448 bytes: a three-node binary tree with a
// string key, its node constructor and its recursive destroy. Nothing in this unit's
// 0x800053B8-0x80009880 range calls any of the three - retail's callers of `fn_80008C28` are
// 0x800040E0 and 0x80005310, of `fn_80008D68` 0x800040C0 and 0x80004700, and of `fn_80008CE0` the
// three outside this unit - so mwcceppc never emitted them and this object did not define the
// symbols at all. **They are in retail's `main.o` because dtk splits by linked address range**,
// which is the same reason the `rc_ptr<T>::ReleaseData()` copies above exist here.
//
// The node is 44 bytes, and every offset is read off the bytes: `+0x00` and `+0x04` are the two
// children (both `lwz`ed and tested before a recursive call), `+0x08` is the parent (`fn_80008C28`
// writes the new node into `+0x08` of each non-null child), `+0x0C` is copied verbatim, and `+0x10`
// is a 28-byte key. **The key is what makes 44 rather than 32**: `li r3,44` at 0x80008CE8 is
// `allocate(44)`, and `fn_80008CE0` then writes the string's three words at `+0x10/+0x14/+0x18`
// out of `+0x20/+0x24/+0x28` of the source key - so the key is `{rstl::string, uint, uint, uint}`
// and the last three words of the node are the key's own tail.
//
// The three names are dtk placeholders: `config/G2ME01/symbols.txt` carries no name for any of
// them, so `extern "C"` under retail's own `fn_` spelling is the only name objdiff can pair on.
struct SNodeKey {
  rstl::string mName;
  uint mTail0;
  uint mTail1;
  uint mTail2;
};
CHECK_SIZEOF(SNodeKey, 0x1C)

struct SNode {
  void* mLeft;
  void* mRight;
  void* mParent;
  void* mFieldC;
  SNodeKey mKey;
};
CHECK_SIZEOF(SNode, 0x2C)

// Retail 0x80008D68, 0x80 = 128 bytes: destroy both subtrees, release the key, free the node.
// **`node->mKey.~SNodeKey()` has to name the wrapper, not the string.** mwcceppc emits a null test
// on the address of every class-type member it destroys, and `SNodeKey`'s destructor destroys
// `mName` - so the wrapper's test and the string's test are both `addic. r0,r31,16`, which is
// exactly retail's doubled pair at 0x80008DB0 and 0x80008DB8. Naming the string directly gives
// one test (measured), and leaving the destructor to scope exit gives none. The `cmplwi r31,0` at
// 0x80008DA8 is the explicit `if (node)`; its branch goes to the `CMemory::Free`, so the free is
// outside the guard, and `node->mLeft` / `node->mRight` are read through `r4` and `r31` before it.
//
// The weak COMDAT `__dt__Q24rstl66basic_string<c,...>Fv` (0x54 = 84 bytes) comes with it: asking
// for a wrapper's destructor explicitly makes mwcceppc emit the string's out of line as well, even
// though the body is inlined here. Retail's `main.o` does not carry it, and `unit_fit.sh` lists it
// as one more unclaimed function - harmless at 5600 bytes short of the claim, and it disappears
// with the wrapper.
extern "C" void fn_80008D68(void* self, SNode* node) {
  if (node->mLeft) {
    fn_80008D68(self, static_cast< SNode* >(node->mLeft));
  }
  if (node->mRight) {
    fn_80008D68(self, static_cast< SNode* >(node->mRight));
  }
  if (node) {
    node->mKey.~SNodeKey();
  }
  CMemory::Free(node);
}

// Retail 0x80008CE0, 0x88 = 136 bytes: allocate 44, store the four pointers, copy-construct the
// key. `self` is dead - retail's `li r3,44` overwrites `r3` before the only call, which is
// `rstl::rmemory_allocator::allocate(int)`, the same out-of-line `allocate` the DOL's `main.o`
// calls at 0x80008D08.
//
// **The key is copy-*constructed*, not assigned, and that is what the three raw word copies are.**
// mwcceppc expands the implicit copy constructor of `SNodeKey` as "copy-construct `mName` (one
// call to `rstl::basic_string`'s copy constructor) then copy the three `uint`s", which is
// `bl __ct__basic_string` followed by `lwz/stw` on `+0x20/+0x24/+0x28` - retail's 0x80008D38 to
// 0x80008D4C, verbatim. Writing `n->mKey = *key` instead emits `bl assign__Q24rstl66basic_string`
// instead of the constructor (measured), because this tree's `rstl::basic_string` declares
// `operator=`; the placement form keeps the constructor, which is what retail has. The
// `addic. r31,r30,16 / beq` guard at 0x80008D18 is that placement new's member-address test.
extern "C" SNode* fn_80008CE0(void* self, SNode* left, SNode* right, void* parent, void* fieldC,
                              const SNodeKey* key) {
  SNode* n = static_cast< SNode* >(rstl::rmemory_allocator::allocate(sizeof(SNode)));
  if (n) {
    n->mLeft = left;
    n->mRight = right;
    n->mParent = parent;
    n->mFieldC = fieldC;
    new (static_cast< void* >(&n->mKey)) SNodeKey(*key);
  }
  return n;
}

// Retail 0x80008C28, 0xB8 = 184 bytes: a post-order rebuild. Both children are rebuilt first, the
// new node is built from them with `parent = 0`, and each rebuilt child has the new node written
// into its `+0x08`. The two `li r31,0 / li r30,0` are materialised **before** the first child test
// (retail 0x80008C5C-0x80008C60), so the two accumulators are declared above the `if`s, and the
// early `return nullptr` is the `li r3,0 / b` at 0x80008C50 - retail branches straight to the
// epilogue rather than carrying a value.
extern "C" SNode* fn_80008C28(void* self, SNode* node) {
  if (node == nullptr) {
    return nullptr;
  }
  SNode* l = nullptr;
  SNode* r = nullptr;
  if (node->mLeft) {
    l = fn_80008C28(self, static_cast< SNode* >(node->mLeft));
  }
  if (node->mRight) {
    r = fn_80008C28(self, static_cast< SNode* >(node->mRight));
  }
  SNode* n = fn_80008CE0(self, l, r, nullptr, node->mFieldC, &node->mKey);
  if (l) {
    l->mParent = n;
  }
  if (r) {
    r->mParent = n;
  }
  return n;
}

// Retail 0x80008B04, 0x2C = 44 bytes, and it is `TOneStatic<CGameGlobalObjects>::operator delete`
// - the class whose `single_ptr` teardown this unit's `__dt__80006678` belongs to releases
// through, at 0x80006600.
//
//     80008b04  stwu r1,-16(r1) ; mflr r0 ; stw r0,20(r1)
//     80008b10  bl   80008b3c <ReferenceCount__32TOneStatic<18CGameGlobalObjects>Fv>
//     80008b14  lwz  r4,0(r3) ; addi r0,r4,-1 ; stw r0,0(r3)
//     80008b20  lwz  r0,20(r1) ; mtlr r0 ; addi r1,r1,16 ; blr
//
// i.e. `ReferenceCount()--` and nothing else: `r3` is the reference `ReferenceCount()` returns,
// the incoming `ptr` is never read. **Byte-identical to 0x80008A78** -
// `__dl__38TOneStatic<24CGameArchitectureSupport>FPv`, which this unit has matched at 100% for
// two items - apart from the one `bl`.
//
// **It is `extern "C"` under dtk's placeholder because that is the name objdiff pairs on.**
// `config/G2ME01/symbols.txt` has `fn_80008B04 = .text:0x80008B04` and no
// `__dl__32TOneStatic<18CGameGlobalObjects>FPv`, so the mangled spelling scores nothing however
// correct it is; the two `TOneStatic` classes each carry an `__nw__`, a `GetAllocSpace` and a
// `ReferenceCount` that this unit already matches at 100% (0x80008AD4, 0x80008B30, 0x80008B3C), and
// this is the sixth member of that group. `ReferenceCount()` is public in
// `include/Kyoto/TOneStatic.hpp` for this and nothing else.
extern "C" void fn_80008B04(void* ptr) {
  TOneStatic< CGameGlobalObjects >::ReferenceCount()--;
}

// The three functions above `CMain::CMain` in retail's address order. mwcceppc emits in reverse
// source order and the rest of this file is descending by address, so these go first, also
// descending, and the whole translation unit is one descending run.
extern "C" void __sys_free(const void* ptr) { CMemory::Free(ptr); }

// Retail 0x80008A1C, 0xC = 12 bytes:
//     lbz r0, 0x90(r3) ; extrwi r3, r0, 1, 26 ; blr
// `extrwi r3,r0,1,26` extracts bit 26 of the loaded byte, i.e. **bit 2 of the byte at +0x90** -
// `finished`(0), `mMfGameBuilt`(1), `mMaxSpeed`(2) - so the answer is `mMaxSpeed` and not
// `finished`, which is the test upstream's name would suggest.
bool CMain::GetMaxSpeed() { return mMaxSpeed; }

// Retail 0x800089AC, 0x10 = 16 bytes:
//     lbz r0, 0x91(r3) ; rlwimi r0, r4, 7, 24, 24 ; stb r0, 0x91(r3) ; blr
// `rlwimi r0,rX,7-n,24+n,24+n` is field *n* counted down, so this writes bit 0 of the byte at
// +0x91 - the `mThirtyFps` group the eighth `bool : 1` above does not reach. The accessor and
// the bitfield moved out of `#ifdef TARGET_PC` in `include/MetroidPrime/CMain.hpp`; without that
// the matching build had no member there and emitted nothing at all.
void CMain::SetThirtyFps(bool drawn) { mThirtyFps = drawn; }

// Retail 0x800089BC, 0x60 = 96 bytes. Three things, and the header's sketch
// (`{x160_26_screenFading = v;}`) was only one of them - the offset it names, 0x26, is not
// `CMain`'s at all, and the bit it does write is bit 2 of the byte at +0x90.
//
//  * `clrlwi. r0,r4,24 ; beq` is the test of the `bool` argument, and
//    `lbz r0,0x90(r30) ; rlwinm. r0,r0,27,31,31 ; bne` is **mask 31** of that byte. Mask 31 is
//    field 7 counted from the byte's first bit, i.e. `mMaxSpeed` again (see the `rlwinm`
//    arithmetic at `CMain::GetMaxSpeed` above). So the guard is "switching max speed on while the
//    screen is not already fading", and the store below sets the same member.
//  * `lfs f0,lbl_8041A3DC ; stfs f0,0x5c(r30)` resets `x5c` to 1.0f - the **same** `.sdata2`
//    symbol the constructor loads it from, not a literal.
//  * `rlwimi r0,r31,5,26,26` is `SH=5`, i.e. field 2 counted down from `finished`, which is
//    `mMaxSpeed`. `r31` is the argument, so the store is after the call and the allocator
//    has to keep `v` live across it.
//
// Declared in `include/MetroidPrime/CMain.hpp` since before this, with no definition anywhere,
// so our object did not define the symbol at all and the 96 bytes read 0.00%.
//
// **`const` on the parameter is load-bearing and is the whole 99.25%.** The prologue and all
// twenty body instructions are already byte-identical to retail; the three epilogue reloads were
// not, and that is what the percentage was:
//
//   retail  80008a04: lwz r0,20(r1) ; lwz r31,12(r1) ; lwz r30,8(r1) ; mtlr r0
//   ours    00002180: lwz r31,12(r1); lwz r30,8(r1) ; lwz r0,20(r1) ; mtlr r0
//
// A void function with no `mr r3,rN` in its epilogue leaves the order of the three reloads free,
// and mwcceppc's tie-break depends on how the argument is treated. `const bool v` puts the
// argument in the same "named, never written" class as retail's and the order comes out right.
// Measured on this unit, twelve spellings, all with the other 88 bytes unchanged
// (`.tmp/opencode/sms/h3.py` re-runs them; only the diff count against the DOL's 96 bytes is
// shown, with the two relocation fields masked):
//
//   `const bool v`                                    0 diff bytes   <- this
//   `const bool fading = v;` before the bitfield write 0 diff bytes   (same effect, one more name)
//   `bool v`                                           8
//   `bool v` + `CMain* const self = this;`            8
//   `bool v` + `!!v`                                   8
//   `bool v` + `mMaxSpeed == 0`                     8
//   `bool v` + `const float one = lbl_8041A3DC;`       8
//   `bool arg` (renamed parameter)                     8
//   `bool v` + `(!mMaxSpeed)`                       8
//   `bool v` + `if (v) { if (mMaxSpeed) {} else {} }` 8
//   `bool v` + `mMaxSpeed = (v != 0)`              wrong size (108 B)
//   `bool v` + `if (!(v && !mMaxSpeed)) .. else ..` wrong size (104 B)
//
// The top-level `const` is not part of the signature, so `CMain.hpp`'s `void SetMaxSpeed(bool)`
// declaration still matches and is left alone; `src/MetroidPrime/mainTail.cpp` is the port's
// copy of this function and is not a DOL unit, so nothing else defines it.
void CMain::SetMaxSpeed(const bool v) {
  if (v && !mMaxSpeed) {
    CFrameDelayedKiller::StallAndFlushAllAllocations();
  }
  mMaxSpeedDrawTimer = lbl_8041A3DC;
  mMaxSpeed = v;
}

// Retail 0x80008898, 0x114 = 276 bytes. Nineteen of its twenty stores are the member
// initialiser list in declaration order; the other two are the `gpMain = this` epilogue. Every
// constant is retail's own `.sdata2` symbol rather than a literal (see the declarations above) -
// `lbl_8041A3D8` is loaded once into `f1` and stored four times, `lbl_8041A3DC` once into `f0`,
// `lbl_8041A3F0` once into `f2` - which is what makes the register allocation come out at all.
CMain::CMain(COsContext* context, CSaveRegion* saveRegion, CMemorySys* memorySys,
             CDvdRequestSys* dvdRequestSys)
: mOsContext(context)
, mSaveRegion(saveRegion)
, mMemorySys(memorySys)
, mDvdRequestSys(dvdRequestSys)
, x10_(lbl_8041A3F0)
, mTickTimes()
, mDrawTimes()
, mAverageTickTime(lbl_8041A3D8)
, mAverageDrawTime(lbl_8041A3D8)
// **`mFrameTimeMinimum` (+0x48) is deliberately absent**: retail's constructor has no store at
// +0x48 at all, so it is left for `CMain::SetFrameTimeMinimum` (0x80005C64, the only writer) and
// `CMain::AsyncIdle` (which reads it, then clears it). Naming it here costs one instruction the
// retail object does not have.
, mSoftResetHoldTime(lbl_8041A3D8)
, mResetInputDelay(lbl_8041A3D8)
, mGameGlobalObjects(nullptr)
, mRestartMode(kRM_Default)
, mMaxSpeedDrawTimer(lbl_8041A3DC)
, mFrameTimes(0xF4240)
, mFrameTimeIdx(0)
, mFinished(false)
, mMfGameBuilt(false)
, mMaxSpeed(false)
, mResetButtonHeld(false)
, mManageCard(false)
, mResetRequested(false)
, mGameExitReset(false)
, mGameFrameDrawn(false)
, mArchSupport(nullptr)
{
  gpMain = this;
}

extern "C" void InvokeCMain(int argc, char** argv, COsContext* context,
                            CSaveRegion* saveRegion, CMemorySys* memorySys,
                            CDvdRequestSys* dvdRequestSys) {
  CMain* main = new (&sMainSpace) CMain(context, saveRegion, memorySys, dvdRequestSys);
  main->RsMain(argc, argv);
  main->~CMain();
}

CMain::~CMain() {}

// The stack-guard fill's word. Retail materialises it as `lis r5,29496 / addi r5,r5,-12275`
// (0x80008710 / 0x8000871C) = 0x73380000 + (-0x2FF1) = **0x7337D00D**, and `CMain::ShutdownSubsystems`
// below confirms it independently: its scan at 0x80008638 is `addis r0,r3,-29495 ;
// cmplwi r0,53261`, i.e. `word + 0x8CC90000 == 0xD00D`. It is **not** a `memset` pattern
// (0D D0 37 73), so the source is a `uint` word loop that MWCC compiled eight-wide with a
// remainder, which is the shape at 0x80008728-0x80008770.
static const uint kStackGuardWord = 0x7337D00D;

// The three retail data objects this reads, declared only. `lbl_80418BA8` is `.sdata` 0x80418BA8 =
// the ARAM bump pointer; `lbl_80418EA0` is the four-byte `.sbss` word `ARAlloc` is handed, zero in
// the DOL as built; `lbl_803C5AB8` is the 12-byte `.bss` AR length stack, and retail reaches it as a
// **relocation** (`lis r3,0 / R_PPC_ADDR16_HA lbl_803C5AB8 / addi r3,r3,0 /
// R_PPC_ADDR16_LO lbl_803C5AB8`) rather than as the bare literal `(u32*)0x803c5ab8`, which compiles
// to `lis r3,-32708 ; addi r3,r3,23224` - the same value, two instructions, and no relocation.
extern "C" {
extern uint lbl_80418BA8;
extern uint lbl_80418EA0;
extern char lbl_803C5AB8[];
}

// The five calls between the second `printf` and `CFrameDelayedKiller::Initialize`, in retail's
// order. **A callee's body is not a precondition for reproducing a function**: retail's own objects
// supply the bytes, so these are declared and called.
extern "C" void fn_802DAE30();
extern "C" void fn_8002ADC8();
extern "C" void fn_80301CC4(uint, uint, uint);
extern "C" void fn_800E85A8();
extern "C" void fn_800DC0B0();

// Retail 0x80008680, 0x15C = 348 bytes. The eight-`stw` fill is a `uint` word loop, and the `+3`
// /`>>2` on its trip count is retail's `(end - start + 3) / 4` in one expression. The two printf
// formats are retail's, reached as `lbl_803A56C0 + 0x187` and `+ 0x19D`.
// v
// v
// v
// Retail 0x80008680, 0x15C = 348 bytes. The eight-`stw` fill is a `uint` word loop, and the `+3`
// /`>>2` on its trip count is retail's `(end - start + 3) / 4` in one expression. The two printf
// formats are retail's, reached as `lbl_803A56C0 + 0x187` and `+ 0x19D`.
//
// **`fillStart` is built by pointer arithmetic on a copy of `stackBase`, not by an integer
// subtraction, and that is load-bearing for the register allocation.** Written the obvious way -
// `uint* fillStart = (uint*)((uintptr_t)stackBase - 0x2000);` - the object is 348 bytes and every
// instruction matches retail except one transposition: mwcceppc puts the loop **bound** in r5 and
// the stack-guard **word** in r4, where retail has r4 and r5 respectively (15 of 87 instructions,
// 99.08%). Every spelling of the obvious form was measured and all 15 land there: the word as a
// literal, as a file-scope or function-local `const`, as seven different integer types; the bound
// as `uint*`/`const uint*`/`void*`/`uchar*`/`uintptr_t`, `register`-qualified or a named
// `uint* const`; `for`/`while`/empty-increment/do-while; `p++`/`p += 1`/`p = p + 1`; a named
// `fillFrom`; a count-based or index-based loop; the word as a struct member; and naming the byte
// count before the loop (that one *does* flip the pairing to retail's, but it keeps
// `guardEnd + 0x400/4` alive across the loop, loses the `mr r6,r0` fold and comes out 352 bytes -
// four over the claim).
//
// The two-statement form changes **which value is live at the loop**, not how many: `fillStart`
// now holds `stackBase` and is decremented, so the loop bound is a *decremented copy* rather than
// a fresh integer subtraction, and MWCC's priority for it rises above the guard word's. The
// emitted pair is then exactly retail's (`addi r4,r29,-8192` for the bound, `lis r5,29496` for the
// word) with no instruction added - `0x2000 / 4` is folded by the compiler and the `addi`'s -8192
// is identical. **The generalisable rule: when a loop body's temporaries come out transposed
// against retail, look for a source shape that changes a value's *provenance* - copy-then-adjust
// versus recompute - before trying more spellings of the same expression.**
void CMain::InitializeSubsystems() {
  ARInit((u32*)lbl_803C5AB8, 3);
  ARAlloc(lbl_80418EA0);
  lbl_80418BA8 = lbl_80418BA8 + lbl_80418EA0;
  ARQInit();

  OSThread* thread = OSGetCurrentThread();
  printf(lbl_803A56C0 + 0x187);
  uint* guardEnd = (uint*)((((uintptr_t)thread->stackEnd) + 1023) & ~1023);
  uint* stackBase = (uint*)thread->stackBase;
  OSProtectRange(3, guardEnd, 1024, 0);
  uint* fillStart = (uint*)stackBase;
  fillStart -= 0x2000 / 4;
  for (uint* p = guardEnd + 0x400 / 4; p < fillStart; ++p) {
    *p = kStackGuardWord;
  }
  DCFlushRange(guardEnd + 0x400 / 4,
               (uint)((uintptr_t)fillStart - (uintptr_t)(guardEnd + 0x400 / 4)));
  printf(lbl_803A56C0 + 0x19D, (unsigned)(uintptr_t)thread->stackBase,
         (unsigned)(uintptr_t)thread->stackEnd);
  fn_802DAE30();
  fn_8002ADC8();
  fn_80301CC4(2048, 0x600000, 4096);
  fn_800E85A8();
  fn_800DC0B0();
  CFrameDelayedKiller::Initialize();
}// Retail 0x80008570, 0x110 = 272 bytes. The ten callees are retail functions this tree has no
// body for and `config/G2ME01/symbols.txt` names at their own addresses, so they are declared and
// called: **a callee's body is not a precondition for reproducing a function**, and declaring them
// costs the matching build nothing because `dtk dol split` supplies retail's bytes for the whole
// claimed range. The 0x801F0xxx family is one class - an 8-byte object `{void* x0; bool x4;}` -
// and `fn_801F03C4` copies its `string` argument into it, so retail's second argument really is an
// `rstl::string const&` and not a `char const*`: the call passes **the address of the temporary**.
//
// **The host does not compile this body at all**, and this is a `#ifdef` rather than a comment
// because retail's tail reads `OSGetCurrentThread()` +0x304/+0x308 as a stack pointer and looks
// for 0x7337D00D in the 8 KB *below* it. Aurora's `OSThread` puts `stackBase`/`stackEnd` at
// exactly those offsets, so the shape compiles and the offsets are right - and what it reads is
// Aurora's allocator's memory rather than a stack. The port's body is `PortShutdownSubsystems()`
// in `src/MetroidPrime/PortBoot.cpp`, a translation unit `configure.py` never claims; mwcceppc
// does not define TARGET_PC, so the matching build still compiles retail's body verbatim.
extern "C" void fn_800E8494();
extern "C" void fn_802DAE24();
extern "C" void fn_8002AD44();
extern "C" void* fn_801F03C4(void* self, const rstl::string& name, bool start);
extern "C" void fn_801F02C4(void* self);
extern "C" int fn_801F025C(void* self);
extern "C" void fn_801F05D0(void* owner);
extern "C" void fn_80218760();
extern "C" void fn_801F0280(void* self);
extern "C" void fn_801F0308(void* self, short value);
extern "C" void fn_801F0518(void* owner);
extern "C" void fn_800DC03C();

// The 8-byte object the 0x801F0xxx class occupies on the stack, at r1+8. It is **not** a retail
// type and it is never constructed here: `fn_801F03C4` is what fills it in, and retail emits no
// store to r1+8 before that call. A local whose address is taken and whose members are never read
// is the only shape that allocates eight bytes and nothing else.
struct STuObject {
  void* x0_owner;
  bool x4_started;
};

void PortShutdownSubsystems();

#ifdef TARGET_PC
void CMain::ShutdownSubsystems() { PortShutdownSubsystems(); }
#else
void CMain::ShutdownSubsystems() {
  CFrameDelayedKiller::ShutDown();
  fn_800E8494();
  fn_802DAE24();
  fn_8002AD44();

  STuObject tu;
  fn_801F03C4(&tu, rstl::string_l(lbl_803A56C0 + 0xCB), true);
  fn_801F02C4(&tu);

  // The condition is a **byte mask on an int**, not a boolean, and the constant is measured rather
  // than guessed. Retail's test is `clrlwi. r0,r3,24` + `beq`, which keeps the low eight bits.
  // Thirteen spellings of the obvious `& 0xFF000000` (and of `!= 0`, `>> 24`, `<< 8`,
  // signed/unsigned, the operands reversed) all compile to `clrrwi. r0,r3,24` and are one
  // instruction wrong; `& 0xFF` is the only mask measured that emits retail's bytes.
  // `& 0xFF != 0` would be `cmpwi r3,0` and one instruction shorter, so the mask is in the source.
  while ((fn_801F025C(&tu) & 0xFF) == 0) {
    fn_801F05D0(gpRelFileManager);
  }

  fn_80218760();
  fn_801F0280(&tu);
  fn_801F0308(&tu, -1);
  fn_801F0518(gpRelFileManager);
  fn_800DC03C();

  OSThread* thread = OSGetCurrentThread();
  uint stackBase = (uint)thread->stackBase;
  uint* p = (uint*)((((uintptr_t)thread->stackEnd) + 1023) & ~1023);
  // **Two statements, not one expression, and that is load-bearing.** Written as a single
  // `p = (uint*)((((uintptr_t)thread->stackEnd) + 1023) & ~1023) + 0x400;` the function is five
  // instructions short of retail and no spelling of it gets closer: mwcceppc's register allocator
  // puts the masked value in r3 and `p` in r4, so `limit` needs a fourth register and lands in r6
  // where retail has it in r3. Written as a separate `p += 0x100` the allocator coalesces the
  // masked value into `p`'s register, r4 serves both, and r3 stays free for `limit`.
  // `0x100` is 256 **words**; mwcceppc scales it to the `addi`'s 1024 bytes.
  p += 0x100;
  for (uint* limit = (uint*)(stackBase - 0x2000); p < limit; ++p) {
    // `addis r0,r3,-29495 ; cmplwi r0,53261`, i.e. `word + 0x8CC90000 == 0xD00D`. The same
    // constant `CMain::InitializeSubsystems` stores at 0x80008710, 0x2EC bytes away, agreeing.
    if (*p + 0x8CC90000 != 0xD00D) {
      break;
    }
  }
  uint used = (uint)(stackBase - 0x2000) - (uint)p + 0x2000;
  OSReport(lbl_803A56C0 + 0x16A, used, used >> 10);
}
#endif // TARGET_PC

// Retail 0x8000848C, 0xE4 = 228 bytes, and **the only writer of `gpGameState` in the DOL** -
// `CGameArchitectureSupport`'s constructor loads it at 0x800081A4 with no null test, and the store
// is 0x8000854C. The body is four member constructors, two allocations and six global stores, and
// it never reads its two parameters (`fn_800084A0`'s prologue is
// `stwu r1,-16(r1); mflr r0; stw r0,20(r1); stw r31,12(r1); mr r31,r3` and r4/r5 are untouched for
// the whole 0xE4).
//
// **The two allocations are written out rather than spelled `new`,** and that is load-bearing:
// they are the two sites that pass retail's `.rodata` pool as `operator new`'s file operand, and
// the `CGameState* made = self; if (made != 0) { made = f(made); } return made;` shape is what
// puts the callee's result in **r0** instead of leaving it in r3, which is what retail does
// (`mr r0,r3 ; stw r0,304(r31)` against `stw r3,304(r31)` for a ternary or a `static_cast`).
// Measured, four variants; only the named temporary fixes it, and it fixes both allocations.
extern "C" CGameState* fn_801449C8(CGameState* self);
extern "C" CInGameTweakManager* fn_8016C230(CInGameTweakManager* self);

static inline CGameState* MakeCGameState() {
  CGameState* self = static_cast< CGameState* >(::operator new(sizeof(CGameState)));
  CGameState* made = self;
  if (made != 0) {
    made = fn_801449C8(made);
  }
  return made;
}

static inline CInGameTweakManager* MakeInGameTweakManager() {
  CInGameTweakManager* self =
      static_cast< CInGameTweakManager* >(::operator new(sizeof(CInGameTweakManager)));
  CInGameTweakManager* made = self;
  if (made != 0) {
    made = fn_8016C230(made);
  }
  return made;
}

CGameGlobalObjects::CGameGlobalObjects(COsContext& context, CMemorySys& memorySys)
    : mMemoryCardSys()
    , mResFactory()
    , mSimplePool(mResFactory)
    , mCharacterFactoryBuilder()
    , mGameState(MakeCGameState())
    , mInGameTweakManager(MakeInGameTweakManager()) {
  // The six stores at 0x80008534-0x80008558. `_SDA_BASE_` is 0x8041FD80 and the displacements are
  // the full signed ones, so -28380 is `gpResourceFactory`, -28376 `gpSimplePool`, -28372
  // `gpCharacterFactoryBuilder`, -28360 `gpGameState`, -28352 `gpTweakManager` and -28344 is
  // 0x80418EC8. `resFactory`, `simplePool` and `mCharacterFactoryBuilder` are the *members'*
  // addresses; `gameState` and `mInGameTweakManager` are read back out of their `single_ptr`s with
  // `lwz`, because the constructor above stored the result there.
  //
  // The two parameters are named because the signature is retail's, and are unused because retail
  // never reads them.
  (void)context;
  (void)memorySys;
  gpResourceFactory = &mResFactory;
  gpSimplePool = &mSimplePool;
  gpCharacterFactoryBuilder = &mCharacterFactoryBuilder;
  gpGameState = mGameState.get();
  gpTweakManager = mInGameTweakManager.get();
  gpRelFileManager = &mRelFileManager;
}

void CGameGlobalObjects::PostInitialize(COsContext& context, CMemorySys& memorySys) {
  AddPaksAndFactories(context);
  LoadStringTable();
  printf(lbl_803A56C0 + 0x150);
  mRenderer = AllocateRenderer(mSimplePool, context, memorySys, mResFactory);
  // Retail stores the renderer into +0x148 and reads it back there, then writes the result into
  // `gpRender` - a separate store, and the reason the vtable load below is `lwz r0, 0x148(r29)`.
  gpRender = reinterpret_cast< CCubeRenderer* >(mRenderer.get());
  CEnvFxManager::Initialize();
}

void CGameGlobalObjects::LoadStringTable() {
  mStringTable = gpSimplePool->GetObj(lbl_803A56C0 + 0x146);
  gpStringTable = **mStringTable;
}

// Retail 0x8000823C. `sInfiniteLoopTime >= lbl_8041A420` and not `>= 10.f`: retail loads the
// constant as a relocation against `.sdata2` 0x8041A420, and the bare literal would come out as a
// reference to our own pool. The format string is at +0x133 into `lbl_803A56C0` for the same reason.
void InfiniteLoopAlarm(OSAlarm* alarm, OSContext* context) {
  if (sInfiniteLoopTime >= lbl_8041A420) {
    OSCancelAlarm(alarm);
    rs_debugger_printf(lbl_803A56C0 + 0x133);
  }
  sInfiniteLoopTime += alarm->period / OS_TIMER_CLOCK;
}

// Retail `.sbss` 0x80418EA0, the four bytes `CMain::InitializeSubsystems` hands to `ARAlloc` and
// which `CGameArchitectureSupport`'s constructor passes as `CAudioSys`'s `aramSize`. Zero at
// load; the one writer is `fn_80009864`, which computes it as `*(u32*)lbl_8041EE00 * 14`.
// (`lbl_8041EE00` is 0x00008F00, so what lands here is 0x7D000 = 512512. The `0x80415980` this
// comment used to name is in `.rodata` and is not what the load reaches - the measurement is on
// `fn_80009864` at the end of this file.) **Read
// here rather than written as the literal `0x5fc000`**, because retail loads it
// (`lwz r8,lbl_80418EA0@r13` at 0x80007C2C, before the `li r4..r7,0x30` run that sets up the other
// four arguments) and a `0x5fc000` literal comes out as `lis r5,96 ; addi r8,r5,-16384` - two
// instructions in the wrong place for the same value. `src/MetroidPrime/CMainInitializeSubsystems.cpp`
// declares the same symbol with the same reasoning.
extern "C" uint lbl_80418EA0;

CGameArchitectureSupport::CGameArchitectureSupport(COsContext& mOsContext)
: mAudioSys(0x30, 0x30, 0x30, 0x30, lbl_80418EA0)
, mInputGenerator(&mOsContext, gpTweakPlayerA->GetLeftAnalogMax(),
                 gpTweakPlayerA->GetRightAnalogMax())
, mGameFrameCount(0)
, mTickRemainder(0.f)
, mPreviousTickRemainder2(0.f)
, mPreviousTickRemainder(0.f)
// , x74_(2)
, mInfiniteLoopAlarmSet(false) {
  CAudioSys::SysSetVolume(0x7F, 0, 0xFF);
  CAudioSys::SetDefaultVolumeScale(0x75);
  CAudioSys::SetVolumeScale(CAudioSys::GetDefaultVolumeScale());
  // The two `bl` targets are `CSfxManager::Initialize` (0x8029EFCC, 0x54 bytes) and
  // `CDSPStreamManager::Initialize` (0x8033CEE8, 0x148), both of which `config/G2ME01/symbols.txt`
  // now names. They were `extern "C" void fn_8029EFCC()` / `fn_8033CEE8()` before, which emits
  // the identical instruction and the identical relocation target (retail's own address) - the
  // names here match the map rather than inventing `fn_` names for functions it names.
  CSfxManager::Initialize();
  CDSPStreamManager::Initialize();
  CStreamAudioManager::SetMusicVolume(0x7F);
  CAudioSys::TrkSetSampleRate(kTSR_One);
  gpMain->SetMaxSpeed(false);
  gpMain->ResetGameState();
  // 0x80007F80, between `ResetGameState` and the first `AddIOWin`. Retail publishes `&mIoWinMgr`
  // into `.sbss` 0x80418EC4 here and the destructor clears it; 0x80007FD4 stores
  // `mInputGenerator.GetController()` into `.sbss` 0x80419300 (`gpController`).
  //
  // **Both were skipped, on a claim about `symbols.txt` that is no longer true**: the comment
  // here used to say `gpController` "is not named in this tree's `symbols.txt`", so writing one
  // without the other was retail's 4 instructions against our 2. It is named -
  // `config/G2ME01/symbols.txt:20698`, `gpController = .sbss:0x80419300; // type:object size:0x4` -
  // so both are written, and retail's four instructions are what comes out.
  // **Written through a named local, and that is load-bearing.** Retail computes `&mIoWinMgr` once
  // at 0x80007F80 (`addi r30,r31,68`) and every one of the four `AddIOWin` calls passes it as
  // `mr r3,r30`. Spelled `mIoWinMgr.AddIOWin(...)` four times, mwcceppc re-materialises the address
  // each time (`addi r3,r31,68`) and spends r0 on the `.sbss` store instead of r30 - four
  // instructions of difference for the identical semantics. A local reference is what lets the
  // allocator hoist it.
  CIOWinManager& mgr = mIoWinMgr;
  lbl_80418EC4 = &mgr;
  gpController = mInputGenerator.GetController();
  mgr.AddIOWin(new CMainFlow(), 0, 0);
  mgr.AddIOWin(new CConsoleOutputWindow(8, 5.f, 0.75f), 100, 0);
  mgr.AddIOWin(new CAudioStateWin(), 100, -1);
  mgr.AddIOWin(new CErrorOutputWindow(CErrorOutputWindow::kF_Zero), 10000, 100000);
  gpGameState->GameOptions().EnsureOptions();
  sInfiniteLoopTime = 0.f;
  OSSetPeriodicAlarm(&mInfiniteLoopAlarm, OSGetTime(), (float)OS_TIMER_CLOCK, InfiniteLoopAlarm);
  mInfiniteLoopAlarmSet = true;
}

CGameArchitectureSupport::~CGameArchitectureSupport() {
  if (mInfiniteLoopAlarmSet) {
    OSCancelAlarm(&mInfiniteLoopAlarm);
    mInfiniteLoopAlarmSet = false;
  }
  mIoWinMgr.RemoveAllIOWins();
  // 0x80007E28: `li r0,0 ; stw r0,lbl_80418EC4`, between `RemoveAllIOWins` and `UnloadAudio`. The
  // counterpart of the store the constructor does.
  lbl_80418EC4 = 0;
  // `UnloadAudio` is declared `static` in `include/MetroidPrime/CGameArchitectureSupport.hpp`
  // precisely so that retail's `bl` at 0x80007E2C has no `mr r3,rN` in front of it.
  UnloadAudio();
  // 0x80007E30, immediately after `UnloadAudio` and before `~CIOWinManager`.
  fn_8033CDA0();
  // CSfxManager::Shutdown();
  // CDSPStreamManager::Shutdown();
}

bool CGameArchitectureSupport::UpdateTicks() {
  bool result = false;
  // **The saved value is what is restored, not a literal `1`.** Retail keeps `OSDisableInterrupts`'s
  // return in r29 across the stopwatch read and hands *that* register back at 0x80007CBC
  // (`mr r29,r3` after the call, `mr r3,r29` before the restore). Passing `1` is the same
  // instruction count but a different register, and it loses the value - which is the one thing
  // the pair exists for. Prime 1 spells it `const BOOL interrupts = ...; OSRestoreInterrupts(interrupts);`
  // and that is the whole fix.
  const u32 interrupts = OSDisableInterrupts();
  float stopwatchTime = mTickStopwatch.GetElapsedTime();
  mTickStopwatch.Reset();
  OSRestoreInterrupts(interrupts);
  sInfiniteLoopTime = 0.0f;
  mTickRemainder += stopwatchTime;
  // **`GetThirtyFps()`, not `GetFinished()`**, and the two are one `lbz` apart. Retail
  // 0x80007C64 is `lwz r3,gpMain ; lbz r0,145(r3) ; rlwinm. r0,r0,25,31,31` - it loads **+0x91**,
  // where `GetFinished()` (the first of the eight `bool : 1` at +0x90) makes us emit `144(r3)`.
  //
  // **The `rlwinm 25,31,31` is the same opcode in both cases and does not tell the two apart.**
  // `rlwinm rA,rS,25,31,31` tests bit `31-25`=6 of whatever `rS` holds, but the probe in
  // `docs/goal-notes/match-main-cmain-0x91-bitfield.md` shows mwcceppc emits exactly this pair
  // for *both* the first and the ninth one-bit field:
  //
  //     bool b0 : 1;  ->  lbz r0,0(r3) ; rlwinm r3,r0,25,31,31
  //     bool b8 : 1;  ->  lbz r0,1(r3) ; rlwinm r3,r0,25,31,31
  //
  // so "bit 6 of the byte" is not a distinct field - it is the *first* field of whichever byte
  // was loaded, and only the displacement separates +0x90 from +0x91. Reading the rotate mask
  // as a bit index (which is what this comment's predecessor did, and what the goal item
  // `match-main-cmain-0x91-bitfield` was filed on) invents a ninth/other field that retail's
  // constructor never writes. `CMain`'s bitfield map needs no change: the constructor at
  // 0x80008940-0x8000899C writes exactly the eight fields at +0x90 and then `stw r8,148(r3)`,
  // and `mThirtyFps` is the ninth, which is why `SetThirtyFps` is the ninth too.
  if (gpMain->GetThirtyFps()) {
    mTickRemainder = 0.033333335f;
  }
  bool flag = gpMain->GetMaxSpeed();
  // `elapsed > 0.035f`, not `0.035 < elapsed`. Retail 0x80007C40 is
  // `lfs f0,lbl_8041A404 ; fcmpo cr0,f31,f0 ; ble` - the **elapsed** value is the first operand of
  // `fcmpo` and the branch is `ble`, so the operands are the other way round from ours and the
  // constant is the second. Spelled as written above mwcceppc emits `fcmpo cr0,f0,f31 ; bge`, which
  // is the same predicate with the operands swapped.
  if (flag || stopwatchTime > 0.035f) {
    gpMain->DecrementMaxSpeedDrawTimer(stopwatchTime);
    mTickRemainder = 0.016666668f;
  }
  // **Declared before the `Push`, and that is load-bearing.** Retail 0x80007CB4 is
  // `li r28,1` and it comes *before* `bl CreateFrameBegin` at 0x80007CBC, not after the
  // `Push` that follows it. Spelled after the call, `bool keepLooping = true;` is
  // materialised at 0x80007CDC instead and the whole tail of the function shifts by one
  // instruction: 0x80007CD8's `lfs f31` lands after the `addi r29,r1,16` instead of before
  // it, and the `bl` targets walk one slot out of step for the rest of the body.
  bool keepLooping = true;
  mArchQueue.Push(MakeMsg::CreateFrameBegin(kAMT_Game, mGameFrameCount));

  // `>=`: retail 0x80007D40 is `fcmpo` + `cror eq,gt,eq`.
  while (keepLooping || mTickRemainder >= 0.016666668f) {
    keepLooping = false;
    if (!mInputGenerator.Update(0.016666668f, mArchQueue)) {
      result = true;
    }
    mArchQueue.Push(MakeMsg::CreateTimerTick(kAMT_Game, 0.016666668f));
    mTickRemainder -= 0.016666668f;
    mIoWinMgr.PumpMessages(mArchQueue);
  }

  // Retail's epsilon is `lbl_8041A408` = 0.00005f, not `Real32::Epsilon()`.
  if (close_enough((mPreviousTickRemainder2 - mPreviousTickRemainder) + (mPreviousTickRemainder - mTickRemainder), 0.0f, 0.00005f)) {
    mTickRemainder = 0.0f;
  }

  mPreviousTickRemainder2 = mPreviousTickRemainder;
  mPreviousTickRemainder = mTickRemainder;
  mIoWinMgr.PumpMessages(mArchQueue);
  // **Not quitting**, which `RsMain` tests: retail ends `cntlzw r0,r0 ; srwi r3,r0,5` on r31, the
  // "input generator failed" flag - `return !result`. Returned uninverted, `RsMain` set `finished`
  // on the first frame whose messages were actually pumped (found 2026-09-29).
  return !result;
}

// Retail 0x80007A14, 0x70 = 112 bytes, and this is its body one-for-one.
//
// `gpGameState->GetWorldState()->Update()` is the two calls retail makes - `bl` on
// `CGameState::GetWorldState` (0x80142520), the `lwz r3,0(r3)` that dereferences the reference it
// returns, and `bl` on `CWorldTransManagerView::Update` (0x8015B9B0). **No null test on the world state**,
// unlike `CWorldTransManagerView::Update`'s own guard: retail's `CGameState` constructor always fills +0x3C,
// and adding a test here drops the function from 100% to 84.78%.
//
// Both callees are left undefined here: retail's bodies are in other units' ranges, and this unit
// is `NonMatching`, so `dtk dol split` supplies retail's bytes for the whole claim and the two
// relocations land on retail's own addresses.
void CGameArchitectureSupport::Update() {
  gpGameState->GetWorldState()->Update();
  mArchQueue.Push(MakeMsg::CreateFrameEnd(kAMT_Game, mGameFrameCount));
  mIoWinMgr.PumpMessages(mArchQueue);
}

// Retail 0x80007AA0, 0x28 = 40 bytes: `rstl::list<CArchitectureMessage>::push_back`, called out
// of line by the `Push` below. The body is the header's `push_back` verbatim - `mr r5,r4` (the
// value into the third argument), `lwz r4,8(r3)` (`mEnd` is a *stored* node pointer, hence the
// load rather than `addi`), one `bl do_insert_before` - and mwcceppc already emitted exactly
// these 40 bytes, but as the weak COMDAT
// `push_back__Q24rstl55list<20CArchitectureMessage,Q24rstl17rmemory_allocator>FRC20CArchitectureMessage`,
// which objdiff cannot pair with `fn_80007AA0` (dtk's map has no name for 0x80007AA0).
// Spelling it out under retail's name is what turns those 40 bytes from an "extra" into a match.
//
// `push_back` itself cannot be written here: with `-inline deferred,noauto` only functions
// declared `inline` are expanded, and marking the shared `rstl::list` member `inline` would
// expand it into every `push_back` call site in the DOL - including this `Push`, which retail
// leaves as a bare call. `mEnd` is private and `do_insert_before` is public, so the member is
// reached through `end()`, which the compiler folds back to the same `mEnd` load.
extern "C" void fn_80007AA0(rstl::list< CArchitectureMessage >* self,
                            const CArchitectureMessage& val) {
  self->do_insert_before(self->end().get_node(), val);
}

// Retail 0x80007A80, 0x20 = 32 bytes: a frame, the one call, the frame out. `push_back` on the
// `rstl::list` is out of line in retail (`fn_80007AA0` above), and so it is here: the call target
// is named, not inlined.
void CArchitectureQueue::Push(const CArchitectureMessage& msg) { fn_80007AA0(&mQueue, msg); }

// Retail 0x80007B20, 0xBC = 188 bytes. Prime 1's `CMain::MemoryCardInitializePump` is this
// function one call short of it: Echoes additionally seeds the system options from the card
// before `CGameState::InitializeMemoryStates`, and the measured bytes at 0x80007C10 are
// `lwz r3,gpGameState ; addi r3,r3,0x54 ; bl CPersistentOptions::InitializeMemoryState` -
// `+0x54` is `CGameState::mSystemOptions` (`include/MetroidPrime/Player/CGameState.hpp:291`).
//
// The allocation is written out rather than spelled `new`, for the reason the two
// `MakeCGameState`/`MakeInGameTweakManager` helpers above already carry: the `__nw__FUlPCcPCc`
// call site is one of the two that passes retail's `.rodata` pool as `operator new`'s file
// operand, and the `CMemoryCard* made = self; if (made != 0) { made = f(made); } return made;`
// shape is what puts the constructor's result in **r30** rather than leaving it in r3, which is
// what retail does at 0x80007C10 (`mr r30,r3` after the null test).
extern "C" CMemoryCard* fn_80177FF0(CMemoryCard* self);
static inline CMemoryCard* MakeCMemoryCard() {
  CMemoryCard* self = static_cast< CMemoryCard* >(::operator new(sizeof(CMemoryCard)));
  CMemoryCard* made = self;
  if (made != 0) {
    made = fn_80177FF0(made);
  }
  return made;
}

void CMain::MemoryCardInitializePump() {
  if (gpMemoryCard == nullptr) {
    if (mGameGlobalObjects->MemoryCard().get() == nullptr) {
      mGameGlobalObjects->MemoryCard() = MakeCMemoryCard();
    }
    CMemoryCard* card = mGameGlobalObjects->MemoryCard().get();
    if (card->InitializePump()) {
      gpMemoryCard = card;
      gpGameState->SystemOptions().InitializeMemoryState();
      gpGameState->InitializeMemoryStates();
    }
  }
}

// Retail 0x800078F8, 0x60 = 96 bytes, and the whole body is what the compiler generates for a
// deleting destructor of a class whose only member is a base: store the derived vtable pointer back
// over the object's first word, call the base destructor with the flag zeroed, then release the
// object when the incoming flag is positive. `li r4,0` before the base call is exactly the "not
// deleting" flag the base's own D1 test reads.
//
// The store's `@ha`/`@l` pair relocates against `__vt__18CErrorOutputWindow`, which is
// `MetroidPrime/CErrorOutputWindow.cpp`'s `.data` object at 0x803B5910 - the same symbol retail's
// constructor reaches. mwcceppc also lays a 28-byte copy of that vtable down in this object,
// because the class's key functions are all undefined here; that `.data` is not claimed by
// `config/G2ME01/splits.txt` for this unit, which is one of the reasons it is not a flip candidate
// (see `docs/research/decl_order.md`).
inline CErrorOutputWindow::~CErrorOutputWindow() {}

void CGameGlobalObjects::AddPaksAndFactories(COsContext& context) {
  CResFactory& factory = *gpResourceFactory;
  CGraphics::SetViewPointMatrix(CTransform4f::Identity());
  CGraphics::SetModelMatrix(CTransform4f::Identity());
  if (CDvdFile::FileExists("Strings.pak")) {
    factory.GetResLoader().AddPakFileAsync(rstl::string_l("aram:Strings"), false, false);
  }

  CDvdFile tweakFile("Standard.NTWK");
  struct TweakData {
    uchar* data;
    TweakData(uchar* ptr) : data(ptr) {}
    ~TweakData() { CMemory::Free(data); }
  } tweakBuf(static_cast< uchar* >(CMemory::Alloc(tweakFile.Length(), IAllocator::kHI_RoundUpLen)));
  rstl::single_ptr< CDvdRequest > request(tweakFile.SyncRead(tweakBuf.data, tweakFile.Length()));
  CRELFileToken tweaks(rstl::string_l("Tweaks.rel"), 1);
  tweaks.Load();
  factory.GetResLoader().AddPakFileAsync(rstl::string_l("NoARAM"), false, false);
  factory.GetResLoader().AddPakFileAsync(rstl::string_l("AudioGrp"), false, false);
  factory.GetResLoader().AddPakFileAsync(rstl::string_l("aram:MiscData"), false, false);
  factory.GetResLoader().AddPakFileAsync(rstl::string_l("aram:TestAnim"), true, false);
  factory.GetResLoader().AddPakFileAsync(rstl::string_l("aram:MidiData"), false, false);
  factory.GetResLoader().AddPakFileAsync(rstl::string_l("aram:GGuiSys"), false, false);
  if (CDvdFile::FileExists("FrontEnd.pak")) {
    factory.GetResLoader().AddPakFileAsync(rstl::string_l("FrontEnd"), false, true);
  }

  CErrorOutputWindow errors(CErrorOutputWindow::kF_One);
  CGraphics::SetIsBeginSceneClearFb(true);
  CGraphics::SetViewport(0, 0, CGraphics::GetViewport().mWidth, CGraphics::GetViewport().mHeight);
  rstl::single_ptr< IController > controller(IController::Create(context));
  gpController = controller.get();
  while (!factory.GetResLoader().AreAllPaksLoaded() || !request->IsComplete() || !tweaks.IsLoaded()) {
    gpResourceFactory->GetResLoader().AsyncIdlePakLoading();
    gpRelFileManager->Update();
    errors.Update();
    CGraphics::BeginScene();
    errors.ShowMessage();
    CGraphics::EndScene();
    if (controller.get() != nullptr) {
      controller->Poll();
    }
    gpMain->CheckReset();
  }
  gpController = nullptr;
  {
    CMemoryInStream stream(tweakBuf.data, tweakFile.Length(), CMemoryInStream::kOS_NotOwned);
    LoadTweaks(stream);
    CreateTweakGlobals();
    tweaks.Unload();
  }

  factory.GetFactoryMgr().AddFactory('STRG', FStringTableFactory);
  factory.GetFactoryMgr().AddFactory('CMDL', FModelFactory);
  factory.GetFactoryMgr().AddFactory('TXTR', FTextureFactory);
  factory.GetFactoryMgr().AddFactory('CSKR', FSkinRulesFactory);
  factory.GetFactoryMgr().AddFactory('ANIM', AnimSourceFactory);
  factory.GetFactoryMgr().AddFactory('CINF', FCharLayoutInfo);
  factory.GetFactoryMgr().AddFactory('ANCS', FAnimCharacterSet);
  factory.GetFactoryMgr().AddFactory('CRSC', FCollisionResponseDataFactory);
  factory.GetFactoryMgr().AddFactory('SWHC', FParticleSwooshDataFactory);
  factory.GetFactoryMgr().AddFactory('PART', FParticleFactory);
  factory.GetFactoryMgr().AddFactory('ELSC', FParticleElectricDataFactory);
  factory.GetFactoryMgr().AddFactory('SPSC', FSpawnParticleSystemDataFactory);
  factory.GetFactoryMgr().AddFactory('SRSC', FSortedParticleSystemDataFactory);
  factory.GetFactoryMgr().AddFactory('WPSC', FProjectileWeaponDataFactory);
  factory.GetFactoryMgr().AddFactory('FRME', RGuiFrameFactoryInGame);
  factory.GetFactoryMgr().AddFactory('FONT', FRasterFontFactory);
  factory.GetFactoryMgr().AddFactory('SCAN', FScannableObjectInfoFactory);
  factory.GetFactoryMgr().AddFactory('AFSM', FAiFiniteStateMachineFactory);
  factory.GetFactoryMgr().AddFactory('FSM2', FAiStateMachine2Factory);
  factory.GetFactoryMgr().AddFactory('AGSC', FAudioGroupSetLocDataFactory);
  factory.GetFactoryMgr().AddFactory('DCLN', FCollidableOBBTreeGroupFactory);
  factory.GetFactoryMgr().AddFactory('DPSC', FDecalDataFactory);
  factory.GetFactoryMgr().AddFactory('ATBL', FAudioTranslationTableFactory);
  factory.GetFactoryMgr().AddFactory('PATH', FPathFindAreaFactory);
  factory.GetFactoryMgr().AddFactory('MAPW', FMapWorldFactory);
  factory.GetFactoryMgr().AddFactory('MAPA', FMapAreaFactory);
  factory.GetFactoryMgr().AddFactory('MAPU', FMapUniverseFactory);
  factory.GetFactoryMgr().AddFactory('CSNG', FMidiDataFactory);
  factory.GetFactoryMgr().AddFactory('DGRP', FDependencyGroupFactory);
  factory.GetFactoryMgr().AddFactory('SAVW', FSaveWorldFactory);
  factory.GetFactoryMgr().AddFactory('HINT', FHintFactory);
  factory.GetFactoryMgr().AddFactory('CSPP', FSpatialPrimitivesFactory);
  factory.GetFactoryMgr().AddFactory('PTLA', FPortalAreaDataFactory);
  factory.GetFactoryMgr().AddFactory('STLC', FStringListFactory);
  factory.GetFactoryMgr().AddFactory('EGMC', FEditorGeometryToStaticGeometryFactory);
  factory.GetFactoryMgr().AddFactory('RULE', FRuleSetFactory);
}

// ---------------------------------------------------------------------------------------------
// Retail 0x800064D0-0x8000661C and 0x80006AE0: `CGameGlobalObjects`' own D1 teardown, its
// `single_ptr<CGameGlobalObjects>::operator=`, and `single_ptr<CGameGlobalObjects>::~single_ptr()`.
// The first two are written here by hand (0x80006518 further down, as an `extern "C"` function
// named `__dt__18CGameGlobalObjectsFv`); 0x80006AE0 is emitted by the `single_ptr<CGameGlobalObjects>`
// instantiation, which `config/G2ME01/symbols.txt` names
// `__dt__Q24rstl32single_ptr<18CGameGlobalObjects>Fv`.
//
// **`single_ptr_assign_800064D0` is still a `dtk` placeholder** (`config/G2ME01/symbols.txt:122`),
// and that is load-bearing for the same reason `__dt__80006678` below is called by name: objdiff
// pairs functions **by name**, and the natural C++ spelling comes out as
// `__as__Q24rstl32single_ptr<18CGameGlobalObjects>FP18CGameGlobalObjects`, which `dtk` never named.
// It is an `extern "C"` free function for that reason and for no other one.
//
// **The members are spelled with their own destructors, one at a time**, because that is what
// decides whether mwcceppc emits its `addic. r0,r30,off / beq` address guard, and retail guards
// exactly three of the ten teardowns:
//
//   member                      retail                              spelling that matches
//   x150_tail      +0x150       `addi ; li r4,-1 ; bl fn_801F097C`   `fn_801F097C(&..., -1)`
//   inGameTweak... +0x14C       `addi ; li r4,-1 ; bl 80006678`      `__dt__80006678(&..., -1)`
//   renderer       +0x148       `addic./beq ; lwz ; vtable +0x08`    `.~single_ptr<IRenderer>()`
//   stringTable    +0x138       `addic./beq ; lbz +0x144 ; beq ; ..` `.~optional_object<...>()`
//   memoryCard     +0x134       `addic./beq ; lwz ; li r4,1 ; bl`    `.~single_ptr<CMemoryCard>()`
//   gameState      +0x130       `addi ; li r4,-1 ; bl 80006620`      see below
//   characterF...  +0x108       `addi ; li r4,-1 ; bl`               `.~CCharacterFactoryBuilder()`
//   simplePool     +0x0E4       `addi ; li r4,-1 ; bl`               `.~CSimplePool()`
//   resFactory     +0x004       `addi ; li r4,-1 ; bl`               `__dt__11CResFactoryFv(..)`
//   pad0           +0x000       `mr ; li r4,-1 ; bl`                 `__dt__14CMemoryCardSysFv(..)`
//
// `delete member.get()` is the same code as the destructor call but **loses** the guard, which is
// the whole of the three that retail has, and `resFactory.~CResFactory()` would be the vtable
// dispatch `IFactory`'s `virtual ~IFactory() = 0` forces, seven instructions where retail has a
// plain `bl` of three - which is why that callee is declared under retail's own mangled name
// (`extern "C"` reproduces it verbatim) instead of being destroyed through the class.
// `gameState` is the one member retail does *not* guard, and the destructor spelling is what stops
// it matching, so it gets neither: a call, which is the three instructions retail has.
//
// The `-1` on every one of those calls is the "not deleting" flag, the incoming flag is a `short`
// (`extsh.`, not the `extsb.` a `bool` gives), and `if (flag > 0)` has to be **inside** `if (self)`
// so that the `beq` lands on the epilogue rather than on the `extsh.` - one nibble of one word,
// and 99.76% instead of 100%.
//
// **`single_ptr_CGameState_dtor` is retail's 0x80006620 under a name a compiler will accept, and
// that is the only reason it exists.** Retail's own symbol for it is
// `__dt__Q24rstl24single_ptr<10CGameState>Fv` (`config/G2ME01/symbols.txt:124`), which **this
// object already emits and already matches at 100%** - it is the weak instantiation of the
// template. It
// cannot be *declared*, because a C++ identifier cannot contain `<` or `>`, and
// `__asm__("...")` after a declarator is rejected by this compiler, so there is no spelling of the
// call that names it. Retail's own `addi r3,r30,304 ; li r4,-1 ; bl 80006620` is a call to the
// **out-of-line** instantiation, and mwcceppc inlines `~single_ptr<CGameState>()` at every spelling
// here, which is five instructions with an `addic. r0,r30,304 / beq` guard where retail has three
// without one - measured, and it costs 8 bytes and 3.03 percentage points on the whole function.
//
// Declaring the callee under a writable name and calling it reproduces retail's three instructions
// exactly, and objdiff scores the function **100.00%** where the inlined spelling reads 96.05% -
// **objdiff does not compare `R_PPC_REL24` targets**, so the call's *target name* is not what the
// score sees (measured: this call, and only this call, is the difference between the two numbers).
// It is declared and never defined, like the 126 other callees `src/MetroidPrime/main.cpp` already
// declares without a body, and `MetroidPrime/main.cpp` is `NonMatching` in `configure.py`, so its
// object is not in the DOL link and an undefined reference here is what every other unit does.
// Defining it instead would put a second copy of retail's 0x80006620 body in this object next to
// the weak instantiation that already is retail's 0x80006620.
extern "C" void* single_ptr_CGameState_dtor(rstl::single_ptr< CGameState >*, short);
extern "C" void fn_801F097C(CRELFileManager*, short);
extern "C" void fn_80008B04(void*);
extern "C" void __dt__11CResFactoryFv(CResFactory*, short);
extern "C" void __dt__14CMemoryCardSysFv(CMemoryCardSys*, short);
extern "C" void* __dt__80006678(rstl::single_ptr< CInGameTweakManager >*, short);

// Retail 0x800064D0, 0x48 = 72 bytes: `single_ptr<CGameGlobalObjects>::operator=` taking a
// `CGameGlobalObjects* const`, whose body is the one `include/rstl/single_ptr.hpp` already spells -
// destroy the old pointer with the deleting flag, store the new one, return `*this`. The
// `mr r3,r30` in retail's epilogue is that `return *this`, and without it the frame is 4 bytes
// short.
//
// The template member itself is **not** called: mwcceppc does not inline it here, so `*self = ptr`
// came out as an 8-instruction thunk onto the weak
// `__as__Q24rstl32single_ptr<18CGameGlobalObjects>FP18CGameGlobalObjects`, which is both a new
// function in the object and half of retail's 18 instructions. And it is the call below rather than
// `delete self->mPtr` that destroys the old pointer, because `delete` made this compiler emit a
// *second* copy of the teardown - the implicit `__dt__18CGameGlobalObjectsFv`, 252 bytes - as one
// more function this object has and retail's does not. Both spellings byte-identical here.
extern "C" void* single_ptr_assign_800064D0(rstl::single_ptr< CGameGlobalObjects >* self,
                                            CGameGlobalObjects* ptr) {
  delete self->mPtr;
  self->mPtr = ptr;
  return self;
}

// ---------------------------------------------------------------------------------------------
// Retail 0x80006678-0x800068F4 is the whole teardown of `CGameGlobalObjects`' `+0x14C` member,
// `rstl::single_ptr<CInGameTweakManager>`, which `~CGameGlobalObjects` reaches at 0x8000654C
// (`addi r3,r30,332 ; li r4,-1 ; bl 80006678`). `+0x14C` is the member `include/MetroidPrime/
// CGameGlobalObjects.hpp` names `mInGameTweakManager`, and 0x80008508 allocates it with `li r3,16`
// and 0x80008514 runs `fn_8016C230` (the tweak manager's own constructor) on the result.
//
// **Only 0x80006678 is carved here now.** Everything else in that range - 0x800066D0
// (`CInGameTweakManager`'s destructor), 0x80006724 (`vector<CTweakValue>`'s), 0x800067A8 /
// 0x800067E0 (the `pointer_iterator` destroy / destroy_impl), 0x80006830 / 0x80006850 (the
// `CTweakValue` delete / destroy pair) and 0x80006874 (`CTweakValue`'s destructor) - gets its bytes
// from the real member or template instantiation, because `config/G2ME01/symbols.txt` gives those
// addresses their upstream names (`__dt__19CInGameTweakManagerFv`,
// `__dt__Q24rstl48vector<11CTweakValue,...>Fv`, `__dt__11CTweakValueFv`, ...) instead of
// placeholders. objdiff pairs functions **by name**, so the carves are not just redundant but
// harmful.

// Retail 0x80004864 (0x80004864, 0x38 = 56 bytes) reads `*(u32*)r4` into r5 and `*(u32*)r3` into
// r0, stores them on its own frame and hands the pair to `fn_8000489C` - which walks `first` to
// `last` in strides of **12** calling `__dt__6CTokenFv` on each element's first word
// (`mr r3,r31 / li r4,0 / bl 8030154c` at 0x800048C8, `addi r31,r31,12` at 0x800048D4). So its
// two arguments are **pointers to pointers** and the block's elements are 12 bytes.
//
// Retail 0x800068F4, 0x60 = 96 bytes, and it is that walk over `CGameState`'s `x1f4` block:
// `lwz r0,4(r31)` / `lwz r5,12(r31)` / `mulli r0,r0,12` / `add r5,r5,r0` are `x04_count` and
// `x0c_data`, which `CGameStateBlocks.hpp:22-26` already measured for every `SGameStateBlock`.
// Both callers pass `gpGameState + 500` (`CMain::RsMain` 0x8000637C, `fn_80143E88` 0x80143EA0),
// and 500 = 0x1F4 is `CGameState::x1f4` (`CGameState.hpp:307`), the block the constructor zeroes
// at `stw r0,504/508/512(r30)` (0x801442CC/D4/D8). The two lines after the call - `stw r0,4(r31)`
// here against `Free(x0c_data)` in the destructor at 0x800047E0, whose store sequence is
// otherwise identical - are what make this the block's *clear* rather than its destructor. The
// walk stays a `bl`: `fn_80004864` lives below this unit's claim at 0x800053B8.
extern "C" void fn_80004864(const void* first, const void* last);

// **The four locals and the `volatile` on two of them are load-bearing, and both are measured.**
// Without the two `volatile` qualifiers mwcceppc folds the copies and emits two `stw`s where
// retail has four (`lwz r5,12(r31)` then `stw r5,12(r1) / stw r5,8(r1) / stw r0,16(r1) /
// stw r0,20(r1)`) - 90.875% and 22 instructions against retail's 24. The copies are the two
// *unused* arguments of `fn_80004864`: retail passes r1+0x14 / r1+0x0C and stores the same two
// values into r1+0x08 and r1+0x10, so its source keeps four address-taken iterators and hands two
// of them over. `volatile` is what stops the register allocator from proving the copies redundant.
//
// The `u8* end` temporary in the body is the second load-bearing line: written as
// `last = base + count * 12; lastCopy = last;` the multiply lands in r0 and the sum in r0, retail
// has `mulli r0,r0,12 / add r5,r5,r0` - i.e. the sum in the register `x0c_data` was loaded into.
// Introducing the temporary first and assigning both copies from **it** (not from `last`) makes
// mwcceppc keep the base register as the accumulator. Both shapes were compiled and scored:
// 98.75% and 100.0% respectively, same 24 instructions either way.
extern "C" void fn_800068F4(SGameStateBlock* self) {
  u32 count = self->x04_count;
  u8* first;
  u8* volatile firstCopy;
  u8* last;
  u8* volatile lastCopy;
  u8* end = reinterpret_cast< u8* >(self->x0c_data) + count * 12;
  last = end;
  lastCopy = end;
  firstCopy = reinterpret_cast< u8* >(self->x0c_data);
  first = reinterpret_cast< u8* >(self->x0c_data);
  fn_80004864(static_cast< const void* >(&first), static_cast< const void* >(&last));
  self->x04_count = 0;
}

// Retail 0x80006678, 0x58 = 88 bytes, and it is byte-identical to 0x80006620 -
// `__dt__Q24rstl24single_ptr<10CGameState>Fv`, which this unit already matches at 100% - apart
// from the one `bl`. The template is instantiated for `CGameState` there because the header's
// `CGameState` has a real destructor; `CInGameTweakManager` is four `uint`s in this tree, so its
// `~single_ptr()` comes out 4 bytes shorter and cannot be retail's. Naming retail's own symbol and
// writing the 88 bytes is what fixes that, and it is the only reason this block is `extern "C"`.
extern "C" void* __dt__80006678(rstl::single_ptr< CInGameTweakManager >* self, short flag) {
  if (self) {
    delete self->get();
    if (flag > 0) {
      CMemory::Free(self);
    }
  }
  return self;
}

// Retail 0x800070FC, 0x6C = 108 bytes. The first call arms `lbl_80418ED4` and every call after it
// returns immediately, so the counter below only ever runs once per load; the `extsb.`/`bne` pair
// is that test and the `stb r0(=1)` is the arm. `cntlzw`/`srwi r4,5` is `counter == 0`.
void CMain::DrawDebugMetrics(double, CStopwatch&) {
  static uint counter = 0;
  ++counter;
  if (counter == 1800) {
    counter = 0;
  }
  CMemory::GetMetrics(counter == 0, false);
}

bool CMain::CheckTerminate() { return false; }

CInGameTweakManager::~CInGameTweakManager() {}

// Retail 0x80006518, 0x108 = 264 bytes: `CGameGlobalObjects::~CGameGlobalObjects`. Written as an
// `extern "C"` function under the destructor's own mangled name, because the implicit member
// teardown guards the wrong members (92.09%); the table above the 0x800064D0 block says which
// spelling each member needs. This file is not in the host link, so nothing else defines it.
extern "C" void* __dt__18CGameGlobalObjectsFv(CGameGlobalObjects* self, short flag) {
  if (self) {
    fn_801F097C(&self->mRelFileManager, -1);
    __dt__80006678(&self->mInGameTweakManager, -1);
    self->mRenderer.~single_ptr< IRenderer >();
    self->mStringTable.~optional_object< TLockedToken< CStringTable > >();
    self->mMemoryCard.~single_ptr< CMemoryCard >();
    single_ptr_CGameState_dtor(&self->mGameState, -1);
    self->mCharacterFactoryBuilder.~CCharacterFactoryBuilder();
    self->mSimplePool.~CSimplePool();
    __dt__11CResFactoryFv(&self->mResFactory, -1);
    __dt__14CMemoryCardSysFv(&self->mMemoryCardSys, -1);
    if (flag > 0) {
      fn_80008B04(self);
    }
  }
  return self;
}

// Retail 0x80007040 and 0x800070A4 are no longer hand-written here. `config/G2ME01/symbols.txt`
// names them `__ct__Q210CGameState20SPreviousGameResultsFv` and
// `__ct__Q24rstl48reserved_vector<Q210CGameState13SPlayerResult,4>FiRCQ210CGameState13SPlayerResult`,
// so each address is emitted by the real upstream-named constructor rather than by a local carve.
// They are `CGameState`'s `+0x1A0` block, whose extent (0x54) and record array (`x14_rec[4][16]`,
// a view onto `char x1a4_[0x50]`) `include/MetroidPrime/Player/CGameStateBlocks.hpp:103-111`
// already measures.

// ---------------------------------------------------------------------------
// Retail 0x800069AC and 0x80006954 are no longer hand-written here either. `CMain::RsMain`
// (0x80005C6C) calls both - four times in the frame-time loop at 0x80005D0C/0x80005D18/
// 0x80006108/0x80006228 and twice at 0x80006114/0x80006234 - and `symbols.txt` names them
// `AddValue__21TReservedAverage<f,4>FRCf` (0x800069AC, 0x134) and
// `GetAverage__21TReservedAverage<f,4>CFv` (0x80006954, 0x58), so `include/Kyoto/
// TReservedAverage.hpp`'s own members are instantiated through `CMain::mTickTimes` /
// `CMain::mDrawTimes` (both `TReservedAverage<float, 4>`) and supply retail's bytes.
// `AddValue` is 308 bytes and `GetAverage` is 88, both over the unit's
// `-pragma "inline_max_size(125)"`, so each is emitted out of line, at retail's own address.
// The class layout they read is `rstl::reserved_vector<float, 4>`, i.e.
// `{ int mCount; float mData[4]; }`, with the count at +0 and `mData` inline at +4.
// ---------------------------------------------------------------------------

bool CMain::CheckReset() {
  const int resetPressed = OSGetResetButtonState();
  const CControllerGamepadData& pad = gpController->GetGamepadData(0);
  bool resetChord = true;
  for (int i = 0; i <= kBU_R && resetChord; ++i) {
    const bool pressed = pad.GetButton(static_cast< EButton >(i)).GetIsPressed();
    switch (i) {
    case kBU_B:
    case kBU_X:
    case kBU_Start:
      if (!pressed) {
        resetChord = false;
      }
      break;
    default:
      if (pressed) {
        resetChord = false;
      }
      break;
    }
  }
  if (resetChord) {
    if (mResetInputDelay >= 0.5f) {
      mSoftResetHoldTime += 1.f / 60.f;
      if (mSoftResetHoldTime > 0.5f) {
        mResetButtonHeld = true;
      }
    }
  } else {
    if (mResetInputDelay < 0.5f) {
      mResetInputDelay += 1.f / 60.f;
    }
    mSoftResetHoldTime = 0.f;
  }
  if (!resetPressed && mResetButtonHeld) {
    mResetRequested = true;
  }
  if (!CMemoryCardSys::mIsCardBusy && (mResetRequested || mManageCard || mGameExitReset)) {
    if (mArchSupport != nullptr && mArchSupport->IsInfiniteLoopAlarmSet()) {
      OSCancelAlarm(&mArchSupport->GetInfiniteLoopAlarm());
      mArchSupport->SetInfiniteLoopAlarmSet(false);
    }
    GXDrawDone();
    GXAbortFrame();
    if (!mGameExitReset) {
      gpGameState->GameOptions() = CGameOptions();
      gpGameState->PreviousGameResults() = CGameState::SPreviousGameResults();
      __PADDisableRecalibration(false);
    } else {
      CGameOptions& options = gpGameState->GameOptions();
      options.SetScreenBrightness(4, false);
      options.SetScreenPositionX(0, false);
      options.SetScreenPositionY(0, false);
      options.SetScreenStretch(0, false);
      __PADDisableRecalibration(true);
    }
    {
      CMemoryStreamOut stream(CSaveRegion::GetSaveBuffer(), CSaveRegion::kSaveBufferSize);
      CBitStreamWriter writer(stream);
      writer.WriteBits(!!CGraphics::GetProgressiveMode(), 1);
      gpGameState->GameOptions().PutTo(writer);
      gpGameState->PreviousGameResults().PutTo(writer);
      writer.WriteBits(!!sProgressiveModePrompt, 1);
      writer.FlushAll();
      if (writer.GetOutputStream().GetWrittenBytes() >= CSaveRegion::kSaveBufferSize) {
        rs_debugger_printf("Reset failed: Tried %d", stream.GetWrittenBytes());
      } else {
        OSReport("Wrote: %d\n", writer.GetOutputStream().GetWrittenBytes());
      }
    }

    gpGameState->GameOptions().EnsureOptions();
    VISetBlack(true);
    VIFlush();
    VIWaitForRetrace();
    if (mManageCard) {
      OSResetSystem(OS_RESET_HOTRESET, 0, true);
    } else if (DVDCheckDisk()) {
      AISetStreamPlayState(0);
      if (CAudioSys::mInitialized) {
        sndQuit();
      }
      void* savedOptions = CSaveRegion::GetSaveRegionStart();
      memcpy(savedOptions, CSaveRegion::GetSaveBuffer(), CSaveRegion::kSaveBufferSize);
      DCFlushRange(savedOptions, CSaveRegion::kSaveBufferSize);
      OSSetSaveRegion(savedOptions, CSaveRegion::GetSaveRegionEnd());
      OSResetSystem(OS_RESET_RESTART, 0, false);
    } else {
      OSResetSystem(OS_RESET_HOTRESET, 0, false);
    }
    mResetButtonHeld = false;
    mResetRequested = false;
    mGameExitReset = false;
    mManageCard = false;
    return true;
  }
  mResetButtonHeld = resetPressed;
  return false;
}

// Retail 0x80006B38, 0x48 = 72 bytes, one line. The resource name is a plain literal: it is
// +0x7C of retail's pool, between "Strings.pak" and CheckReset's "Reset failed: Tried %d", and
// the literal is what brings it into this unit's own pool at that offset. (It used to be
// `lbl_803A56C0 + 0x07C`, needed only while the pool lacked this string and CheckReset's
// two formats were misspelled, which shifted every later offset.)
void CMain::FillInAssetIDs() {
  gpSimplePool->fn_8029c7e8(*gpResourceFactory->GetResourceIdByName("sound_lookup_ATBL"));
}

// Retail 0x80005C64, 0x8 = 8 bytes: `stw r4, 0x48(r3) ; blr`. The only writer of
// `mFrameTimeMinimum` other than `CMain::AsyncIdle`, which clamps against it and clears it.
// Declared in `include/MetroidPrime/CMain.hpp` and **not** inline: nothing in the port calls
// it, so an inline body is never emitted and the function stayed at 0% in the matching build.
// Placed immediately before `CMain::RsMain` because 0x80005C64 is retail's order between
// `CMain::AsyncIdle` (0x80005B44) and `CMain::RsMain` (0x80005C6C).
void CMain::SetFrameTimeMinimum(uint time) { mFrameTimeMinimum = time; }

// Retail 0x80005C6C, 0x868 = 2152 bytes, **0.19% here** - the function is unwritten apart from
// the two allocations below, and this is the first of them.
//
// **Why this one function carries the item's other four matches.** `TOneStatic<T>`'s
// `operator new` and `GetAllocSpace` are template member functions: nothing but an allocation
// site brings them into a translation unit. Retail has exactly two allocation sites for the two
// `TOneStatic` classes, and they are both in this function:
//
//   0x80005CC0  lis r4,.. / li r3,356 / addi r4,r4,.. / li r5,0 / bl 0x80008AD4
//               mr. r0,r3 / beq / lwz r4,0(r31) / lwz r5,8(r31) / bl 0x8000848C   <- CGameGlobalObjects
//   0x80005E08  lis r4,.. / li r3,168 / addi r4,r4,.. / li r5,0 / bl 0x80008A48
//               mr. r0,r3 / beq / lwz r4,0(r31)                 / bl 0x80007EC4   <- CGameArchitectureSupport
//
// `li r3,356` and `li r3,168` are `sizeof` retail passes, and 0x8000848C / 0x80007EC4 are
// `CGameGlobalObjects::CGameGlobalObjects` and `CGameArchitectureSupport::CGameArchitectureSupport`,
// the two constructors this file already defines. So the allocation is spelled
// `TOneStatic<T>::operator new(sizeof(T), <file>, 0)` with an explicit `sizeof` and explicit
// arguments, because an ordinary `new T` would select the one-argument overload
// (`include/Kyoto/TOneStatic.hpp:34`) and retail has no such body in this range.
//
// **The arguments retail passes are not a filename.** `addi r4,r4,22208` after
// `lis r4,-32710` gives 0x802A56C0, which is inside `.text` (`readelf -S`: 0x80003840 +
// 0x3A1C54) and holds `li r4,0x1924 ; li r28,0x100`, and the same value is passed to both
// allocation sites, so it is not the class name either. `operator new` ignores both arguments -
// 0x80008AD4 reads only r3 - so they are passed as null here rather than given a value this
// measurement does not identify. **Naming that is the open question for a run that writes the
// rest of the function**; it costs the two 48-byte bodies nothing either way.
//
// The construction of the object over the storage `operator new` returns is retail's next
// instruction and is **not** written here: it needs a placement `operator new`, which this tree
// does not declare. `CGameGlobalObjects`'s constructor is in this file at line 338 and
// `CGameArchitectureSupport`'s at line 401, and both are `Matching`-shaped already, so the pair is
// in place when the rest of `RsMain` is written.
int CMain::RsMain(int argc, const char* const* argv) {
  PPCSetFpIEEEMode();
  CStopwatch startupTimer;
  if (GetLockedCacheAllocationBase() == LCGetBase()) {
    LCEnable();
  }
  rstl::single_ptr< CGameGlobalObjects > globalObjects(
      rs_new CGameGlobalObjects(*mOsContext, *mMemorySys));
  mGameGlobalObjects = globalObjects.get();
  CStringTable::SetLanguage(GetLanguage());
  for (int i = 0; i < 4; ++i) {
    mTickTimes.AddValue(0.3f);
    mDrawTimes.AddValue(0.2f);
  }
  mAverageTickTime = 0.3f;
  mAverageDrawTime = 0.2f;
  InitializeSubsystems();
  globalObjects->PostInitialize(*mOsContext, *mMemorySys);
  AddWorldPaks();

  {
    rstl::string audioTweaksStatus;
    if (gpTweakManager->ReadFromMemoryCard(rstl::string_l("AudioTweaks"))) {
      audioTweaksStatus = rstl::string_l("Loaded audio tweaks from memory card\n");
    } else {
      audioTweaksStatus = rstl::string_l("FAILED to load audio tweaks from memory card\n");
    }
    FillInAssetIDs();
    rstl::single_ptr< CGameArchitectureSupport > architecture(
        rs_new CGameArchitectureSupport(*mOsContext));
    mArchSupport = architecture.get();
    srand(startupTimer.GetElapsedMicros());
    if (CSaveRegion::GetNonVolatileSettingsBuffer() != nullptr) {
      CMemoryInStream stream(CSaveRegion::GetNonVolatileSettingsBuffer(),
                             CSaveRegion::kSaveBufferSize);
      CBitStreamReader reader(stream);
      reader.ReadBits(1);
      gpGameState->GameOptions() = CGameOptions(reader);
      gpGameState->PreviousGameResults() = CGameState::SPreviousGameResults(reader);
      gpGameState->GameOptions().EnsureOptions();
      sProgressiveModePrompt = reader.ReadPackedBool();
    }
    const int gameMode = gpGameState->GetGameModeType();
    if (gameMode != 'COIN' && gameMode != 'DTHM') {
      architecture->GetIOWinManager().AddIOWin(
          rs_new CSplashScreen(CSplashScreen::kSplashScreen_Nintendo), 1000, 10000);
    }
    CDvdFile::FileExists("Strings.pak");

    while (!mFinished) {
      CStopwatch& drawTimer = architecture->GetStopwatch2();
      drawTimer.Reset();
      gpResourceFactory->GetResLoader().AsyncIdlePakLoading();
      gpRelFileManager->Update();
      if (gpMemoryCard == nullptr && gpResourceFactory->GetResLoader().AreAllPaksLoaded()) {
        MemoryCardInitializePump();
      }
      CARAMManager::CollectGarbage();
      CARAMToken::UpdateAllDMAs();
      if (!architecture->UpdateTicks()) {
        mFinished = true;
      }
      const double tickTime = drawTimer.GetElapsedTime();
      mTickTimes.AddValue(tickTime / (1.f / 60.f));
      mAverageTickTime = *mTickTimes.GetAverage();
      drawTimer.Reset();

      bool drawFrame = true;
      if (GetMaxSpeed()) {
        AsyncIdle(1000000);
        if (mMaxSpeedDrawTimer > 0.f) {
          drawFrame = false;
          CFrameDelayedKiller::FlushAllocationsForFrame();
          CFrameDelayedKiller::FlushAllocationsForFrame();
        } else {
          mMaxSpeedDrawTimer = 1.f;
        }
      }
      if (drawFrame) {
        gpRender->BeginScene();
        architecture->GetIOWinManager().Draw();
        DrawDebugMetrics(tickTime, drawTimer);
        const double drawTime = drawTimer.GetElapsedTime();
        mDrawTimes.AddValue(drawTime / (1.f / 60.f));
        mAverageDrawTime = *mDrawTimes.GetAverage();
        gpRelFileManager->Update();
        const double idleTime = (1.f / 60.f - (tickTime + drawTimer.GetElapsedTime())) - 0.00075;
        AsyncIdle(idleTime <= 0.0 ? 0 : static_cast< uint >(idleTime * 1000000.0));
        if (gpMain->GetThirtyFps()) {
          const float waitTime =
              1.f / 30.f - static_cast< float >(tickTime + drawTimer.GetElapsedTime());
          if (waitTime > 0.f) {
            CStopwatch::Wait(waitTime);
          }
        }
        gpRender->EndScene();
        if (mGameFrameDrawn) {
          ++architecture->GetFramesDrawn();
          mGameFrameDrawn = false;
        }
      } else {
        gpResourceFactory->AsyncIdle(1000000, false);
      }
      architecture->Update();
      CSfxManager::Update(1.f / 60.f);
      UpdateStreamedAudio();
      if (CheckTerminate()) {
        gpGameState->ClearAudioGroups();
        break;
      }
      if (architecture->GetIOWinManager().IsEmpty() || CheckReset()) {
        mRestartMode = kRM_Default;
        CStreamAudioManager::StopAll();
        PADRecalibrate(0xf0000000);
        CGraphics::SetIsBeginSceneClearFb(true);
        CGraphics::BeginScene();
        CGraphics::EndScene();
        CFrameDelayedKiller::StallAndFlushAllAllocations();
        architecture = nullptr;
        architecture = rs_new CGameArchitectureSupport(*mOsContext);
        mArchSupport = architecture.get();
      }
    }
  }

  ShutdownSubsystems();
  globalObjects = nullptr;
  CARAMManager::Shutdown();
  return 0;
}

// Retail 0x80005B44, 0x120 = 288 bytes, **99.17%** (was 85.94%). The whole body is
// instruction-for-instruction retail's except the last argument setup, and the two
// decompositions below are what make it so. Both were measured; neither is a cosmetic rewrite.
//
//  1. The clamp result must be a **separate variable** from the parameter, and its `5000` arm
//     must be the *fall-through* with `time` as the branch target. Retail 0x80005BF0 is
//     `cmplwi r4,5000 / li r31,5000 / bgt / mr r31,r4`, which only the initialiser spelling
//     lays out that way: `t = (time <= 5000) ? time : 5000` gives 11 differing instructions and
//     `time = time; if (time > 5000) { time = 5000; }` gives 5. It is also what lets the
//     parameter stay in `r4` and the clamp live in `r31` across `GetMaxSpeed()`'s call.
//  2. The flag must be **initialised before the test**, not assigned from it.
//     `bool flag = GetMaxSpeed();` scores 18 differing instructions because mwcceppc then keeps
//     the result in a volatile and never spills, so retail's `r30` leaves the prologue and the
//     epilogue entirely; `bool flag = false;` with the assignment inside the `if` reproduces
//     retail's `li r30,0` / `li r30,1` and the `stw r30,8(r1)` spill.
//
// **The last instruction is a `bool` that mwcceppc will not mask, and a `static_cast` fixes it.**
// Retail 0x80005C44 is `clrlwi r5,r30,24` where this used to be `mr r5,r30` - the byte mask
// mwcceppc emits when a **one-byte** value goes into an argument register. Seventeen
// argument- and local-type spellings were compiled and measured first and every one of them is
// worse, because they change the *type* of the value and mwcceppc then also has to normalise it:
//
//   bool (retail's) 99.17   uchar local 94.79   char local 94.79   uint local 96.18
//   (uchar)flag 94.79   (char)flag 94.79   (bool)(uchar)flag 94.79   flag | 0 94.79
//   flag != 0 94.79   bool flag = GetMaxSpeed() != 0 87.68   uchar flag = (uchar)fn() 89.49
//   signed char / char local: `extsb r5,r30` then the same normalisation   int/uint local:
//   `neg r0,r30 ; or r0,r0,r30 ; srwi` - 96.7 either way
//
// The mask is emitted for a one-byte **value**, but a `bool` local never needs one, because
// mwcceppc range-analyses it: it only ever holds 0 or 1. Measured on this unit's exact
// `build.ninja` flags, the mask appears when the value is a **copy** the compiler cannot see
// through - a `const bool` initialised from the local - and it appears *bare*, with no
// normalisation, because the copy is already `bool`. Declaring `CResFactory::AsyncIdle`'s second
// parameter `unsigned char` also emits it, and is still wrong for the reason below.
//
// Two details are load-bearing and both were measured:
//   * the `const bool` copy must be declared **inside** the `if (t != 0)` body. Hoisted above it,
//     mwcceppc schedules the conversion before the branch (`beq`), retail has it after the guard;
//   * the initialiser must be `static_cast< bool >(flag)` and not the bare `flag`. A bare `flag`
//     gives retail's bytes in the wrong order - `clrlwi` before `mr r4,r31` instead of after -
//     and the explicit cast, which is a no-op to the language, is what orders them.
//
// (`flag ? true : false` at the call is the third shape that produces a bare `clrlwi`, but it
// costs three extra instructions: `neg r0,r5 ; or r0,r0,r5 ; srwi r5,r0,31`, mwcceppc's
// `int`-to-`bool` normalisation of the conditional's value.)
//
// Declaring `CResFactory::AsyncIdle`'s second parameter `unsigned char` **is** byte-exact
// (100.00%, measured) and is still wrong: it renames the callee to
// `AsyncIdle__11CResFactoryFUiUc`, and `main/Kyoto/CResFactory` is a `NonMatching` unit whose
// own `AsyncIdle__11CResFactoryFUib` body is at 100.0% (268 bytes) under the name symbols.txt
// gives it. Measured cost of taking the point: `main/Kyoto/CResFactory` 35 -> 34 functions,
// `matched_code` 5532 -> 5408, against `main/MetroidPrime/main` 62 -> 63. Net negative, and it
// leaves `FUiUc` undefined at DOL link. Retail's own mangling says the parameter is `bool`, so
// the clrlwi is mwcceppc's narrowing of an argument it already knows is 0 or 1.
void CMain::AsyncIdle(uint time) {
  if (time < 500) {
    uint total = 0;
    for (int i = 0; i < mFrameTimes.capacity(); ++i) {
      total += mFrameTimes[i];
    }
    if (total < 500 * mFrameTimes.capacity()) {
      time = 500;
    } else {
      time = 0;
    }
  }
  mFrameTimes[mFrameTimeIdx] = time;
  mFrameTimeIdx = mFrameTimeIdx + 1;
  if (mFrameTimeIdx >= mFrameTimes.capacity()) {
    mFrameTimeIdx = 0;
  }

  uint t = 5000;
  if (time <= 5000) {
    t = time;
  }
  if (t < mFrameTimeMinimum) {
    t = mFrameTimeMinimum;
  }
  mFrameTimeMinimum = 0;
  bool flag = false;
  if (GetMaxSpeed()) {
    flag = true;
    t = 1000000;
  }

  if (t != 0) {
    const bool keepPumping = static_cast< bool >(flag);
    gpResourceFactory->AsyncIdle(t, keepPumping);
  }
}

// `__pl__4rstlFRCQ24rstl66basic_string<c,...>RCQ24rstl66basic_string<c,...>`
//   = .text:0x80005AE8, 0x5C = 92 bytes, weak.
//
// The Itanium mangling of `operator+` is `pl`, so this is `rstl::operator+(const string&,
// const string&)` - declared at `include/rstl/string.hpp:369` and never defined by any unit
// `configure.py` claims, which is why the symbol is `U` here and the 92 bytes stay retail's.
// Metroid Prime 1's `src/MetroidPrime/main.cpp` defines exactly this function in exactly this
// place (immediately above `CMain::AddWorldPaks`, its only caller in the object), and its body
// is the four calls retail makes: copy-construct the first operand onto the frame, append the
// second, copy-construct the result into the return slot, then destroy the temporary.
//
// The body is **identical** to the one `src/MetroidPrime/PortGlobals.cpp:883` already carries
// for the PC link, so this is not a second implementation of anything: `PortGlobals.cpp` is
// deliberately not a `configure.py` unit (its header explains why - a definition in a claimed
// unit would collide with the retail object), and `src/MetroidPrime/main.cpp` is not in the
// port's `files.cmake` either (the port links `mainHead`/`mainMid`/`mainTail`), so the two never
// meet in a link.
namespace rstl {
string operator+(const string& a, const string& b) {
  string result(a);
  result.append(b);
  return result;
}
} // namespace rstl

// Retail 0x800057A8, 0x180 = 384 bytes, 96.00% here. Prime 1's `CMain::AddWorldPaks` is this
// function with the loop count changed (9 there, 16 here - retail's own `cmpwi r29,16` at
// 0x800056C0) and `GetWorldPrefix` renamed to `GetPakFile`.
//
// **Three measured differences are left, and two of the three obvious fixes make it worse.**
// Recorded here so the next attempt does not repeat them (all three were tried, 2026-09-30):
//
//  1. **`rstl::rmemory_allocator allocator;` and naming the pool literals are both right and both
//     cost 13.5 points** (96.00% -> 82.44%). Retail does pass `r1+8` as `rmemory_allocator const&`
//     at 0x8000563C and does reach `.pak` and `%d` through `lbl_803A56C0`, but mwcceppc allocates
//     the frame from the *tallest* local it sees, so naming them changes every spill offset at
//     once: all 16 `r1+N` displacements move together and none of them lands where retail has it.
//     The two changes are individually correct and jointly wrong.
//  2. **`GetPakFile` returning by value instead of by const reference** is likewise required by
//     the measured bytes (retail 0x80216D5C is a bare copy-constructor into the caller's sret
//     slot, and `CMain::AddWorldPaks` sets `addi r3,r1,92` before the call and calls
//     `internal_dereference` on `r1+92` after). Same frame-size consequence. Changing the return
//     type of `CTweakGame::GetPakFile` is a header edit affecting `CGameState.cpp`,
//     `CPlayerState.cpp`, `CScriptPickup.cpp` and `Tweaks.cpp` as well, so it wants its own
//     item, not a rider on this one.
//  3. What is left after (1) and (2) is the frame size itself: 0xA0 against our 0x90, i.e. one
//     16-byte `rstl::string` temporary that retail has and we do not.
void CMain::AddWorldPaks() {
  rstl::string basePath = gpTweakGame->GetPakFile();
  for (int i = 0; i < 16; ++i) {
    rstl::string pak =
        basePath + (i == 0 ? rstl::string_l("") : rstl::string(CBasics::Stringize("%d", i)));
    if (CDvdFile::FileExists((pak + rstl::string_l(".pak")).data())) {
      gpResourceFactory->GetResLoader().AddPakFileAsync(pak, false, true);
    }
  }
}

void CMain::EnsureWorldPaksReady() {
  CResLoader& resLoader = gpResourceFactory->GetResLoader();
  for (int i = 0; i < resLoader.GetPakCount(); ++i) {
    CPakFile& file = *resLoader.GetPakFile(i);
    if (file.IsWorldPak()) {
      file.EnsureWorldPakReady();
    }
  }
}

// Retail 0x80005698 (`main.o` +0x2E0), 0xD0 = 208 bytes. Every loaded **world** pak gets its name
// list **copied** (`CPakFile`+0x58, the `rstl::vector` copy constructor at +0x4C) and scanned for
// `id`. A pak that does **not** have it is told to fetch what it is missing -
// `CPakFile::sub_80323554`; one that does is told to finish loading -
// `CPakFile::EnsureWorldPakReady`. Both tails are retail's own relocations against defined
// `CPakFile` members; neither is a stand-in. The copy is destroyed with `li r4,-1` at +0xA0, and
// writing the function at all is what first pairs the vector's copy constructor and destructor in
// this tree - they were unpaired COMDATs at 0.00% because nothing instantiated them.
//
// Three spellings of the same algorithm were measured; all three differ from retail only in
// register allocation and scheduling, which mwcceppc does not normalise, so each one is load-bearing.
//  1. The scan must be written with **iterators** (100.00% against **94.13%** for the index
//     spelling). mwcceppc strength-reduces `for (int j = 0; j < names.size(); ++j)` to a **counted**
//     loop - it emits `mtctr r0 / cmpwi r0,0 / ble / ... / bdnz` - where retail's bytes are the
//     pointer form `mulli r0,r0,24 / add r3,r4,r0 / cmplw r4,r3 / bne` (stride 24 =
//     `sizeof(rstl::pair<rstl::string, SObjectTag>)`).
//  2. The flag's **initialisation has to precede the `GetPakFile` call**, so it is declared on the
//     line *above* `CPakFile& file` (100.00% against **95.58%** declared below it). Retail's
//     `li r29,1` sits at +0x2C, between the argument setup and the `bl` at +0x30. Declared below
//     it, mwcceppc sinks the `li` past the call and past `lbz r0,40(r3)`, its live range then fits
//     entirely between two calls, and it allocates the flag to a scratch register (r28) and the pak
//     pointer to r29 - retail has those the other way round. The live range, not the declaration
//     order, is what picks the register.
//  3. The flag's **polarity** is the last single instruction: `bool found = false` with the arms in
//     their natural order is **99.96%**, differing in exactly `li r29,1` against `li r29,0` and back.
//     Retail stores "not yet seen" and clears it on a match, so the flag is named `notFound` and the
//     two arms are written in that sense. The control flow is identical either way.
void CMain::EnsureWorldPakReady(CAssetId id) {
  CResLoader& resLoader = gpResourceFactory->GetResLoader();
  for (int i = 0; i < resLoader.GetPakCount(); ++i) {
    bool notFound = true;
    CPakFile& file = *resLoader.GetPakFile(i);
    if (file.IsWorldPak()) {
      rstl::vector< rstl::pair< rstl::string, SObjectTag > > names = file.NameList();
      for (rstl::vector< rstl::pair< rstl::string, SObjectTag > >::iterator it = names.begin();
           it != names.end(); ++it) {
        if (it->second.id == id) {
          notFound = false;
        }
      }
      if (notFound) {
        file.sub_80323554();
      } else {
        file.EnsureWorldPakReady();
      }
    }
  }
}

// Retail 0x800090A8, 0x7C = 124 bytes, and the whole body is what the compiler generates for
// the class's four members plus the deleting-destructor tail: the two `rstl::vector`s and the two
// `rstl::bit_vector`s are destroyed in **reverse declaration order** (the vectors at +0x38 and
// +0x28, the bit_vectors at +0x14 and +0x00), each with `li r4,-1` so the member destructors run
// their bodies and skip their own `operator delete`, and the object itself is released by
// `CMemory::Free` when the incoming flag is positive. `rstl::vector<rstl::pair<TEditorId, bool>>`
// is 0x14 bytes and `rstl::bit_vector<>` is 0x14 bytes, so +0x38/+0x28/+0x14/+0x00 and the 0x4C
// object size the header's `CHECK_SIZEOF` pins both fall out of the header's member order.
//
// **The class had no destructor before this**, so every `rstl::rc_ptr<CMapWorldInfo>` holder inlined
// the four member teardowns. Retail keeps one out-of-line copy of the deleting destructor, and the
// member types' own destructors are declared rather than defined (`rstl::bit_vector`'s is implicit
// but still out of line), so each stays a `bl` here instead of expanding.
// Retail 0x80009058, 0x50 = 80 bytes: `rstl::rc_ptr<CMapWorldInfo>::ReleaseData()`, and the
// class's own deleting destructor is the next symbol, at 0x800090A8.
//
// **`template class` is what puts it in this object, and it is the only thing that does.**
// `include/rstl/rc_ptr.hpp` defines `ReleaseData` out of line and this unit's flags carry
// `-inline deferred,noauto`, so mwcceppc never inlines it - but it only *emits* it when something
// instantiates `rc_ptr<CMapWorldInfo>` **in this translation unit**, and nothing here does: the
// holders are `CWorldState`'s, in `Player/CWorldState.cpp`. Retail's object carried it because
// `dtk`'s `auto_03_80003BE8_text` - the range `CGameState`'s destructor at 0x800044C8 calls it
// from - is a *split of the same original object* as this unit, so retail's compiler emitted the
// definition and its caller together. An explicit class instantiation is the C++ spelling for
// "emit this template's definitions here"; it adds no call site, and the body is the one already
// in the header: 20 words, byte-identical to retail's object. `delete GetPtr()` is
// `lwz r3,0(r31) ; li r4,1 ; bl __dt__13CMapWorldInfoFv` because `~CMapWorldInfo` is MWCC's
// deleting destructor, and `delete mRefCount` is `CMemory::Free`.
//
// 0x80009224 is the same function for `CWorldLayerState` and reads 0.00% for a different reason:
// `config/G2ME01/symbols.txt` has no mangled name for that address, only dtk's `fn_80009224`
// placeholder, so objdiff cannot pair it with anything this unit emits however the body is
// spelled. 0x80009058 and 0x8000934C are the two `ReleaseData` in this range the map *does* name.
template class rstl::rc_ptr< CMapWorldInfo >;

CMapWorldInfo::~CMapWorldInfo() {}

// Retail 0x80009274, 0x84 = 132 bytes, and it is the same arrangement one class along: retail
// keeps ONE out-of-line deleting destructor for `CWorldLayerState` and every `rc_ptr` holder of it
// goes through `rstl::rc_ptr<CWorldLayerState>::ReleaseData` (0x80009224, whose only caller is
// this), so **the class had no destructor at all** and the four member teardowns were inlined at
// every use. The body is `{}` and the compiler generates all 132 bytes:
//   +0x2C `mLayerNameOffsets` (`rc_ptr<vector<int>>`) and +0x24 `mLayerNames`
//   (`rc_ptr<vector<string>>`), each a `addic. r0,off / beq` null guard then `bl ReleaseData`,
//   then +0x10 `mSaveLayers` (`rstl::bit_vector`, `li r4,-1`) and +0x00 `mAreaLayers`
//   (`rstl::vector<CWorldLayers::Area>`, `li r4,-1`), i.e. reverse declaration order read straight
//   back off the header, and the `extsh. r0,r31 / ble / mr r3,r30 / bl CMemory::Free` tail.
// 0x34 is what the header's `CHECK_SIZEOF` already asserted, so the member order is unchanged -
// only the linkage is. Nothing here is invented; `CWorldState::mLayerState` is the retail holder
// (`include/MetroidPrime/Player/CWorldState.hpp:46`) and it already calls retail's out-of-line
// `ReleaseData`, so taking the destructor out of line moves no other unit's bytes.
CWorldLayerState::~CWorldLayerState() {}

// ---------------------------------------------------------------------------
// Retail 0x80009008, 0x80009224 and 0x800095E4: three `rstl::rc_ptr<T>::ReleaseData`
// instantiations, 0x50 = 80 bytes each, and one body.
//
// `include/rstl/rc_ptr.hpp` already defines the template -
// `if (--*mRefCount <= 0) { delete GetPtr(); delete mRefCount; }` - and this unit already emits
// five copies of it as weak COMDATs (`ReleaseData__Q24rstl15rc_ptr<6CIOWin>Fv` and friends, all at
// 100%). **None of these three is one of those five**, for the reason the rest of this file's
// `extern "C"` blocks give: `config/G2ME01/symbols.txt:185` and `:191` are `fn_80009008` and
// `fn_80009224`, retail's own placeholders, and objdiff pairs functions **by name** - the weak
// `ReleaseData__Q24rstl23rc_ptr<16CWorldLayerState>Fv` the template would emit for
// `fn_80009224` would be scored against nothing. Retail's own names are the only ones that pair.
//
// The body is the template's and the three differ **only** in which destructor the `delete`
// reaches: `CWorldLayerState`'s (0x80009274, defined above), an unnamed one at 0x800B8CA0, and
// `CWorldTransManager`'s (0x8015C17C). 0x50 rather than the 0x64 of
// `ReleaseData__Q24rstl15rc_ptr<6CIOWin>Fv` is the whole of the difference between the two shapes,
// and it is not the class: `CIOWin`'s destructor is **virtual**, so MWCC's `delete` goes through
// the vtable (`lwz r12,0(r3) / lwz r12,8(r12) / mtctr r12 / bctrl`, four extra instructions and a
// null guard), while all three of these classes declare a plain non-virtual destructor, for which
// `delete p` is the direct `li r4,1 / bl ~D0` that the class's own D1 form ends in. Nothing else
// in the 20 instructions differs between the three.
//
// `SPairRcPtr` is `rstl::rc_ptr<T>`'s two words - `{ const T* mPtr; int* mRefCount; }` at +0 and
// +4 - read through a same-layout view rather than through the class, for the reason
// `rstl::CRcPtrData` exists (`include/rstl/rc_ptr.hpp`): retail's refcount is a separate
// four-byte `CMemory` allocation, and both members are private. The view is eight bytes and
// changes no layout.
// ---------------------------------------------------------------------------

// The two destructors outside this unit, declared the way the symbol table names them.
// `CWorldTransManager`'s own header is not included here and the class is not otherwise used in
// this translation unit, so the call is spelled against the symbol; 0x800B8CA0 has no name in the
// map at all. Both bodies are retail's, in other units' ranges, and both stay undefined here -
// this unit is `NonMatching`, so `dtk dol split` supplies retail's bytes for the whole claim.
extern "C" void* __dt__18CWorldTransManagerFv(void* self, short flag);
extern "C" void* fn_800B8CA0(void* self, short flag);

struct SPairRcPtr {
  void* x0_ptr;
  int* x4_refCount;
};

extern "C" void fn_80009224(SPairRcPtr* self) {
  if (--*self->x4_refCount <= 0) {
    delete static_cast< CWorldLayerState* >(self->x0_ptr);
    delete self->x4_refCount;
  }
}

extern "C" void fn_80009008(SPairRcPtr* self) {
  if (--*self->x4_refCount <= 0) {
    fn_800B8CA0(self->x0_ptr, 1);
    delete self->x4_refCount;
  }
}

extern "C" void fn_800095E4(SPairRcPtr* self) {
  if (--*self->x4_refCount <= 0) {
    __dt__18CWorldTransManagerFv(self->x0_ptr, 1);
    delete self->x4_refCount;
  }
}

// Retail 0x800053B8, 0x214 = 532 bytes, the first function of this unit. It is the only
// "load a saved game" path: it copies five blocks out of the **old** `gpGameState` onto the
// stack, reads one of them back as a `CMemoryInStream`, builds a `CGameState` from it, and
// copies the four temporaries into the new one. Retail's own object names every callee, so
// each is declared and called - a callee's body is not a precondition for reproducing a
// function, and `dtk dol split` supplies retail's bytes for the whole claimed range.
//
// **The five temporaries are constructed by retail's own copy constructors, and the order
// matters**: the second one (`x110`) is read back at 0x80005488 into the index that selects
// which of its three blocks becomes the stream, so it has to outlive the first. The block
// chosen is `x110.x04_blk[oldSystemOptions.mSaveIdx]` unless the `x188` block is non-empty
// **and** the argument is false, in which case it is `x188` - that is what the `r31` flag is,
// and it is also the flag the `RecordCheckpoint` call at the end is guarded by.
//
// `SGameStateStreamSource` is a same-layout view of the five `CGameState` members this reads,
// declared here rather than freighting `CMain::StreamNewGameState` into `CGameState.hpp`: the
// members are `private`, a friend declaration is a shared-header change this item may not
// make, and the layout is already written down in that header's comment block (the five
// offsets, +0x54/+0x80/+0x110/+0x144/+0x178/+0x188). No offset is spelled as a raw number
// at the use sites - each member is named - which is the same arrangement as
// `SGameGlobalObjectsPtr` above.
extern "C" void fn_80005108(void* self, const void* src);
extern "C" void fn_80004C90(void* self, const void* src);
extern "C" void fn_80004AA0(void* self, const void* src);
extern "C" void fn_80004E84(void* self, const void* src);
extern "C" void fn_80004154(void* self, void* value);
extern "C" void fn_80003F08(void* self, const void* src);
extern "C" void fn_80003D00(void* self, const void* src);
extern "C" void fn_80004D84(void* self, short flag);
extern "C" void fn_80004A4C(void* self, short flag);
extern "C" void __dt__80004B9C(void* self, short flag);
extern "C" void __dt__PersistentOptions_800050A4(void* self, short flag);
extern "C" void RecordCheckpoint__10CGameStateFv(CGameState* self);
extern "C" void SetCompressedGameStates__10CGameStateFRCQ24rstl65reserved_vectorQ24rstl37vectorUcQ24rstl17rmemory_allocatorE3(
    CGameState* self, const void* states);
extern "C" void SetCompressedGameOptions__10CGameStateFRCQ24rstl65reserved_vectorQ24rstl37vectorUcQ24rstl17rmemory_allocatorE3(
    CGameState* self, const void* options);
extern "C" void SetCompressedMultiplayerOptions__10CGameStateFRCQ24rstl37vectorUcQ24rstl17rmemory_allocatorE(
    CGameState* self, const void* options);

// **The five temporaries are copied by retail's own out-of-line copy constructors, and
// `= old->member` does not produce them**: mwcceppc inlines these four classes' implicit
// copies into a word-by-word store run, and the object came out 772 bytes against retail's
// 532. The wrapper below has the same size and member offsets, and its copy constructor is a
// **user-provided** one - which is what stops the inlining - so the call reaches the
// retail-named function with `this` in r3 and the source in r4, exactly as retail's
// relocations show.
//
// `SStreamBlock` is deliberately **trivially default-constructible**. A user-provided
// default constructor on it made mwcceppc emit a `__construct_array` loop for `x04_blk[3]`
// (measured, 5 instructions and a reloc retail does not have); the block array is raw bytes
// here and `BlockAt()` returns a `const SStreamBlock&` view over one, which is what retail's
// `lwz r4,12(r5) ; lwz r5,4(r5)` pair reads - `x0c_data` and `x04_count`.
//
// `CHECK_SIZEOF` on each is the guard that a layout change in `CGameStateBlocks.hpp` or the
// two option headers cannot silently break the offsets.
struct SStreamBlock {
  u32 x00_unk;
  // **`x04_count` is compared with `cmpwi r0,0`, not `cmplwi`** (measured: declaring it `u32`
  // gives `cmplwi` and one differing instruction). The element count of a saved-game block is
  // never negative, so the sign is retail's own choice of the declared type, and `SGameStateBlock`
  // in `CGameStateBlocks.hpp` spells it `u32` - which is why this wrapper is a separate type
  // rather than a reuse of that struct.
  int x04_count;
  u32 x08_cap;
  void* x0c_data;
};
CHECK_SIZEOF(SStreamBlock, 0x10)

struct SStreamSlots {
  int x00_count;
  // **The block array is raw bytes, reached through `Blocks()`.** With a real
  // `SStreamBlock x04_blk[3]` member, mwcceppc builds the element address as
  // `&slotsStates` then `+4` then `+idx*0x10` - three instructions where retail has two
  // (`addi r5,r1,128 ; add r5,r5,r0`, the `+4` folded into the `addi`). Going through a
  // `uchar[0x30]` and a `reinterpret_cast` is what lets it fold: 66 differing instructions
  // against 101 for the array member, same source otherwise (both measured).
  uchar x04_raw[0x30];
  SStreamBlock* Blocks() { return reinterpret_cast< SStreamBlock* >(x04_raw); }
  // The count word retail tests at 0x80005434, `lwz r0,40(r1)` = `x00_count` + the block's
  // own `x04_count` is at +4 of the block itself.
  int Count() const { return x00_count; }
  SStreamSlots() {}
  SStreamSlots(const void* src) { fn_80004C90(this, src); }
  void Destroy() { __dt__80004B9C(this, -1); }
};
CHECK_SIZEOF(SStreamSlots, 0x34)

struct SStreamBlockOwner {
  char x00_[0x10];
  SStreamBlockOwner() {}
  SStreamBlockOwner(const void* src) { fn_80004AA0(this, src); }
  SStreamBlock& Block() { return *reinterpret_cast< SStreamBlock* >(x00_); }
  void Destroy() { fn_80004A4C(this, -1); }
};
CHECK_SIZEOF(SStreamBlockOwner, 0x10)

struct SStreamSysOpts {
  char x00_[0x28];
  int mSaveIdx; //!< CGameState+0x54+0x28
  SStreamSysOpts() {}
  SStreamSysOpts(const void* src) { fn_80005108(this, src); }
  int GetSaveIdx() const { return mSaveIdx; }
  void Destroy() { __dt__PersistentOptions_800050A4(this, -1); }
};
CHECK_SIZEOF(SStreamSysOpts, 0x2C)

struct SStreamGameOpts {
  char x00_[0x44];
  SStreamGameOpts() {}
  SStreamGameOpts(const void* src) { fn_80004E84(this, src); }
  void EnsureOptions();
  void Destroy() { fn_80004D84(this, -1); }
};
CHECK_SIZEOF(SStreamGameOpts, 0x44)
extern "C" void EnsureOptions__12CGameOptionsFv(void* self);
void SStreamGameOpts::EnsureOptions() { EnsureOptions__12CGameOptionsFv(this); }

// The view of the six `CGameState` members, at `CGameState+0x54`. It is a view and not a
// `friend`, because the members are `private` and a friend declaration is a shared-header
// change this item may not make; the six offsets are already written down in
// `CGameState.hpp`'s own comment block, and no offset is spelled as a raw number at a use
// site - each member is named. Same arrangement as `SGameGlobalObjectsPtr` above.
struct SGameStateStreamSource {
  SStreamSysOpts mSystemOptions; //!< CGameState+0x54, 0x2C
  SStreamGameOpts gameOptions;  //!< CGameState+0x80, 0x44
  char xc4_[0x44];              //!< CGameState+0xC4 .. +0x107
  u32 cardSerialA;              //!< CGameState+0x108
  u32 cardSerialB;              //!< CGameState+0x10C
  SStreamSlots x110;            //!< CGameState+0x110, 0x34
  SStreamSlots x144;            //!< CGameState+0x144, 0x34
  SStreamBlockOwner x178;       //!< CGameState+0x178, 0x10
  SStreamBlockOwner x188;       //!< CGameState+0x188, 0x10
};
static inline SGameStateStreamSource* StreamSource(CGameState* self) {
  return reinterpret_cast< SGameStateStreamSource* >(reinterpret_cast< char* >(self) + 0x54);
}

void CMain::StreamNewGameState(bool fromSave) {
  // **`gpGameState` is re-read from SDA for every copy, not cached in a local.** Retail's
  // `R_PPC_EMB_SDA21 gpGameState` appears six times in this function's relocations, once per
  // source; naming the pointer once in a local and reusing it replaces those with one `addi`
  // and costs four instructions and a register. The `StreamSource(gpGameState)` call at each
  // use site is what reproduces them, and it is also correct: `gpGameState` is reassigned in
  // the middle of the function, so a cached pointer would be the wrong object after that.
  SStreamSysOpts sysOpts(&StreamSource(gpGameState)->mSystemOptions);
  // `lwz r27,216(r1)` is `sysOpts.mSaveIdx` and the two `lwz` at +0x108/+0x10C are the card
  // serials; retail reads all three **here**, between the first and second copy, and not at
  // the point of use. Naming them later moves four instructions.
  const int saveIdx = sysOpts.GetSaveIdx();
  const uint cardA = StreamSource(gpGameState)->cardSerialA;
  const uint cardB = StreamSource(gpGameState)->cardSerialB;
  SStreamSlots slotsStates(&StreamSource(gpGameState)->x110);
  SStreamBlockOwner blockMultiplayer(&StreamSource(gpGameState)->x188);
  // `cmpwi r0,0` on `x188`'s count word, then `clrlwi. r0,r26,24` on the argument: the flag is
  // set when the block has data **and** the argument is false, and it is what selects the
  // source block and guards the checkpoint below.
  const bool checkpoint = (blockMultiplayer.Block().x04_count != 0) && !fromSave;
  SStreamSlots slotsOptions(&StreamSource(gpGameState)->x144);
  SStreamBlockOwner blockCard(&StreamSource(gpGameState)->x178);
  SStreamGameOpts gameOpts(&StreamSource(gpGameState)->gameOptions);

  // **The slot address is recomputed, not cached**: retail's `lwz r3,84(r28) ; addi r3,r3,304`
  // pair appears three times in this function's relocations, once per call. A named reference
  // hoists it into a callee-saved register and costs four instructions.
  fn_80004154(&mGameGlobalObjects->GameState(), nullptr);

  gpGameState = nullptr;
  // **The address is formed before the branch, and the branch only picks between two
  // finished pointers.** Retail is `clrlwi. r0,r31,24 / li r3,0 / stw r3,gpGameState /
  // slwi r0,r27,4 / addi r5,r1,128 / add r5,r5,r0 / beq / addi r5,r1,36` - the scaled index
  // and the base are computed unconditionally, the global is cleared in the middle of it, and
  // the `beq` selects. A `?:` that forms the address inside each arm cannot produce that, and
  // five spellings of the select (reference, pointer, `&x04_blk[i]`, `x04_blk + i`,
  // `x04_blk[0] + i`, a `char*` base with an explicit `* 0x10`) all compile to the same three
  // instructions, one of them too many.
  SStreamBlock* source = slotsStates.Blocks() + saveIdx;
  if (checkpoint) {
    source = &blockMultiplayer.Block();
  }
  CGameState* made = nullptr;
  {
    // **The stream and the reader are in a nested scope, and that is load-bearing.** Retail
    // destroys them at 0x800054C0 and 0x800054CC - immediately after the `single_ptr` assign
    // that consumes the new `CGameState`, and *before* `gpGameState` is re-read - not at the
    // end of the function. Written at function scope, mwcceppc sinks both destructions into
    // the epilogue and the object grows by two calls (measured: 8 destroys against retail's 6).
    CMemoryInStream stream(source->x0c_data, source->x04_count);
    CBitStreamReader reader(stream);
    made = new CGameState(reader);
    fn_80004154(&mGameGlobalObjects->GameState(), made);
  }

  gpGameState = mGameGlobalObjects->GameState().get();
  fn_80003F08(&StreamSource(gpGameState)->mSystemOptions, &sysOpts);
  SetCompressedGameStates__10CGameStateFRCQ24rstl65reserved_vectorQ24rstl37vectorUcQ24rstl17rmemory_allocatorE3(
      gpGameState, &slotsStates);
  SetCompressedGameOptions__10CGameStateFRCQ24rstl65reserved_vectorQ24rstl37vectorUcQ24rstl17rmemory_allocatorE3(
      gpGameState, &slotsOptions);
  SetCompressedMultiplayerOptions__10CGameStateFRCQ24rstl37vectorUcQ24rstl17rmemory_allocatorE(gpGameState,
                                                                                             &blockCard);
  fn_80003D00(&StreamSource(gpGameState)->gameOptions, &gameOpts);
  StreamSource(gpGameState)->gameOptions.EnsureOptions();
  // **The two card-serial stores share one `gpGameState` read.** Retail is
  // `lwz r3,0(0) / clrlwi. r0,r31,24 / stw r29,268(r3) / stw r30,264(r3)` - one load for both
  // stores, with `r31` (the checkpoint flag) tested in the middle of them. Written as two
  // separate `StreamSource(gpGameState)->...` assignments mwcceppc emits a second `lwz`
  // between the stores (measured, one instruction over).
  SGameStateStreamSource* const fresh = StreamSource(gpGameState);
  // Retail stores **+0x10C before +0x108** (`stw r29,268(r3) ; stw r30,264(r3)`), i.e. the
  // declaration order of the two members is the reverse of the store order. Writing them the
  // other way round is two differing instructions (measured) - and no other property changes.
  fresh->cardSerialB = cardB;
  fresh->cardSerialA = cardA;
  if (checkpoint) {
    RecordCheckpoint__10CGameStateFv(gpGameState);
  }

  gameOpts.Destroy();
  blockCard.Destroy();
  slotsOptions.Destroy();
  blockMultiplayer.Destroy();
  slotsStates.Destroy();
  sysOpts.Destroy();
}

// Retail 0x8000934C, 0x50 = 80 bytes: `rstl::rc_ptr<CPlayerState>::ReleaseData()`, emitted for the
// reason the `rc_ptr<CMapWorldInfo>` instantiation above gives, and byte-identical to retail's
// 20 words (`delete GetPtr()` -> `bl __dt__12CPlayerStateFv`, which the next symbol at 0x8000939C
// is).
//
// **`src/MetroidPrime/Player/CPlayerStateRefRelease.cpp` is now stale and this supersedes it.**
// That file writes the same 80 bytes as an `extern "C"` `fn_8000934C` on the premise that "retail
// instantiates `ReleaseData` per type and leaves this one unnamed in the symbol table". The map
// now says otherwise: `config/G2ME01/symbols.txt` carries
// `ReleaseData__Q24rstl22rc_ptr<12CPlayerState>Fv = .text:0x8000934C`, so the name objdiff pairs
// on is the mangled one and only this instantiation reaches it. The file stays unregistered, and
// it is not touched here: it is not in `files.cmake`, so nothing compiles it and nothing collides.
template class rstl::rc_ptr< CPlayerState >;

CPlayerState::~CPlayerState() {}

CPlayerState::SPersistentState::~SPersistentState() {}

CStaticInterference::~CStaticInterference() {}

// Retail `.sdata2` 0x8041EE00, `data:4byte`, **0x00008F00** (`config/G2ME01/symbols.txt:25502`,
// `size:0x8` - retail names the pair). Declared, never defined; the DOL's `auto_*_sdata2.o` that
// holds `.sdata2` supplies it.
extern const uint lbl_8041EE00;

// Retail 0x80009864, 0x1C = 28 bytes, seven instructions, and the only writer of
// `lbl_80418EA0` (`.sbss` 0x80418EA0) - the ARAM size `CGameArchitectureSupport`'s constructor
// hands to `CAudioSys`:
//
//   lwz   r0,-13760(r2) ; mulli r0,r0,28 ; srawi r0,r0,3 ; addze r0,r0 ; slwi r0,r0,2 ;
//   stw   r0,-28384(r13) ; blr
//
// `(v * 28) / 8 * 4` is `v * 14` arithmetically, and **that is the only spelling of it that emits
// all five of those instructions**: mwcceppc folds a bare `* 14` to one `mulli` and keeps the
// `/ 8 * 4` as a `srawi`/`addze`/`slwi` run. Five tried, diff count against the DOL's 28 bytes
// with the two relocation fields masked:
//
//   `(*(const int*)&lbl_8041EE00 * 28) / 8 * 4`               0   <- this
//   the same with the value in a `const int` local first        0   (same code)
//   `*(const int*)&lbl_8041EE00 * 14`                        16 B, wrong shape
//   `((*v * 28) / 8) << 1`                                     1 word out (slwi 1 not 2)
//   `((*(const uint*)&lbl_8041EE00 * 28) / 8) * 4`            20 B, wrong shape
//   `(v * 28) / 2 / 2 / 2 * 4`                               56 B, wrong shape
//
// **`-13760` is an r2 displacement, and r2 is not `_SDA_BASE_`.** Retail's `__init_registers`
// (0x80003464-0x80003470) loads **two** small-data bases, and they are 0x2640 apart:
//
//   3c 40 80 42  lis r2,0x8042  /  60 42 23 c0  ori r2,r2,0x23C0   ->  r2  = 0x804223C0
//   3d a0 80 41  lis r13,0x8041 /  61 ad fd 80  ori r13,r13,0xFD80 ->  r13 = 0x8041FD80
//
// 0x8041FD80 is the value `powerpc-eabi-nm` reports for `_SDA_BASE_` **and the one `tools/sda.py`
// uses**, so **`tools/sda.py` answers an r2-relative displacement with the wrong address**: it
// says `lbl_8041C7C0` for `-13760`, and `.sdata2` 0x8041C7C0 is 0x3F7D70A4 = 0.99f. The right
// answer is `0x804223C0 - 0x35C0 = 0x8041EE00`, which is also what dtk names in retail's own
// object (`build/G2ME01/obj/MetroidPrime/main.o`: `R_PPC_EMB_SDA21 lbl_8041EE00` at 0x44AC). The
// `stw` below is an **r13** displacement, where 0x8041FD80 *is* the right base, so this one
// function needs both: r2-relative addresses `.sdata2`, r13-relative addresses `.sdata`/`.sbss`.
extern "C" void fn_80009864() {
  lbl_80418EA0 = (*(const int*)&lbl_8041EE00 * 28) / 8 * 4;
}
