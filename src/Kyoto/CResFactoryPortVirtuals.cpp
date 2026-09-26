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
 * The four bodies below are empty because the DOL's are not written. What retail's are, measured:
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
#include "Kyoto/CResFactory.hpp"

CResFactory::~CResFactory() {}

void CResFactory::BuildAsync(const SObjectTag& tag, const CVParamTransfer& xfer, IObj** out) {
  *out = nullptr;
}

void CResFactory::CancelBuild(const SObjectTag& tag) {}

bool CResFactory::CanBuild(const SObjectTag& tag) { return false; }

const SObjectTag* CResFactory::GetResourceIdByName(const char* name) const { return nullptr; }
