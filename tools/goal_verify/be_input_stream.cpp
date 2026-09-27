// Host byte-order test for CInputStream - the acceptance test for goal item port-pak-byteorder.
// Written by the agent that did the fix, kept here because tools/ is outside what an agent may
// edit, so the test that judges the item cannot be weakened by the item.
// Compile with -DTARGET_PC, exactly as the port build does (CMakeLists.txt:148).
#include "Kyoto/Streams/CInputStream.hpp"

#include <cstdio>
#include <cstring>

static int fails = 0;
#define CHECK(cond)                                                                              \
  do {                                                                                           \
    if (!(cond)) {                                                                               \
      printf("FAIL: %s\n", #cond);                                                               \
      fails++;                                                                                   \
    }                                                                                            \
  } while (0)

int main() {
  // The first four bytes of every pak on the disc: 00 03 00 05 -> 0x30005.
  unsigned char buf[32];
  memset(buf, 0, sizeof buf);
  buf[0] = 0x00; buf[1] = 0x03; buf[2] = 0x00; buf[3] = 0x05;  // version
  buf[4] = 0x00; buf[5] = 0x00; buf[6] = 0x00; buf[7] = 0x08;  // name-list length 8
  buf[8] = 0xAB; buf[9] = 0xCD;                                // a 16-bit field
  buf[10] = 0x00; buf[11] = 0x00;
  buf[12] = 0x3F; buf[13] = 0x80; buf[14] = 0x00; buf[15] = 0x00;  // 1.0f

  CInputStream in(buf, sizeof buf, false);

  const int version = in.ReadInt32();
  printf("version    = 0x%08X (want 0x00030005)\n", version);
  CHECK(version == 0x30005);

  const int count = in.ReadInt32();
  printf("count      = %d (want 8)\n", count);
  CHECK(count == 8);
  CHECK(in.GetReadPosition() == 8);

  const u16 half = in.ReadUint16();
  printf("uint16     = 0x%04X (want 0xABCD)\n", half);
  CHECK(half == 0xABCD);
  in.ReadInt16();  // the pad word

  const float f = in.ReadFloat();
  printf("float      = %f (want 1.000000)\n", f);
  CHECK(f == 1.0f);

  // A negative word: FF FF FF FF -> -1, and 00 00 00 01 -> 1 (native would read both as -1 / 0x01000000).
  unsigned char buf2[8];
  memset(buf2, 0xFF, 4);
  memset(buf2 + 4, 0, 4);
  buf2[7] = 1;
  CInputStream in2(buf2, sizeof buf2, false);
  const int neg = in2.ReadInt32();
  const int one = in2.ReadInt32();
  printf("neg        = %d (want -1), one = 0x%08X (want 0x00000001)\n", neg, one);
  CHECK(neg == -1);
  CHECK(one == 1);

  printf(fails ? "BE_TEST FAIL (%d)\n" : "BE_TEST PASS\n", fails);
  return fails != 0;
}
