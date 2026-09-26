// CDarkSamus u8 members, retail DarkSamus .text 0x00016B48..0x00016B64.
//
// Three virtuals that read one byte each out of `this`: a bitfield read at
// +0xAB8, and two plain u8 members at +0xA80 and +0xA40. The shapes are the ones
// `src/MetroidPrime/ScriptObjects/CDarkSamus.cpp` already proves byte-exact - a u8
// member is one `lbz`, and a bit read is `lbz` + `extrwi r3,r0,1,24` with the
// source written `>> 7 & 1`.
//
// Keep definitions in descending retail .text order: MWCC emits them in reverse
// source order, and a permuted object scores 100% per function, passes the report
// and still breaks the module's hash.
#include "types.h"

extern "C" {

// 0x00016B5C, 0x8
uchar fn_10_16B5C(const void* self) {
  return *reinterpret_cast<const uchar*>(static_cast<const char*>(self) + 0xa40);
}

// 0x00016B54, 0x8
uchar fn_10_16B54(const void* self) {
  return *reinterpret_cast<const uchar*>(static_cast<const char*>(self) + 0xa80);
}

// 0x00016B48, 0xC: bit 7 of the byte at this+0xAB8.
int fn_10_16B48(const void* self) {
  return (*reinterpret_cast<const uchar*>(static_cast<const char*>(self) + 0xab8) >> 7) & 1;
}

}
