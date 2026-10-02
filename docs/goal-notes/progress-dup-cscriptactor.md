# progress-dup-cscriptactor

Target unit `main/MetroidPrime/ScriptObjects/CScriptActor`, `progress` item (stays `NonMatching`).
**Measured: `matched_functions` 14 -> 19 of 34; fuzzy 77.60% -> 82.78%; matched code 3184 -> 3636
of 8732 bytes.** `tools/goal_check.sh build/goal/item.json` -> `PASS` (gate.sh, counts, symbol
names, build, target_rose, no asm). No function anywhere got worse - `gate.sh`'s per-function
report diff is part of that PASS.

Diff: `src/MetroidPrime/ScriptObjects/CScriptActor.cpp` and `config/G2ME01/symbols.txt` only.

## What was matched, per function

| retail symbol | before | after | the construct |
| --- | --- | --- | --- |
| `fn_80070CF4` (108 B) | no match (0.00%, retail's map had a placeholder name) | 100% | renamed to `__dt__Q24rstl48optional_object<29TLockedToken<13COBBTreeGroup>>Fv` in `symbols.txt` |
| `fn_80070C68` (140 B) | no match | 100% | renamed to `__ct__15CProjectileInfoFRC15CProjectileInfo` |
| `fn_80070AD8` (132 B) | no match | 100% | renamed to `__as__Q24rstl48optional_object<29TLockedToken<13COBBTreeGroup>>FRC29TLockedToken<13COBBTreeGroup>` |
| `fn_8006F574` (40 B) | no match | 100% | written: `extern "C" void fn_8006F574(CModelData*, const CModelData&) { new (dest) CModelData(src); }` |
| `fn_8006F554` (32 B) | no match | 100% | written: `extern "C" void fn_8006F554(CModelData*, const CModelData&) { fn_8006F574(dest, src); }` |

### The three renames

**These were never missing code.** mwcceppc already emits all three, byte for byte, under the
mangled names above; retail's map simply had `fn_` placeholders there, so objdiff could not pair
the two sides. I paired them by instruction bytes with a lane-private script that reads
`build/G2ME01/obj/...` and `build/G2ME01/src/...`, replaces every relocated word with its target
symbol name, and compares:

```
fn_80070CF4   27 insns  __dt__Q24rstl48optional_object<29TLockedToken<13COBBTreeGroup>>Fv
fn_80070C68   35 insns  __ct__15CProjectileInfoFRC15CProjectileInfo
fn_80070AD8   33 insns  __as__Q24rstl48optional_object<29TLockedToken<13COBBTreeGroup>>FRC29TLockedToken<13COBBTreeGroup>
```

That is the same fix as commit `53543a9f` (`fn_8009D3D8` -> `__dt__16CUnknownItemListFv`), which
the reviewer accepted after byte-comparing the DOL itself: `orig/G2ME01/sys/main.dol` carries no
symbol table, so a name in `config/G2ME01/symbols.txt` cannot change a byte of the binary. The
gate re-checks the DOL sha1 and all 86 RELs regardless, and they hold. `dtk dol split` reruns by
itself (`build/G2ME01/config.json: split config/G2ME01/config.yml`), so no reconfigure is needed.
Neither mangled name occurred anywhere else in `symbols.txt`, so no name is defined twice.

**For the 101 copies in other modules: this is the line to write them from.** Same fix, per
address - `rstl::optional_object<TLockedToken<T>>::~optional_object()` is 27 instructions and its
only two calls are `__dt__6CTokenFv` and `Free__7CMemoryFPCv`.

### The two `rstl::construct` halves

`fn_8006F574` is `rstl::construct_impl<CModelData>` (the placement new, with its own
`cmplwi r3,0` guard, in mwcc's order - the compare is issued *before* the LR is spilled) and
`fn_8006F554` is the `rstl::construct<CModelData>` that forwards to it with r3/r4 untouched.
`LoadActor` builds its `optional_object<CModelData>` from a `CModelData`, which this port spells
with one `rstl::construct` call, so mwcceppc has no reason to outline the pair and the unit
emitted neither. Retail's `LoadActor` had a second construct call; that is what put the outlined
copies in this object. C linkage, so the definitions carry retail's unmangled names -
mwcceppc mangles the template instantiations and nothing in C++ can emit one under a `fn_` name
(the same reason `src/MetroidPrime/TypesMatch.cpp:229` has).

They are declared **between `GetPrimitiveTransform` and `LoadActor`**, because mwcceppc emits in
reverse source order and retail has them at 0x8006F554/0x8006F574, between `LoadActor` and
`GetPrimitiveTransform`. First attempt put them at the top of the file and `gate.sh` failed
`GATE FAIL: decl-order`; `tools/check_decl_order.py --unit MetroidPrime/ScriptObjects/CScriptActor`
is what shows the fix. Note the emitted order is 0xC8 `LoadActor`, 0xBB4 `fn_8006F554`,
0xBD4 `fn_8006F574`, 0xBFC `GetPrimitiveTransform`.

## Not done, and what it costs

### `fn_8006FB10` (72 B, 21 copies) and `fn_8006FB58` (176 B) - `CImpactVisorEffect`'s constructors

Decoded, and both halves are needed (`fn_8006FB10` calls `fn_8006FB58`):

* `fn_8006FB58` = `CImpactVisorEffect::CImpactVisorEffect(const optional_object<SParticleEffect>&,
  const optional_object<SBlurEffect>&, const optional_object<pair<int,float>>&)`. It calls
  0x8003607C (the out-of-line construction of `mParticleEffect`), then copies `mBlurEffect`
  (0x18) and `mLowPassFilter` (0x28) from the two optionals, then stores `-1` at 0x34 and
  **4.0f** at 0x38. The layout is `CImpactVisorEffect` exactly as `include/MetroidPrime/Weapons/
  CImpactVisorEffect.hpp` already declares it (0x3C, `mParticleEffect` 0x00, `mBlurEffect` 0x18,
  `mLowPassFilter` 0x28), so nothing in `include/` needs fixing - retail simply emitted the
  `SParticleEffect` copy out of line, which needs a second call site this unit does not have.
* `fn_8006FB10` = the default constructor: three default-constructed optionals on the stack
  (`stb r0` at +20, +12, +8 of three locals at sp+36, sp+20, sp+8), a forward to `fn_8006FB58`,
  then `fn_80032D0C(sp+36, -1)` for the one local with a non-trivial destructor.

A spelling that gets most of `fn_8006FB58` (a local `SVisorEffect` mirror + placement news)
misses on two points that both have to be fixed together:

1. retail stores the blur flag as `stb r0,36(r29)` *before* computing `addic. r3,r29,24`, i.e. the
   flag store does not go through the destination pointer; the placement-new spelling reorders it
   to `addic. r4,r29,24` first and `stb r0,12(r4)`.
2. the `4.0f` is `lfs f0,-31044(r2)` - an `_SDA_BASE_`-relative load, so the constant's **slot**
   in `.sdata2` has to match. Retail's `.sdata2` is
   `[1.0f, 0.0f, 4.0f, 0.727f, 0.25f, -1.0f]` and ours today is
   `[1.0f, 0.0f, 0.727f, 0.25f, -1.0f, 0.3f]`: the slot order is set by the order the constants are
   first used in the unit, so adding `4.0f` means moving it, and every other function in the unit
   that loads those constants has to keep matching.

I stopped here rather than risk the passing state; the unit is 19/34 and the two remaining
constructors are 248 bytes of the 5548 still unmatched.

### `fn_80070B5C` / `fn_80070B8C` / `fn_80070C20` / `fn_80070C40` (48/148/32/40 B)

The same chain, one class deeper: `fn_80070C20` is the out-of-line
`rstl::construct<TLockedToken<COBBTreeGroup>>`, `fn_80070B5C` forwards to `fn_80070B8C`, and
`fn_80070B8C`/`fn_80070C40` are the `operator=` and construct halves of the token assignment at
+0x28 with its flag byte at +0x28. Byte-shape copies elsewhere, but none of the four exists in our
object under any name, so each needs a construct decided the same way as `fn_8006F554` above -
and `fn_80070B8C` calls 0x80032D0C-ish helpers that are not in the tree yet.

WALL: fn_8006FB58 176 B reached 41/44 instructions with a mirror-struct spelling; the last three
are the flag-store order and the `.sdata2` slot of the 4.0f, which is a data-layout fix, not a
spelling one.

## New

NEW: none filed. The two constructors left are real work on the item's own target, so the item
should simply be requeued; no placeholder target is warranted.
