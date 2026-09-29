#include "MetroidPrime/ScriptLoader.hpp"
#include "Kyoto/Math/CVector3f.hpp"
#include "REL/REL_Setup.h"

// Retail 0x8022E134..0x8022E13C, 8 bytes:
//
//     8022e134  stw  r3,gLoader_IngBlobSwarm@sda21(r0)
//     8022e138  blr
//
// That is this module's loader setter, and it lives in the DOL - not in the module - beside
// `MetroidPrime/ScriptLoader/IngBlobSwarm`'s 44-byte thunk, which dereferences the slot at
// 0x804195E8. The module's own import table names it `fn_8022E134`, so that is the name the
// call below has to use; `strings build/G2ME01/IngBlobSwarm/IngBlobSwarm.plf | grep 8022E`
// is where to read it. A friendlier name will not link.
extern "C" void fn_8022E134(FScriptLoader* loader);

extern "C" {
// A declaration under MWCC, a definition on the host; see CScriptPlayerProxy.cpp, which is
// the same arrangement one module over. This unit's split claims only .text, so the module's
// .bss slot is dtk's to define, and a second definition here is the suspected cause of
// mwldeppc's internal linker error on the modules that do it.
#ifdef __MWERKS__
extern FScriptLoader lbl_31_bss_20;
#else
FScriptLoader lbl_31_bss_20 = 0;
#endif

// The module's own loader, .text 0xD8, 0x40C bytes, still retail. Its signature is fixed by
// what the DOL's LoadIngBlobSwarm thunk calls it with: (CStateManager&, CInputStream&,
// const CEntityInfo&). Only its address is taken here, so the body is not needed to
// reproduce these three functions.
CEntity* fn_31_D8(CStateManager&, CInputStream&, const CEntityInfo&);

// .text 0xA8, 0x30 bytes. Publishes the module's loader and hands the slot to the DOL.
void fn_31_A8() {
  lbl_31_bss_20 = fn_31_D8;
  fn_8022E134(&lbl_31_bss_20);
}

// Every REL module defines RELMain/RELExit, which a flat host link cannot hold, so on the
// host these take distinct names that platform/compiled_modules.cpp would call. The MWCC
// branch is the retail source token for token, so the matching build cannot see this change.
// Neither host name is registered yet: fn_31_D8 is one of the module's unwritten functions,
// so listing this file in files.cmake would add it to the port's link gap, which
// tools/link_check.sh --strict fails on. The module still loads from the disc, where
// retail's own RELMain runs.
#ifdef __MWERKS__
void RELMain() { fn_31_A8(); }

void RELExit() { fn_8022E134(nullptr); }
#else
void mp_relmain_ingblobswarm() { fn_31_A8(); }

void mp_relexit_ingblobswarm() { fn_8022E134(nullptr); }
#endif

// .text 0x3C, 0x28 bytes. Reads the x/y/z of one blob in the module's fixed-stride array, at
// +0xC, +0x1C and +0x2C of the element. It has to be one assignment: three separate
// `SetX/SetY/SetZ` statements emit load-store-load-store, which scores 58.30% against
// retail's load-all-three-then-store.
void fn_31_3C(CVector3f* out, void* self, int index) {
  const char* base = *reinterpret_cast<const char* const*>(static_cast<const char*>(self) + 0x184);
  const float* blob = reinterpret_cast<const float*>(base + index * 0xB8);
  *out = CVector3f(blob[0xC / 4], blob[0x1C / 4], blob[0x2C / 4]);
}

// .text 0x0, 0x3C bytes. Bounds-checks a blob index and returns one bit of the blob: the
// word at +0x17C is the blob count, the word at +0x184 the array, the stride 0xB8, and the
// bit a `bool : 1` at +0xB2 of the element. **The struct's size is load-bearing** - a
// trailing pad of 6 rather than 5 bytes emits `mulli r4,r4,185` against retail's 184 - and
// the flag has to be read as a condition (`if (flag) result = true;`), not as a value, or
// MWCC gives `rlwinm` + `mr r5,r0` where retail has the recording `rlwinm.` + `beq`.
struct SIngBlobSwarmBlob {
  char xc[0xB2];
  bool flag : 1;
  char xd[0x5];
};

bool fn_31_0(void* self, int index) {
  bool result = false;
  if (index > -1) {
    const char* members = static_cast<const char*>(self);
    if (index < *reinterpret_cast<const int*>(members + 0x17C)) {
      const SIngBlobSwarmBlob* blobs = reinterpret_cast<const SIngBlobSwarmBlob*>(
          *reinterpret_cast<const char* const*>(members + 0x184));
      if (blobs[index].flag) {
        result = true;
      }
    }
  }
  return result;
}
}
