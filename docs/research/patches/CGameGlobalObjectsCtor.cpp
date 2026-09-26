/**
 * `CGameGlobalObjects::CGameGlobalObjects(COsContext&, CMemorySys&)` - retail
 * `__ct__18CGameGlobalObjectsFR10COsContextR10CMemorySys`, `.text:0x8000848C`, `size:0xE4` =
 * 228 bytes, ending at 0x80008570 where `ShutdownSubsystems__5CMainFv` begins.
 *
 * **This is the only writer of `gpGameState` in the DOL.** `CGameArchitectureSupport`'s
 * constructor loads it at 0x800081A4 with no null test, and the store is 0x8000854C
 * (`stw r4,-28360(r13)`, `-28360` being the full signed displacement from `_SDA_BASE_` =
 * 0x8041FD80, not half of it). `CMain::RsMain` calls this constructor at 0x80005CE4, which is
 * boot-path step 7 - so `gpGameState` needs step 7 and not the paks of step 13, and the port
 * stops here for want of this function rather than for want of a pak. `docs/research/
 * boot_globals.md` has the store-by-store map; `src/MetroidPrime/PortBoot.cpp` has the boot ladder.
 *
 * The body is four member constructors, two allocations and six global stores, and it never
 * reads its two parameters - `fn_800084A0`'s prologue is `stwu r1,-16(r1); mflr r0; stw r0,20(r1);
 * stw r31,12(r1); mr r31,r3` and `r4`/`r5` are untouched for the whole 0xE4.
 *
 * ## What it takes to get the offsets right
 *
 * `include/MetroidPrime/CGameGlobalObjects.hpp` had **no member** at +0x108, so every member
 * below `CSimplePool` read 0x28 lower than retail and the five stores at 0x80008534-0x80008558
 * had nowhere to land. The header now has the member (`CCharacterFactoryBuilder`, 0x28) and the
 * two stand-ins for the unnamed +0x00 and +0x150 members; the reasoning is in that header.
 *
 * ## `simplePool` is not initialised from an uninitialised `resFactory`
 *
 * `src/MetroidPrime/PortBoot.cpp` says the constructor "initialises `simplePool` from an
 * uninitialised `resFactory`", and that was true of the *stub* only. Retail calls
 * `fn_802FB154(this+0x04)` - which is the real `CResFactory` constructor, and the header's
 * `CResFactory()` - at 0x800084A8, *before* `fn_80301008(this+0xE4, this+0x04)` at 0x800084B4.
 * The second argument is `&resFactory`, the member's own address, and all `CSimplePool` keeps
 * of it is the reference at +0x18. So `resFactory` is a fully constructed `CResFactory` by then
 * and the initialisation is valid. On the port it needs `fn_802FB154` to have a body; until
 * 2026-09-26 `src/Kyoto/CResFactoryCtor.cpp` gave `CResFactory::CResFactory()` the body of
 * `fn_803096C4` (the +0x00 member's constructor) instead.
 *
 * ## The port side, and what listing this file does to the link
 *
 * **Not in `files.cmake`, measured (lane `cgo`, 2026-09-26).** Listing it takes
 * `tools/link_check.sh` from 325 to 333 undefined and closes none, because nothing in the port
 * calls it: `CMain::RsMain` (`src/MetroidPrime/PortBoot.cpp`) checks `gpGameState` instead. The
 * eight are exactly `CSimplePool::CSimplePool(IFactory&)`, `CSimplePool::~CSimplePool()` (asked
 * for only by g++'s exception cleanup for `simplePool`), `fn_803096C4`, `fn_80032008`,
 * `fn_801449C8`, `fn_8016C230`, `fn_801F0A44` and `lbl_80418EC8`. Three are cheap
 * (`fn_8016C230` is four instructions, `fn_801F0A44` twelve, `lbl_80418EC8` a port-side
 * definition), two are another lane's (`CSimplePool`'s constructor, `fn_803096C4`), and three
 * are not cheap: `fn_801449C8` is `CGameState`'s default constructor, whose own listing is +10;
 * `fn_80032008` is `CCharacterFactoryBuilder`'s constructor, which on the host asks for its
 * `CDummyFactory` vtable and so for a 0x144-byte `Build` and a 0x8C-byte `BuildAsync`; and
 * `~CSimplePool` is 0xA0 bytes with its own retail callees. So the floor is +3 even with the
 * other lane's two - see `docs/research/cgameglobalobjects_ctor.md`.
 */

// `#define _CMEMORY` keeps `Kyoto/Alloc/CMemory.hpp`'s own throwing `operator new` out, so the
// two below are the only `new`s in this translation unit - the same arrangement as
// `MetroidPrime/Player/CGameStateCtor.cpp`, which is where the reasoning is.
#define _CMEMORY

#include "types.h"

// The merged `.rodata` pool object both `operator new` calls pass as their file operand. **This
// constructor's is `lbl_803A56C0` and not the `lbl_803A9208` that `CGameStateCtor.cpp` uses** -
// there is one such pool per DOL `.rodata` blob, retail's `lis r4,-32710; addi r4,r4,22208` at
// 0x800084C0 is 0x803A56C0, and 0x803A56C0 is where the pak names and `"%d"` live.
//
// **These two declarations have to come before every other include, which is not what
// `CGameStateCtor.cpp` does and is the opposite of what it looks like it does.** Declaring
// *any* namespace-scope `operator new` makes mwcceppc drop the implicit overload set, so
// `rstl/construct.hpp`'s `new (dest) T(src)` then has no `operator new(size_t, void*)` to
// find. `CGameStateCtor.cpp` gets away with putting them late because nothing in its include
// graph instantiates `construct<>`; this unit does - `rstl::optional_object<TLockedToken<
// CStringTable>>`'s teardown reaches `rstl::construct<TToken<CTexture>>` - and the error is
// "function call 'operator new(unsigned long, void *)' does not match 'operator new(unsigned
// long)'", from inside a header, hundreds of lines before the calls.
extern "C" const char lbl_803A56C0[];

#if defined(__MWERKS__) || defined(CLANGD)
void* operator new(size_t sz, const char*, const char*);
inline void* operator new(size_t n, void* ptr) { return ptr; }
inline void* operator new(size_t sz) { return operator new(sz, lbl_803A56C0, 0); }

// **`_CMEMORY` is `Kyoto/Alloc/CMemory.hpp`'s own include guard, so defining it takes the
// whole class with it - and `rstl/rc_ptr.hpp` still names `CMemory::Free`.** `CSimplePool`
// carries an `rstl::rc_ptr<CVParamTransfer>` member and mwcceppc instantiates a class
// template's members eagerly rather than on use, so the reference is a hard error in a unit
// that never frees a refcount: "undefined identifier 'CMemory' (instantiating:
// 'rstl::rc_ptr<CVParamTransfer>::FreeRefCount(int *)')". Two declarations are all that is
// missing, and on the host `rc_ptr.hpp` takes the `delete ptr` branch and never mentions
// `CMemory` at all. The real header is skipped, not redeclared, so there is no conflict.
class CMemory {
public:
  static void* Alloc(size_t len);
  static void Free(const void* ptr);
};
#endif

#include "MetroidPrime/CGameGlobalObjects.hpp"

#include "MetroidPrime/CInGameTweakManager.hpp"
#include "MetroidPrime/Player/CGameState.hpp"

#if !defined(__MWERKS__)
#include <new>
#endif

extern "C" {
// CGameState's default constructor, `fn_801449C8` - `MetroidPrime/Player/CGameStateCtor.cpp`,
// `Matching`, 740 bytes and eight nested constructors. Named by retail's address because
// `symbols.txt` has no name for it; it returns `this`, which is what retail stores.
CGameState* fn_801449C8(CGameState* self);
} // extern "C"

// The two allocations. Both are `static inline` because a mem-init list needs one expression and
// an out-of-line helper would be a second function the retail object does not define -
// `tools/unit_fit.sh` lists it as an extra function if mwcceppc declines to inline. The shapes
// are retail's: `li r3,N`, the two-operand file string, `li r5,0`, the call, then `mr. r0,r3;
// beq` and the constructor, which is why both spell the null test as a conditional and not as an
// `if` (`MetroidPrime/Player/CGameStateCtor.cpp` records the same thing for `new CWorldState`).
// The two `CGameState* made = self; if (made != 0) made = ...; return made;` are not a
// spelling preference: they are what puts the call's result in **r0** instead of leaving it in
// r3, and retail does the same. `MetroidPrime/Player/CGameStateCtor.cpp` gets the same two
// instructions from a named local for the same reason. Measured, four variants: the ternary
// `return self != 0 ? f(self) : self;` gives `stw r3,304(r31)` against retail's
// `mr r0,r3 ; stw r0,304(r31)`, and so do an extra `static_cast<CGameState*>` and a
// `reinterpret_cast<unsigned int>` round trip; only the named temporary fixes it, and it fixes
// both allocations at once.
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

// `gpCharacterFactoryBuilder` (.sbss 0x80418EAC) is **not defined here**: it is inside
// `main.cpp`'s claimed `.sbss` range 0x80418EA0-0x80418EC4, and `src/MetroidPrime/main.cpp`
// already defines it. Defining it a second time makes `dtk dol split` see the two units
// reference each other and fail with "Cyclic dependency encountered while resolving link
// order: MetroidPrime/main.cpp -> MetroidPrime/CGameGlobalObjectsCtor.cpp" - measured, and it
// is the same shape as the CAi "cycle" that `RUNNING_THE_DECOMP.md` records as never real,
// except that this one is about *definition* order and not COMDAT weakness.
extern CCharacterFactoryBuilder* gpCharacterFactoryBuilder;
// .sbss 0x80418EC8, the address of the +0x150 member. `dtk dol split` leaves it in
// `auto_10_80418EC4_sbss` because this unit's claim is `.text` only, and three functions in the
// DOL read it (`lwz r3,lbl_80418EC8@sda21` at 0x80006078, 0x80006240 and 0x8000743C), so the
// symbol must not be defined here.
extern "C" void* lbl_80418EC8;

CGameGlobalObjects::CGameGlobalObjects(COsContext& osContext, CMemorySys& memorySys)
    : pad0()
    , resFactory()
    , simplePool(resFactory)
    , characterFactoryBuilder()
    , gameState(MakeCGameState())
    , inGameTweakManager(MakeInGameTweakManager()) {
  // The six stores at 0x80008534-0x80008558. `_SDA_BASE_` is 0x8041FD80 and the displacements
  // are the full signed ones, so -28380 is 0x80418EA4 (`gpResourceFactory`), -28376 is
  // 0x80418EA8 (`gpSimplePool`), -28372 is 0x80418EAC (`gpCharacterFactoryBuilder`), -28360 is
  // 0x80418EB8 (`gpGameState`), -28352 is 0x80418EC0 (`gpTweakManager`) and -28344 is
  // 0x80418EC8. `resFactory`, `simplePool` and `characterFactoryBuilder` are the *members'*
  // addresses; `gameState` and `inGameTweakManager` are read back out of their `single_ptr`s
  // with `lwz`, because the constructor above stored the result there.
  //
  // The two parameters are named because the signature is retail's, and are unused because
  // retail never reads them.
  (void)osContext;
  (void)memorySys;
  gpResourceFactory = &resFactory;
  gpSimplePool = &simplePool;
  gpCharacterFactoryBuilder = &characterFactoryBuilder;
  gpGameState = gameState.get();
  gpTweakManager = inGameTweakManager.get();
  lbl_80418EC8 = &x150_tail;
}
