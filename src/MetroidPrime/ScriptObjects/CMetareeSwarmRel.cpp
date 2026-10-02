// CMetareeSwarmRel.cpp - MetareeSwarm's (module 43) head, .text 0x0..0xD8: the five functions
// above the module's class code. Same arrangement as
// `MetroidPrime/ScriptObjects/CScriptPlayerProxy.cpp`, and the ranges come from
// `config/G2ME01/rels/MetareeSwarm/symbols.txt`:
//
//   0x00  fn_43_0   0x3C   index-guarded flag test on record[index], index * 0xB8
//   0x3C  fn_43_3C  0x28   record[index].x0C/x1C/x2C -> *out, index * 0xB8
//   0x64  RELExit  0x24   li r3,0 / bl fn_8022D5A8
//   0x88  RELMain  0x20   bl fn_43_A8
//   0xA8  fn_43_A8 0x30   lbl_43_bss_20 = fn_43_D8 ; fn_8022D5A8(&lbl_43_bss_20)
//
// Everything else in the module is left unclaimed, so dtk fills it from retail and the module's
// sha1 against `config/G2ME01/config.yml` still holds (measured: e9b5a7bd..., unchanged). The one
// neighbour left retail is fn_43_D8 (0xD8, 0x3D8), the module's own entity loader: behavioural
// class code, and it needs the CActor/CPatterned hierarchy this tree does not have.
//
// The two callees are named by what they are, not invented:
//   - `fn_8022D5A8` is the DOL's 0x8022D5A8, two instructions, `stw r3, gLoader_MetareeSwarm; blr`.
//     So it stores the *address* of a loader slot, not a loader. `LoadMetareeSwarm` at 0x8022D57C
//     reads it as `lwz r6, gLoader_MetareeSwarm; lwz r12, 0(r6); mtctr r12; bctrl`, which is why
//     the store below hands it `&lbl_43_bss_20` and why that slot is four bytes wide.
//   - `lbl_43_bss_20` is `.bss:0x20`, `size:0x4 data:4byte`: the module's own copy of the loader
//     pointer. This unit's split claims .text only, so dtk's `.bss` object has to define it, and
//     a second definition under MWCC is what produced mwldeppc's internal linker error on
//     ScriptPlayerProxy. Hence extern under MWCC and a host definition.
//
// **The three floats in fn_43_3C have to be built, not indexed.** Retail loads all three before it
// stores any (`lfs f2,0x2c / lfs f1,0x1c / lfs f0,0xc / stfs f0,0 / stfs f1,4 / stfs f2,8`), one
// register each. Spelled `out[0] = values[3]; out[1] = values[7]; out[2] = values[11];` it compiles
// to the same ten instructions in the *interleaved* order - load, store, load, store - and scores
// 58.30% (measured, 2026-09-29). `CVector3f`'s three-argument constructor is what hoists the
// three arguments into f0/f1/f2 before the copy, and that is 100.00%.
//
// **fn_43_0's flag test is `(byte >> 7) & 1`, not a bit-24 test.** Its tail is
// `lbzx r0,3(r0) / rlwinm. r0,r0,25,31,31 / beq`, and `rlwinm. r0,r0,25,31,31` is what
// `mwcceppc` emits for `(uchar >> 7) & 1`: the shift is a rotate-left by 32-7=25 and the `& 1`
// narrows the mask to one bit. The in-tree proof is the Matching accessor `fn_42_36C`
// (`CScriptMetaree.cpp:78`), whose retail word is `rlwinm r3,r0,29,31,31` and whose source says
// `>> 3` - same shape, shift 3, so the encoding is 32-n. An earlier reading of the same function
// took dtk's `extrwi. r0,r0,1,24` spelling literally, concluded the bit was always 0 and called
// the function retail dead code; that was wrong and it is superseded (see RUNNING_THE_DECOMP.md).
//
// Definitions are in descending retail text order: mwcceppc emits definitions in reverse source
// order and mwldeppc keeps the object's `.text` order verbatim, so ascending would permute the
// module's bytes with objdiff still at 100%.

#include "Kyoto/Math/CVector3f.hpp"
#include "MetroidPrime/ScriptLoader.hpp"
#include "REL/REL_Setup.h"
#include "types.h"

extern "C" {
CEntity* fn_43_D8(CStateManager&, CInputStream&, CEntityInfo&);
void fn_8022D5A8(FScriptLoader* loader);

#ifdef __MWERKS__
extern FScriptLoader lbl_43_bss_20;
#else
FScriptLoader lbl_43_bss_20 = 0;
#endif

void fn_43_A8() {
  lbl_43_bss_20 = fn_43_D8;
  fn_8022D5A8(&lbl_43_bss_20);
}

// Every REL module defines RELMain/RELExit, which a flat host link cannot hold, so on the
// host these take distinct names. The MWCC branch is the retail source token for token, so the
// matching build cannot see this change.
//
// **Nothing calls the host pair, and that is deliberate.** Listing this file in `files.cmake`
// would make the port link `fn_43_D8` and `fn_8022D5A8`, which it cannot, and
// `tools/link_check.sh --strict` fails on a growing undefined count (measured 314 -> 316). So
// the port keeps reading `MetareeSwarm.rel` off the disc through `platform/rel.cpp` instead of
// compiling the module in, and the `#else` branch exists only so the file is still a valid
// translation unit. The same wall is why CScriptPlayerProxy.cpp's `fn_62_188` is in the
// link-gap list rather than defined.
#ifdef __MWERKS__
void RELMain() { fn_43_A8(); }

void RELExit() { fn_8022D5A8(nullptr); }
#else
void mp_relmain_metareeswarm() { fn_43_A8(); }

void mp_relexit_metareeswarm() { fn_8022D5A8(nullptr); }
#endif

// .text 0x3C, 0x28 bytes. `self+0x184` is a pointer to an array of 0xB8-byte records, and the
// three floats read out of one of them are 0x10 apart, so this is a 0xB8-byte record whose
// 0x0C, 0x1C and 0x2C are three separate floats rather than a 12-byte vector.
void fn_43_3C(CVector3f* out, const void* self, int index) {
  const char* element =
      *reinterpret_cast<char* const*>(static_cast<const char*>(self) + 0x184) + index * 0xB8;
  const float* values = reinterpret_cast<const float*>(element);
  *out = CVector3f(values[0x0C / 4], values[0x1C / 4], values[0x2C / 4]);
}

// .text 0x0, 0x3C bytes. The module's only free function that answers a question about one
// record: `self+0x17C` is the count and `self+0x184` the array, both shared with fn_43_3C above
// (`mulli r4,r4,0xb8` against a `lwz r0,0x17c(r3) / cmpw r4,r0` bound check), so the record is
// 0xB8 bytes wide and +0xB2 of it is a byte of flags whose top bit this returns. Both guards are
// signed, hence `cmpwi r4,-1` and `cmpw r4,r0` rather than the unsigned forms.
//
// Two spellings below are not the obvious ones, and both were measured (2026-09-29):
//   - `index > -1`, not `index >= 0`. They are the same test, but `mwcceppc` gives `>= 0` a
//     `cmpwi r4,0 / blt` and only the `<= -1` form gives retail's `cmpwi r4,-1 / ble`.
//   - the flag byte is read through a pointer dereference,
//     `*(...)(records + index * 0xB8 + 0xB2)`, not as a subscript `records[index * 0xB8 + 0xB2]`.
//     The subscript folds the constant into the load's displacement
//     (`mulli r0,r4,0xb8 / add r3,r3,r0 / lbz r0,0xb2(r3)`); retail keeps the base in `r3` and
//     the whole offset in `r0` and loads indexed (`addi r0,r4,0xb2 / lbzx r0,3,r0`).
//     `fn_43_3C` above is the opposite spelling over the same array, and it is the one retail used.
int fn_43_0(const void* self, int index) {
  int result = 0;
  if (index > -1) {
    const char* self_bytes = static_cast<const char*>(self);
    if (index < *reinterpret_cast<const int*>(self_bytes + 0x17C)) {
      const uchar* records = *reinterpret_cast<uchar* const*>(self_bytes + 0x184);
      const uchar flags = *reinterpret_cast<const uchar*>(records + index * 0xB8 + 0xB2);
      if ((flags >> 7) & 1) {
        result = 1;
      }
    }
  }
  return result;
}
}
