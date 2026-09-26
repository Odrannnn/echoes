// CDarkSamus state predicates, retail DarkSamus .text 0x000116F8..0x00011814.
//
// Twelve virtuals of CDarkSamus that answer "is the machine in state N?", all
// reading one word at `this+0x984`, plus one of them that also reads a byte at
// `this+0xDEC`. Retail's idiom for `x == N` is
//
//     lwz  r0, 0x984(r3)
//     subfic r0, r0, N        ; when N fits in a signed 16-bit immediate
//     cntlzw r0, r0
//     srwi  r3, r0, 5
//
// and, when N does not fit, `lis r0, hi` + `subf r0, r3, r0` with the load
// landing in r3 instead of r0. Which of the two a function gets is decided by
// the constant alone -- 0x20000, 0x100000 and 0x200000 take the `lis` form and
// 0x1 .. 0x400 take `subfic` -- so the constants below are the whole of the
// codegen question here.
//
// The `(x == N) ? 1 : 0` spelling is load-bearing and was measured, not guessed:
// a bare `return x == N;` is one instruction short of retail, because mwcceppc
// narrows the result to a byte and emits `rlwinm r3, r0, 27, 24, 31` where
// retail shifts the whole word with `srwi r3, r0, 5`. Spelled as a conditional
// expression the value is a full-width int and the shift is the retail one
// (`tools/try_batch.py`, 12 bodies, `ternary` at 0 differing instructions).
//
// Keep definitions in descending retail .text order: MWCC emits them in reverse
// source order.
#include "types.h"

extern "C" {

// 0x00011800, 0x14
int fn_10_11800(const void* self) {
  return (*reinterpret_cast<const uint*>(static_cast<const char*>(self) + 0x984) == 0x400) ? 1 : 0;
}

// 0x000117EC, 0x14
int fn_10_117EC(const void* self) {
  return (*reinterpret_cast<const uint*>(static_cast<const char*>(self) + 0x984) == 0x200) ? 1 : 0;
}

// 0x000117D8, 0x14
int fn_10_117D8(const void* self) {
  return (*reinterpret_cast<const uint*>(static_cast<const char*>(self) + 0x984) == 1) ? 1 : 0;
}

// 0x000117A0, 0x38: 0xDEC picks between two of the states above.
int fn_10_117A0(const void* self) {
  const char* p = static_cast<const char*>(self);
  if (*reinterpret_cast<const uchar*>(p + 0xdec) == 1) {
    return (*reinterpret_cast<const uint*>(p + 0x984) == 0x200000) ? 1 : 0;
  }
  return (*reinterpret_cast<const uint*>(p + 0x984) == 0x40) ? 1 : 0;
}

// 0x0001178C, 0x14
int fn_10_1178C(const void* self) {
  return (*reinterpret_cast<const uint*>(static_cast<const char*>(self) + 0x984) == 0x80) ? 1 : 0;
}

// 0x00011778, 0x14
int fn_10_11778(const void* self) {
  return (*reinterpret_cast<const uint*>(static_cast<const char*>(self) + 0x984) == 0x100) ? 1 : 0;
}

// 0x00011760, 0x18
int fn_10_11760(const void* self) {
  return (*reinterpret_cast<const uint*>(static_cast<const char*>(self) + 0x984) == 0x100000) ? 1 : 0;
}

// 0x0001174C, 0x14
int fn_10_1174C(const void* self) {
  return (*reinterpret_cast<const uint*>(static_cast<const char*>(self) + 0x984) == 0x20) ? 1 : 0;
}

// 0x00011738, 0x14
int fn_10_11738(const void* self) {
  return (*reinterpret_cast<const uint*>(static_cast<const char*>(self) + 0x984) == 8) ? 1 : 0;
}

// 0x00011724, 0x14
int fn_10_11724(const void* self) {
  return (*reinterpret_cast<const uint*>(static_cast<const char*>(self) + 0x984) == 0x10) ? 1 : 0;
}

// 0x00011710, 0x14
int fn_10_11710(const void* self) {
  return (*reinterpret_cast<const uint*>(static_cast<const char*>(self) + 0x984) == 4) ? 1 : 0;
}

// 0x000116F8, 0x18
int fn_10_116F8(const void* self) {
  return (*reinterpret_cast<const uint*>(static_cast<const char*>(self) + 0x984) == 0x20000) ? 1 : 0;
}

}
