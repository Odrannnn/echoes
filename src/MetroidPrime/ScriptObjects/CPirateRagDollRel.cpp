// CPirateRagDollRel.cpp - PirateRagDoll's (module 50) module head, .text 0x0..0x10C: the four
// entry functions. Same arrangement as `MetroidPrime/ScriptObjects/CGrenchlerRel.cpp` and
// `MetroidPrime/ScriptObjects/CMysteryFlyerRel.cpp`, and the ranges come from
// `config/G2ME01/rels/PirateRagDoll/symbols.txt`:
//
//   0x000 RELExit  0x24  li r3,0 / bl fn_80227538
//   0x024 RELMain  0x20  bl fn_50_44
//   0x044 fn_50_44  0x30  lbl_50_bss_188 = fn_50_74 ; fn_80227538(&lbl_50_bss_188)
//   0x074 fn_50_74  0x98  __nw__FUlPCcPCc(0x11C, lbl_50_rodata_8C, nullptr), then fn_50_1938
//
// **Why this range and not more.** The family claims the whole head, 0x0 up to the module's first
// class function, and this module's first *five* functions are the head plus `fn_50_10C`
// (0x10C, 0x2A8) - which is the module's ragdoll constraint solver, not an accessor: it indexes
// two parallel 0x44-byte node arrays off a `mulli`, calls `CVector3f::AsNormalized` and
// `CQuaternion::YRotation`, and writes quaternion and vector temporaries to its own frame. That
// needs the CActor hierarchy this tree does not model, so the claim stops at the end of the head
// at 0x10C and `fn_50_10C` and everything above it stay unclaimed, filled from retail by dtk,
// which is what keeps the module's sha1 against `config/G2ME01/config.yml` holding. Everything
// below 0x0 does not exist.
//
// The callees are named by what they are, not invented:
//   - `fn_80227538` is the DOL's 0x80227538, two instructions, `stw r3, lbl_80419568@sda21(r0);
//     blr` (`build/G2ME01/asm/auto_03_80227530_text.s`). So it stores the *address* of a loader
//     slot, not a loader, which is why the store below hands it `&lbl_50_bss_188` and why that
//     slot is four bytes wide. `src/MetroidPrime/ScriptLoader/PirateRagDoll.cpp` reads it the
//     same way every other REL loader does, and the setter is deliberately not claimed in the
//     DOL because REL modules import it by its retail name - so it stays in dtk's auto unit and
//     this declaration is the same one `CMysteryFlyerRel.cpp` makes for `fn_80232868`. It is
//     `extern "C"`: an alias would be a different symbol and the call would resolve to nothing.
//   - `lbl_50_bss_188` is `.bss:0x188`, `size:0x4` (`.obj lbl_50_bss_188, global` in
//     `build/G2ME01/PirateRagDoll/asm/auto_05_00000000_bss.s`): the module's own copy of the
//     loader pointer. It is **`lbl_50_bss_188` and not `lbl_50_bss_18`**, which is a different
//     object the module's own code uses far above the head. This unit's split claims `.text`
//     only, so dtk's `.bss` object has to define it, and a second definition under MWCC is what
//     produced mwldeppc's internal linker error on ScriptPlayerProxy. Hence extern under MWCC
//     and a host definition.
//   - `lbl_50_rodata_8C` is `.rodata:0x8C`, 0x9C bytes (`build/G2ME01/PirateRagDoll/asm/
//     auto_03_00000000_rodata.s`), the module's own `__FILE__`-style assertion path. It needs the
//     **explicit `extern` keyword**: without it mwcceppc reads
//     `extern "C" const char lbl_50_rodata_8C[];` as a *definition* and asks for an initializer.
//     Same reasoning as the module-local `const float` of `CMysteryFlyerRel.cpp`.
//   - `fn_50_1938` is `.text 0x1938`, unclaimed: the module's own entity loader. It is reached
//     by a direct `bl` from `fn_50_74`, which is in the module's `ldscript.lcf` FORCEACTIVE
//     block, so it survives the `.plf` link's `-strip_partial` without a `force_active:` entry.
//
// **No dead-strip hazard, and that is measured, not assumed.**
// `build/G2ME01/PirateRagDoll/ldscript.lcf` lists `fn_50_74` in its FORCEACTIVE block,
// `fn_50_44` is a direct `bl` from `RELMain`, and RELMain/RELExit are this module's entry
// points, referenced by `_prolog`/`_epilog`, which the shared `REL/REL_Setup.cpp` unit defines.
// `tools/audit_rel_claim.py PirateRagDoll` prints the preplf/plf symbol counts that measure it.
//
// Definitions are in descending retail text order: mwcceppc emits definitions in reverse source
// order and mwldeppc keeps the object's `.text` order verbatim, so ascending would permute the
// module's bytes with objdiff still at 100% and only `tools/flip_test.sh` would catch it. Note
// the sharper form of that rule, measured here: the order has to be **strictly** descending, so
// `fn_50_74` comes first even though `RELExit` is the lowest offset - the compiler groups a
// callee after its caller otherwise, and `fn_50_44` is only a `lis`/`addi` pair away from calling
// `fn_50_74`. Check after building with `python3 tools/check_decl_order.py --unit CPirateRagDoll`.

#include "MetroidPrime/ScriptLoader.hpp"
#include "REL/REL_Setup.h"

extern "C" const char lbl_50_rodata_8C[];

// The loader this module registers. Its shape is fixed by the register assignment of the call
// `fn_50_1938(r3 = allocated block, f1, f2, r4..r8 = the five incoming pointers)` - seven
// arguments with the two floats second and third - and it is a `void*`-sized slot either way.
typedef void* (*CPirateRagDollLoader)(void*, void*, void*, void*, void*, float, float);

extern "C" {
void fn_80227538(void* loader);
void* __nw__FUlPCcPCc(uint size, const char* file, const char* function);

// .text 0x1938, unclaimed: the module's own entity loader, reached by a direct `bl` below.
void* fn_50_1938(void* block, float f, float g, void* a, void* b, void* c, void* d, void* e);

#ifdef __MWERKS__
extern CPirateRagDollLoader lbl_50_bss_188;
#else
CPirateRagDollLoader lbl_50_bss_188 = nullptr;
#endif

// .text 0x74, 0x98 bytes. Retail allocates 0x11C bytes from the module's own assertion path and,
// if that succeeded, runs the block's own loader on the result, passing the five incoming
// pointers through unchanged and the two incoming floats in f1/f2. The frame is 0x30 because the
// five pointer arguments and the double's two halves are all held across the allocation call.
void* fn_50_74(void* a, void* b, void* c, void* d, void* e, float f, float g) {
  void* block = __nw__FUlPCcPCc(0x11C, lbl_50_rodata_8C, nullptr);
  if (block != nullptr) {
    block = fn_50_1938(block, f, g, a, b, c, d, e);
  }
  return block;
}

// .text 0x44, 0x30 bytes: the module's loader registration. Retail loads the loader out of
// `.text` and the slot address out of `.bss`, and stores the loader with `stwu` so the store
// writes the slot itself and leaves r3 holding its address for the setter call.
void fn_50_44() {
  lbl_50_bss_188 = fn_50_74;
  fn_80227538(&lbl_50_bss_188);
}

// Every REL module defines RELMain/RELExit, which a flat host link cannot hold, so on the host
// these take distinct names. The MWCC branch is the retail source token for token, so the
// matching build cannot see this change.
//
// **Nothing calls the host pair, and that is deliberate**, exactly as in `CMysteryFlyerRel.cpp`
// and `CPlantScarabSwarmRel.cpp`: this file is deliberately absent from `files.cmake`, because
// listing it would make the port link `fn_50_1938` and `fn_80227538`, which it cannot, and
// `tools/link_check.sh --strict` fails on a growing undefined count. So the port keeps reading
// `PirateRagDoll.rel` off the disc through `platform/rel.cpp`, and the `#else` branch exists
// only so the file is still a valid translation unit for `tools/probe_sources.sh`.
#ifdef __MWERKS__
void RELMain() { fn_50_44(); }

void RELExit() { fn_80227538(nullptr); }
#else
void mp_relmain_pirateragdoll() { fn_50_44(); }

void mp_relexit_pirateragdoll() { fn_80227538(nullptr); }
#endif
} // extern "C"