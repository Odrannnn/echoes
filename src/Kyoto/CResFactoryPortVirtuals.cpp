/**
 * The port's definitions of the five `CResFactory` members the port's link still has no body for,
 * and of its destructor. **This file is port-only**: `configure.py` does not declare it, so
 * mwcceppc never sees it and it is not a decompilation unit - the same arrangement as
 * `src/Kyoto/CResFactoryCtor.cpp` and `src/MetroidPrime/PortGlobals.cpp`.
 *
 * **It exists because of the vtable, and the vtable is the point.** Until now
 * `src/MetroidPrime/PortReachStubs.cpp` carried
 *
 *     extern "C" char reachstub_data_0[64] asm("_ZTV11CResFactory") = {};
 *
 * - 64 bytes of zeros. It linked, and every call through `CResFactory`'s vtable jumped to address
 * zero. `CResFactory` is constructed during the port's initialisation (it is a
 * `CGameGlobalObjects` member by value) and `CMain::AsyncIdle` calls `gpResourceFactory` through
 * the global, so this was one of the three frame-0 vtables the boot path was waiting on, and a
 * zero vtable is a null dereference the moment anything calls through it.
 *
 * **A vtable is only emitted by the translation unit that defines a class's key function**, which
 * is the first non-pure, non-inline virtual *declared* - and in both compilers that is
 * `~CResFactory`, because `Kyoto/CResFactory.hpp` declares it out of line. So deleting the
 * stub is not enough: something has to define the destructor, and that is what this file is for.
 * With it, GCC emits `_ZTV11CResFactory` into this object with six real slots and the port's
 * `CResFactory` is a working object rather than 64 zero bytes.
 *
 * `CResFactory::Build` is **not** here. It is written, byte-exact, in
 * `src/Kyoto/CResFactoryBuild.cpp` (`configure.py Matching`, retail `fn_802FA960`, 0x802FA960,
 * 0xC0 = 192 bytes, 100.00% with `flip_test` PASS), and that file is in `files.cmake`, so the port
 * compiles it too. Its three callees - `fn_802FAAE4`, `fn_802FA1BC` and `fn_802FA7D4` - are retail
 * functions this tree has not decompiled, so the port's link now asks for them by name. **That is
 * the trade and it is the right way round**: three named holes in the resource chain, which is the
 * part of the boot path that is not written, instead of an unnamed vtable that crashes.
 *
 * `BuildAsync` and `CancelBuild` below are empty because the DOL's are not written; `CanBuild`
 * and `GetResourceIdByName` forward exactly as retail's do. What retail's four are, measured:
 *
 *  * `BuildAsync` - `fn_802FA658`, **0x17C = 380 bytes**, a 128-byte frame, ten arguments to
 *    `fn_802FA140`, and a `new` for a `CLZOInputStream`. The largest of the five and the least
 *    like to be one edit from 100%.
 *  * `CancelBuild` - `fn_802FA490`, 0x84 = 132 bytes. `fn_802FAAE4` again, then a vcall on
 *    `*(node+0x08)+0x14` and `fn_802FA514(this+0xC8, node)`.
 *  * `CanBuild` - `CanBuild__11CResFactoryFRC10SObjectTag`, 0x8008F3C8, **0x24 = 36 bytes**: a
 *    prologue, `addi r3,r3,4` - the `CResLoader` at `CResFactory`+0x04 - a tail call to
 *    `fn_802FCBD0`, and an epilogue. The cheapest of the five by a wide margin.
 *  * `GetResourceIdByName` - `GetResourceIdByName__11CResFactoryCFPCc`, 0x80006B80, also 0x24:
 *    the same three instructions with a tail call to `fn_802FCC44`, the loader's
 *    `GetResIdByName(const char*)`. **Both of the 0x24-byte pair are forwarders retail placed in
 *    the middle of unrelated CGame code**, one of them inside `MetroidPrime/main.cpp`'s claimed
 *    range at 0x800053B8-0x80009880, so promoting either means re-splitting that unit.
 *
 * The two 0x24-byte forwarders' mangled names come from compiling the class declaration with
 * `tools/probe_cc.sh` and reading the object's `.data` relocations, not from guessing: MWCC spells
 * a const member `C` immediately before the parameter list, so `GetResourceIdByName` is
 * `...CFPCc` and not `...FPCc`.
 */
#include "Kyoto/CSimplePool.hpp"
#include "Kyoto/CResFactory.hpp"

// `fn_802FB154` - retail 0x802FB154, `size:0xA8` - is `CResFactory::CResFactory()`, and it lives
// in `src/Kyoto/CResFactoryCtor.cpp`, which is a `configure.py` unit (`NonMatching`, 93.86%). It is
// a C-linkage function taking and returning `CResFactory*` rather than a C++ constructor for a
// measured reason: retail's epilogue has no `mr r3,r31`, and a C++ constructor that returns
// `this` would not reproduce it. **The port still needs a `CResFactory::CResFactory()` to link
// against** - `CGameGlobalObjects` holds the factory by value - so this is where the two are put
// back together. It used to be the other way round: `CResFactoryCtor.cpp` defined
// `CResFactory::CResFactory()` as the *one-shot `CARDInit`* of `fn_803096C4`, the constructor of
// the four bytes at `CGameGlobalObjects`+0x00, which is now
// `src/MetroidPrime/CGameGlobalObjectsPad0Ctor.cpp`.
// **The port's body is a default constructor, not a forwarder to `fn_802FB154`**, and that is a
// measured trade. `src/Kyoto/CResFactoryCtor.cpp` *is* `CResFactory::CResFactory()` and is a
// `configure.py` unit, but it is **excluded from `files.cmake`**: compiling it for the host adds
// four undefined symbols (`fn_802FD0F4`, `fn_802F98D0`, `lbl_803B19B8`, `__vt__11CResFactory` -
// the last because GCC mangles the vtable to `_ZTV11CResFactory` and the name above is the DOL's)
// and closes none, so `tools/link_check.sh` goes 326 -> 330. The measurement is in
// `tools/check_files_cmake.py`. What is left here is a constructor that lets the host build
// `CResFactory`'s own members, which is what this file's other bodies do.
// **The two `rstl::list`s have to start empty, and this is the half of retail's constructor
// that says so.** Retail's `fn_802FB154` writes each list's four pointers to its own
// `xc_empty_prev` and its count to 0 (`stw r7,160(r31)` .. `stw r6,176(r31)` and the second
// set at +0xCC..+0xDC - the store list is in `docs/research/paks.md`, "The `CResFactory`
// interior, measured"). An empty body left all six words of both members indeterminate on the
// host, which was harmless only while nothing read them: `SLoadList` is a plain struct, so no
// member's default constructor touches it. `CResFactory::AsyncIdle` reads `x4_start`, `x8_end`
// and `x14_count` of both, and walking an indeterminate `x4_start` is a segfault rather than a
// wrong answer. `x0_allocator` is deliberately left alone - retail stores nothing there either.
CResFactory::CResFactory() {
  x9c_loading.x4_start = x9c_loading.x8_end = x9c_loading.xc_empty_prev =
      x9c_loading.x10_empty_next = &x9c_loading.xc_empty_prev;
  x9c_loading.x14_count = 0;
  xc8_active.x4_start = xc8_active.x8_end = xc8_active.xc_empty_prev =
      xc8_active.x10_empty_next = &xc8_active.xc_empty_prev;
  xc8_active.x14_count = 0;
}

CResFactory::~CResFactory() {}

void CResFactory::BuildAsync(const SObjectTag& tag, const CVParamTransfer& xfer, IObj** out) {
  *out = nullptr;
}

void CResFactory::CancelBuild(const SObjectTag& tag) {}

// These two are retail's 0x24-byte forwarders, written out: `addi r3,r3,4` - the `CResLoader`
// at +0x04 - and a tail call, to `fn_802FCBD0` (`CResLoaderResAccessors.cpp`) and `fn_802FCC44`
// (`CResLoaderGetResIdByName.cpp`), both written and both in the port build. They used to answer
// `false` and `nullptr`, which made every named lookup fail however many paks were loaded:
// `CGameGlobalObjects::LoadStringTable` asks for `STRG_Main` by name, got null here, and
// `CSimplePool::GetObj(const char*)` dereferenced it.
extern "C" bool fn_802FCBD0(void* resLoader, const SObjectTag& tag);
extern "C" const SObjectTag* fn_802FCC44(void* resLoader, const char* name);

bool CResFactory::CanBuild(const SObjectTag& tag) { return fn_802FCBD0(&x4_resLoader, tag); }

const SObjectTag* CResFactory::GetResourceIdByName(const char* name) const {
  // **The port's stand-in registry gets first refusal here too, and this is the fix
  // `src/MetroidPrime/PortPoolStandIns.cpp` names on its own `sound_lookup_ATBL` entry.** Until
  // now only `CSimplePool::GetObj(const char*)` asked the registry, and this forwarder - which is
  // a *different* name path, straight into `CResLoader::GetResIdByName` - did not. Two retail
  // callers reach the loader this way and neither can be answered by the pool's own table:
  //
  //   * `CMain::FillInAssetIDs` (retail 0x80006B38, `main.cpp:581`), which reads the tag itself
  //     rather than going through the pool; and
  //   * `CEnvFxManager::Initialize` (retail 0x80166880), which is the boot's next wall and asks
  //     for `"DUMB_SnowForces"`. Its fault was `fn_802FCEEC`'s `lwz r4,4(r4)` - a null `SObjectTag`
  //     reached by `*tag` on the line below the call - so with no registry row there is no name
  //     to dereference and no tag to hand the loader.
  //
  // The order matters and is the same as in `CSimplePoolPort.cpp`: the registry, then the real
  // two-list walk. Nothing about the fallback changes, and it still returns `nullptr` for a name
  // nobody has heard of, which is retail's own answer with no pak loaded.
  if (const SObjectTag* const tag = port::pool::FindStandInTag(name)) {
    return tag;
  }
  return fn_802FCC44(const_cast< CResLoader* >(&x4_resLoader), name);
}
