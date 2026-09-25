#include "MetroidPrime/Player/CGameOptions.hpp"

#include "rstl/pair.hpp"

// Retail 0x80227694, 0x14 bytes. A free function of CGameOptions.cpp's translation
// unit - it sits inside dtk's auto_03_8021BFE0_text blob and retail gives it no name.
//
// CGameOptions::CGameOptions (0x80161B48) calls it exactly once, at 0x80161BF0, and
// then copies the two bytes it produced into the four pair<bool,bool> slots of unk2
// at +0x3C, +0x3E, +0x40 and +0x42. So this is "the default value of unk2" rather
// than a member of CGameOptions: it reads no field of the object being constructed and
// takes no arguments at all.
//
// It takes no arguments, yet r3 is a pointer, because MWCC returns a class type through
// a caller-supplied slot. The last write to r3 before the call is `addi r3, r1, 8` at
// 0x80161B7C, and the constructor's own locals at r1+8 and r1+9 are what this body
// writes - the same two bytes the constructor then fans out four ways.
//
// The two fields are assigned rather than passed to the constructor on purpose. With
// `pair(true, false)` MWCC takes the address of both bool literals and materialises them
// in .sdata (`lbz r0, @105` / `lbz r0, @106`); retail's body holds the 1 in r4 and the 0
// in r0, which is what default-constructing and then assigning produces. Note the
// `const L&` constructor in rstl/pair.hpp cannot be changed to fix this: it is what
// rstl/string.hpp's position_iterator goes through, and taking it by value moves that
// function 0x30 bytes and breaks the DOL.
extern "C" rstl::pair< bool, bool > fn_80227694() {
  rstl::pair< bool, bool > result;
  result.first = true;
  result.second = false;
  return result;
}
