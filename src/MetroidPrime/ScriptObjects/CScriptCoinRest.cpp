// CScriptCoinRest.cpp - module ScriptCoin (REL id 58), `.text` 0x32B8..0x3374: the
// `rstl::reserved_vector<float, 8>` pair.
//
// The claim was `MetroidPrime/ScriptObjects/CScriptCoinRest.cpp` at `.text 0x1BA4..0x36A4` with no
// source file anywhere, so all 17 of the module's remaining functions sat in one `NonMatching`
// range that no object could reproduce and `Object(NonMatching, ...)` in `configure.py` could not
// resolve the queue's module-qualified target. This unit now carries the two functions the range
// can actually hold, and the rest of the old claim is two `NonMatching` entries with no source -
// the arrangement "The recipe for decompiling a REL module" prescribes: one contiguous range per
// unit, one file per unit, the module's own name for every unit.
//
//   0x32B8 fn_58_32B8  0x40  the fill-all constructor: `mCount = 0`, then `resize(8, value)`,
//                              then `self` returned (`mr r3,r31` in the epilogue).
//   0x32F8 fn_58_32F8  0x7C  `reserved_vector<float, 8>::resize`.
//
// **The bodies are `fn_800D0170`/`fn_800D0130` in `src/MetroidPrime/Player/CMorphBall.cpp`
// unchanged** - that file's `reserved_vector<float, 15>` pair is the same two statements over the
// same 4-byte element, and the spellings it documents are what make these byte-exact:
//
//   * `resize` is **spelled out here rather than called**. Calling `self->resize(n, *value)` emits a
//     weak outlined instantiation and leaves this function a forwarder, which objdiff cannot pair
//     with retail at all.
//   * the fill is `rstl::uninitialized_fill_n`, not a memberwise loop: retail's base is
//     `slwi r0,count,2` / `add r5,r3,r0` / `addi r5,r5,4` - `(self + count*4) + 4` - which is
//     `data() + count`, and the loop body is `stfs`+`addi` on a walked pointer, which is
//     `uninitialized_fill_n`'s `++cur`. MWCC unrolls it eight wide (`srwi. r0,r7,3` / `bdnz`, then
//     the `andi. r6,r6,7` remainder), which is retail's shape.
//   * **the value arrives by pointer, not in a register.** Retail's `lfs f0,0(r5)` dereferences
//     `r5`, and `uninitialized_fill_n`'s `const S& value` keeps `r5` as that address through the
//     loop; a by-value parameter leaves the value already live in `f1` and shifts every
//     subsequent allocation.
//
// **`mw_version="GC/2.7"` is load-bearing and measured.** Under the module's default `GC/1.3.2`,
// `fn_58_32F8` keeps `r6` for both the remaining count and the cursor (`or r7,r0,r0` copies the
// count out, `stfs f0,0(r6)`..`+0x1c` fills through it) and hoists `lfs f0,0(r5)` above the cursor
// arithmetic, which pushes `beq .tail` 0x14 further out than retail's and widens every displacement
// in the function. 2.7 keeps the cursor in `r5` where retail has it and both bodies are
// byte-identical. Same per-object override `CIngBoostBallGuardianF78.cpp` uses.
//
// **Names are the module's own and are reproduced verbatim, so every definition stays C**:
// `config/G2ME01/rels/ScriptCoin/symbols.txt` carries the `fn_<addr>` placeholders, and the linked
// module's symbol table is part of the file the sha1 covers, so a C++ definition would mangle to
// `_Z<len>fn_58_32B8...` and put a name where retail has none. The arrangement
// `CBacteriaSwarmRelTail.cpp` and `CSandBossRelTail.cpp` use.
//
// **`fn_58_32B8` is called from the module's own retail bytes** (`fn_58_28AC` at 0x2D8C) and
// `fn_58_32F8` from `fn_58_32B8`, so both survive the link and neither needs `config.yml`'s
// `force_active`. The 0x3C destructor either side of this pair (`fn_58_327C`, `fn_58_3374`) is
// referenced by nothing in the module and **is deliberately not claimed here** - mwldeppc
// dead-strips it and the module comes out 0x3C short. Structural fact 3 in the module recipe,
// re-measured on this module.
//
// Definitions are in descending retail text order: mwcceppc emits definitions in reverse source
// order and mwldeppc keeps the object's `.text` order verbatim, so ascending would permute the
// module's bytes with objdiff still at 100% and only the module's sha1 would catch it. Every body
// is read off `build/G2ME01/ScriptCoin/asm/MetroidPrime/ScriptObjects/CScriptCoinRest.s`.

#include "rstl/reserved_vector.hpp"

/** 0x32F8, 0x7C = 31 insns: `rstl::reserved_vector<float, 8>::resize`, with the element stride
 *  `slwi ...,2` (4-byte `float`). */
extern "C" void fn_58_32F8(rstl::reserved_vector< float, 8 >* self, int n, const float* value) {
  const int count = self->mCount;
  if (count == n) {
    return;
  }
  if (count <= n) {
    rstl::uninitialized_fill_n(self->data() + count, n - count, *value);
  }
  self->mCount = n;
}

/** 0x32B8, 0x40 = 16 insns: the `N = 8` wrapper over `fn_58_32F8` - `mr r5,r4` / `li r4,8` pass the
 *  literal count and the caller's pointer straight through, `li r0,0` / `stw r0,0(r3)` empties the
 *  vector first, and the epilogue's `mr r3,r31` returns the object, so the return type is a
 *  pointer, not `void`. */
extern "C" rstl::reserved_vector< float, 8 >* fn_58_32B8(rstl::reserved_vector< float, 8 >* self,
                                                          const float* value) {
  self->mCount = 0;
  fn_58_32F8(self, 8, value);
  return self;
}