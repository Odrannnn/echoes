/**
 * `fn_80145ACC` (retail 0x80145ACC, `size:0xC4` = 196 bytes, 0x80145ACC..0x80145B90) - the option
 * map's **set-if-absent**, written here for the port because the decompilation's own copy cannot be
 * linked: `src/MetroidPrime/Player/CPersistentOptionsMapInsert.cpp` is `NonMatching` at 71.37% and
 * relocates against `fn_80146338`, the rbtree node insert, which no unit implements.
 *
 * It is not a stub. It does the work retail does, on the map the object actually has.
 *
 * ## The map is `mVariables`, and there is only one
 *
 * `CPersistentOptions` (`CHECK_SIZEOF(CPersistentOptions, 0x2c)`) is `CGameStateEnvVarManager`
 * (0x18 = a 4-byte scope word plus a 0x14 `red_black_tree`) followed by `mCinematicStates`
 * (0x10) and `mSaveIdx` (4). **0x2C leaves no room for a second map**, and retail's four target
 * offsets all agree with the first one:
 *
 *  - `fn_80145ACC` searches and inserts at `self + 4` (0x80145AF4/0x80145AFC);
 *  - `fn_80146154` (`CPersistentOptionsCtor.cpp`, `Matching`) zeroes `+0x08`, `+0x0C`, `+0x10`,
 *    `+0x14` and nothing else - which is exactly a map at `+0x04`: count at `+0x08`, then the
 *    header's three words at `+0x0C`, `+0x10`, `+0x14`. A map at `+0x00` would leave its count at
 *    `+0x04` unzeroed and overshoot by one word into `mCinematicStates`;
 *  - `include/MetroidPrime/Player/CPersistentOptionsMap.hpp`'s `SMap` is `CHECK_SIZEOF(SMap, 0x18)`
 *    and its last word is `x14_unk`, so it ends at `self + 0x1C` - the base class's end.
 *
 * So the eleven rows `fn_80145C98` builds land in the environment-variable map that
 * `AddVariable` and `FindEnvironmentVariable` already use. Retail keeps two out-of-line copies of
 * the same body over that one map (`fn_80145ACC` for the option table, `fn_80145B0C` for the
 * variables read off the memory card); the port has one function, called by both.
 *
 * ## The two value types are the same twelve bytes with the same meaning
 *
 * `SPersistentOptionsValue` is `{lo, hi, value}` and `CEnvironmentVariable` is `{mMin, mMax,
 * mValue}`; both are three `int`s and both `CHECK_SIZEOF` 0xC. Each constructor clamps the third
 * word into `[first, second]` (retail's clamp is `fn_801461AC`, called from `fn_801462DC`), and
 * every one of the eleven rows has `lo == 0` with a default inside the range, so the clamp is a
 * no-op for all of them - retail passes the three numbers for the same reason.
 *
 * ## `mVariables` is private, so the class is mirrored rather than reopened
 *
 * `CGameStateEnvVarManager::mVariables` is private and this file is not that class, and the
 * alternative - making the member public - would touch a header that four `Matching` units
 * include. So the two-member class is mirrored locally, which is the convention the three
 * neighbours already use (`SFirst1C` in `CPersistentOptionsCtor.cpp`, `SGameStateVarTree` in
 * `CGameState.cpp`, `SMap` in `CPersistentOptionsMap.hpp`). The offset is not hard-coded: it is
 * the compiler's own placement of a 4-byte member before an 8-byte-aligned one, and the mirror's
 * size is asserted against the class's.
 *
 * This file is **not** in `configure.py`, so it cannot affect `main.dol` or any of the 86 RELs.
 * `configure.py` does not mention `fn_80145ACC` either, so no retail offset is claimed here.
 */

#include "types.h"

#include "MetroidPrime/Player/CPersistentOptions.hpp"
#include "MetroidPrime/Player/SPersistentOptionsValue.hpp"

#include "rstl/map.hpp"

namespace {

// `CGameStateEnvVarManager`'s two members, byte for byte: the 4-byte scope word, then the map.
// `CHECK_SIZEOF` below is the assertion that this is the class and not a lookalike.
struct SEnvVarManagerMirror {
  CGameStateEnvVarManager::EVariableScope x00_scope;
  rstl::map< rstl::string, CEnvironmentVariable > x08_variables;
};
CHECK_SIZEOF(SEnvVarManagerMirror, 0x28)

} // namespace

extern "C" void fn_80145ACC(CPersistentOptions* self, const rstl::string& name,
                            const SPersistentOptionsValue& value) {
  rstl::map< rstl::string, CEnvironmentVariable >& variables =
      reinterpret_cast< SEnvVarManagerMirror* >(self)->x08_variables;

  // Retail's test is `it == end()` on both words of the eight-byte iterator, and the insert is
  // reached only from that arm (0x80145B2C onwards). `rstl::map::find` and `end()` are the same
  // node-plus-header comparison.
  if (variables.find(name) == variables.end()) {
    variables.insert(rstl::pair< rstl::string, CEnvironmentVariable >(
        name, CEnvironmentVariable(value.x00_lo, value.x04_hi, value.x08_value)));
  }
}