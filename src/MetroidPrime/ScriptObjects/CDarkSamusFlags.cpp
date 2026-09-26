// CDarkSamus scalar accessors, retail DarkSamus .text 0x0000F840..0x0000F86C.
//
// The tail of a longer accessor group: the u8 member at `this+0xD4C`, two of the
// constants a message id can take chosen by the byte at `this+0xCD0`, and bit 7 of
// the byte at `this+0x90C`. The three shapes are the ones
// `src/MetroidPrime/ScriptObjects/CDarkSamus.cpp` already proves byte-exact: a u8
// member is one `lbz`, a bit read is `lbz` + `extrwi r3,r0,1,24` written `>> 7 & 1`,
// and a two-way choice is `(x == 1) ? A : B`, which puts the default in r3 first and
// branches past the second `li` - that branch is retail's `bnelr`.
//
// Keep definitions in descending retail .text order: MWCC emits them in reverse
// source order, and a permuted object scores 100% per function, passes the report and
// still breaks the module's hash.
#include "types.h"

extern "C" {

// 0x0000F864, 0x8
uchar fn_10_F864(const void* self) {
  return *reinterpret_cast<const uchar*>(static_cast<const char*>(self) + 0xd4c);
}

// 0x0000F84C, 0x18
int fn_10_F84C(const void* self) {
  const uchar b = *reinterpret_cast<const uchar*>(static_cast<const char*>(self) + 0xcd0);
  return (b == 1) ? 0x6a : 0x2e;
}

// 0x0000F840, 0xC: bit 7 of the byte at this+0x90C.
int fn_10_F840(const void* self) {
  return (*reinterpret_cast<const uchar*>(static_cast<const char*>(self) + 0x90c) >> 7) & 1;
}

}
