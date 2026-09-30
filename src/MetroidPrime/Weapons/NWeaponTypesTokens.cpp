// Retail `fn_8018A6EC` (0x8018A6EC..0x8018A748, 0x5C) and
// `NWeaponTypes::lock_tokens` (0x8018A748..0x8018A7A4, 0x5C).
//
// They are the same loop twice - one calls `CToken::Unlock`, the other `CToken::Lock` -
// and the second is 0x5C bytes of it with the two call targets changed:
//
//     8018a6ec  stwu  r1,-16(r1)          8018a748  stwu  r1,-16(r1)
//     ...                                 ...
//     8018a710  bl    Unlock__6CTokenFv   8018a76c  bl    Lock__6CTokenFv
//     8018a718  lwz   r0,4(r30)           8018a774  lwz   r0,4(r30)
//     8018a71c  lwz   r3,12(r30)          8018a77c  lwz   r3,12(r30)
//     8018a720  slwi  r0,r0,3             8018a780  slwi  r0,r0,3
//     8018a724  add   r0,r3,r0            8018a784  add   r0,r3,r0
//     8018a728  cmplw r31,r0              8018a788  cmplw r31,r0
//     8018a72c  bne   8018a70c            8018a78c  bne   8018a768
//
// The loop is precomputed rather than indexed: `r30` is the vector, `r31` walks its
// **data pointer** from `+12`, and the bound is `data + (size << 3)`. The empty vector
// costs one `lwz` of the size and one compare, which is why the backward branch is
// tested before the body rather than after it.
//
// `fn_8018A6EC` is `symbols.txt`'s name for the first of the two. There is no
// `NWeaponTypes::unlock_tokens` in `symbols.txt` because dtk never named it, so the
// declaration in `include/MetroidPrime/Weapons/WeaponCommon.hpp` uses the `fn_` name
// retail's object actually has. The body here is retail's, not a stand-in, and
// `CGunWeapon::UnlockTokens` calls it exactly as retail does.
//
// **Its own file** because both live in the unclaimed gap between
// `MetroidPrime/CDamageInfo.cpp` (which ends at 0x8018A188) and
// `MetroidPrime/Player/CMorphBallShadow.cpp` (which starts at 0x8018A9CC) - 0x8018A6EC
// and 0x8018A748 are 0x544 bytes into that gap, and every function either side of them is
// unclaimed. Claiming the gap is a four-file carve; one function pair per file is the
// arrangement the other single-body port files use (see `CGameAreaSetAreaAttributes.cpp`).
#include "Kyoto/CToken.hpp"
#include "MetroidPrime/Weapons/WeaponCommon.hpp"

namespace NWeaponTypes {

void lock_tokens(rstl::vector< CToken >& tokens) {
  rstl::vector< CToken >::iterator it = tokens.begin();
  const rstl::vector< CToken >::iterator end = tokens.end();
  for (; it != end; ++it) {
    it->Lock();
  }
}

} // namespace NWeaponTypes

// `NWeaponTypes::unlock_tokens`, which `symbols.txt` calls `fn_8018A6EC`. It is the
// `Unlock`-per-element twin of `lock_tokens`; see the note above for the disassembly.
extern "C" void fn_8018A6EC(rstl::vector< CToken >* tokens) {
  rstl::vector< CToken >::iterator it = tokens->begin();
  const rstl::vector< CToken >::iterator end = tokens->end();
  for (; it != end; ++it) {
    it->Unlock();
  }
}
