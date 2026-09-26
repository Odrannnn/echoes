// CDarkSamus accessors, retail DarkSamus .text 0x0000CE4C..0x0000CED0.
//
// The whole module's symbol table is unnamed retail (`fn_10_<offset>` for 526 of
// 535 functions), so the bodies are keyed by retail offset, and the unit's claim
// in `config/G2ME01/rels/DarkSamus/splits.txt` is the twelve functions this file
// reproduces. Everything either side stays with dtk's `auto_*` objects, which is
// what keeps the module's sha1 equal to `config/G2ME01/config.yml`.
//
// The bodies themselves are CPatterned/CMetaree base-class accessors, so the
// shapes below are the ones `src/MetroidPrime/ScriptObjects/CScriptMetaree.cpp`
// already proves byte-exact: a `+N` reference return is one `addi`, a `u8` member
// is one `lbz`, a bitfield read is `lbz`+`extrwi`, and a three-float copy out of
// `this+0x54` is three `lfs`/`stfs` pairs.
//
// Keep definitions in descending retail .text order: MWCC emits them in reverse
// source order, and the module's bytes come out permuted otherwise.
#include "types.h"

struct TUniqueId {
  ushort value;
};

extern const TUniqueId kInvalidUniqueId;
extern float lbl_8041B758;

extern "C" {

// 0x0000CEB4, 0x1C: copy the three floats at this+0x54 out through the return slot.
void fn_10_CEB4(float* out, const void* self) {
  const float* src = reinterpret_cast<const float*>(static_cast<const char*>(self) + 0x54);
  out[0] = src[0];
  out[1] = src[1];
  out[2] = src[2];
}

int fn_10_CEAC(void*) { return 0; }

int fn_10_CEA4(void*) { return 0; }

int fn_10_CE9C(void*) { return 1; }

// 0x0000CE94, 0x8: a reference to the member object at this+0x754.
void* fn_10_CE94(void* self) { return static_cast<char*>(self) + 0x754; }

float fn_10_CE88() { return lbl_8041B758; }

// 0x0000CE7C, 0xC: bit 3 of the byte at this+0x34C, which is the same
// CPatterned::x34c_28_ field CScriptMetaree reads at the same offset.
int fn_10_CE7C(const void* self) {
  return (*reinterpret_cast<const uchar*>(static_cast<const char*>(self) + 0x34c) >> 3) & 1;
}

void fn_10_CE6C(TUniqueId* self) { *self = kInvalidUniqueId; }

int fn_10_CE64(void*) { return 0; }

int fn_10_CE5C(void*) { return 0; }

// 0x0000CE54, 0x8: the u8 member at this+0x44F.
uchar fn_10_CE54(const void* self) {
  return *reinterpret_cast<const uchar*>(static_cast<const char*>(self) + 0x44f);
}

// 0x0000CE4C, 0x8: a reference to the member object at this+0x7C0.
void* fn_10_CE4C(void* self) { return static_cast<char*>(self) + 0x7c0; }

}
