# progress-unit-rmathutils — Kyoto/Math/RMathUtils, 16/39 → 23/39

`kind: progress`. Seven functions added, all at 100%. The unit stays `NonMatching` and
`flip_test.sh` was not run to decide anything.

## Measured

| | before | after |
|---|---|---|
| `main/Kyoto/Math/RMathUtils` matched_functions | 16 / 39 | **23 / 39** |
| unit matched code % | 23.35 | 26.03 |
| tree matched functions | 11684 / 28465 | 11691 / 28465 |
| tree `All:` fuzzy | 33.30% | 33.31% |

`./tools/goal_check.sh build/goal/item.json` → **`goal_check: PASS progress-unit-rmathutils`**,
with the whole gate green: DOL sha1, all 86 RELs, per-function report diff, module wiring,
docs claims, port probe, `check_symbol_names.py`, counts, and no `asm` added. Every new
function is byte-exact against retail modulo relocations (which objdiff ignores), so no
function anywhere got worse and the `linked` count correctly stayed at 5625 — the unit is
`NonMatching`, so none of this is in the binary yet.

## Per function

| function | before | after | notes |
|---|---|---|---|
| `fn_802CC120` (116 B) | 0% | **100%** | byte-exact, 0/29 differing |
| `fn_802CCE1C` (12 B) | 0% | **100%** | byte-exact, 0/3 differing |
| `fn_802CCE28` (36 B) | 0% | **100%** | 3 differing, all `lfs` sda21 relocation fields |
| `Noise1d` (40 B) | 0% | **100%** | 2 differing: one `lfs`, one `bl` — both relocations |
| `Noise2d` (36 B) | 0% | **100%** | same |
| `Noise3d` (32 B) | 0% | **100%** | 1 differing: the `bl` |
| `Noise4d` (32 B) | 0% | **100%** | 1 differing: the `bl` |

### `fn_802CC120` — the whole function is `FloorPowerOfTwo` without the `1 <<`

`CMath::FloorPowerOfTwo` (already in the file, 100%) and `fn_802CC120` are the same
four-stage bit scan; retail's second one returns the shift count instead of `1 << shift`,
so the answer is the position of `v`'s highest set bit. **Copying `FloorPowerOfTwo`'s body
verbatim and changing only the final line to `return ((1 - finalSig) >> 0x1f) + totalShift;`
is byte-exact** — I confirmed `0 differing instructions of 29` before landing it. Do not
try to "improve" the expression; two rearrangements of the return (`s1` added last, and a
named `finalShift` local) each produce a different register allocation and are *not* exact.

One detail that is load-bearing: the parameter is **signed `int`**, not `uint`. Retail
compares it with `cmpwi` and then shifts it as unsigned. `uint` gives `cmplwi` + `sraw`,
`signed int` gives `cmpwi` + `srw`, and only the latter matches. The existing
`CRagDoll.cpp:19` declaration says `extern "C" int fn_802CC120(uint value);` — that
declaration is what the port compiles against and it is left untouched, so this is a
C-vs-C++ linkage mismatch I did **not** fix (see NEW below). It links because the mangled
name has no parameter types, but the port passes it an unsigned value where retail's
object expects a signed one; behaviour only differs for `v >= 0x80000000`.

### `fn_802CCE1C` — linear interpolation

`f0 = f3 - f2; f1 = f1 * f0 + f2`, i.e. `t * (b - a) + a`. One line, byte-exact first try.
This is the same shape as `CVector3f::Lerp` in `include/Kyoto/Math/CVector3f.hpp:52`, but
it is a free function on bare floats, not the vector method.

### `fn_802CCE28` — the quintic fade

`x^3 * (6x^2 - 15x + 10)`, the C2-continuous smootherstep that the Perlin noise weights its
lattice corners with. Retail's three constants are `6`, `15`, `10` in that order in
`.sdata2` (`lbl_8041E724`, `lbl_8041E728`, `lbl_8041E720`). Byte-exact modulo the three
`lfs` relocations. The `x2`/`x3`/`linear` split matters: it is what puts `f4 = x*x` before
the `fmsubs`.

### The four `Noise` wrappers

All four are thin forwarders: `Noise1d(x) = fn_802CCA38(x, 0, 0)`,
`Noise2d(x,y) = fn_802CCA38(x, y, 0)`, `Noise3d = fn_802CCA38(x,y,z)`,
`Noise4d = fn_802CC4E4(x,y,z,w)`. Each matches modulo its `bl` and (for 1d/2d) the `lfs` of
the `0.0f` it passes. `CMath.hpp` already declared all four; only the definitions were
missing.

**They are wrapped in `#ifndef TARGET_PC`.** This is not cosmetic. Without the guard the
port's link picks up two new undefined symbols (`fn_802CCA38`, `fn_802CC4E4`) and the gate
**fails**: `link_check: STRICT FAIL - regression gate: 326 undefined against a baseline of
324 (GREW)`. The only caller is `CREPerlinNoise*` in `src/Kyoto/Particles/CRealElement.cpp`,
and that file is **not in `files.cmake`**, so the port has no user for them. The guard
follows the convention already in this file for `fn_802CDF64` / `fn_802CDF50`. I hit this
failure, measured it, and fixed it that way — anyone re-adding these must keep the guard or
expect the port's undefined count to grow.

## Not done, and why

### `fn_802CCDA4` (120 B) and `fn_802CCD28` (124 B) — the Perlin gradient pickers

These are the two next-smallest unmatched functions and both are pure leaf code, so they
looked cheap. They are not. ~14 spellings each, none reached 100% (best: 17 of 30
instructions differing for `fn_802CCDA4`).

What retail does, from the object at `0x802CCDA4`:

1. `clrlwi r3, r3, 28` — mask to 0..15, **in place**.
2. Three `cmpwi`/`bge` selects against that same r3.
3. **Then** three sign tests on bits 30, 29 and 31 of **that same masked r3**.

Step 3 is dead code — the value is 0..15, so bits 29-31 are always zero and retail's own
compiler failed to notice, keeping the tests. Reproducing it requires the sign tests to read
a value MWCC will not range-prove is small, *and* the sign bits to still land in the same
register as the mask. I tried: ternary vs `if/else` vs `switch`; the mask as a new `const`,
in-place on the parameter, and via a `static inline` helper; sign bits read from the mask, a
pre-mask copy, and a separate variable; `int` vs `uint`; `1u << 30` vs the hex literal;
`&`-tests vs `>=`/`==` compares; and reordering the sign tests before the selects. The
closest variants (g2p, g2r at 17/22 differing) collapse to 88-96 bytes where retail is 120 —
MWCC folds the dead tests away and the whole tail changes shape, so the remaining gap is not
register allocation but whether the dead code is emitted at all. That needs a source form I
did not find, not another spelling of this one.

WALL: fn_802CCDA4 0% - needs retail's unfolded dead sign-bits on the 0..15 mask in the same register; 14 source shapes fold them away, none reproduced 120 bytes
WALL: fn_802CCD28 0% - same dead-sign-bit problem as fn_802CCDA4 plus a 3-way gradient select; no spelling reproduced 124 bytes

### `fn_802CDF64` — 80%, and the 20% is a register-allocation wall

Pre-existing at 80%, untouched. The only difference is that retail stores the `SqrtD` result
to `lbl_80419A58` **after** the `lwz r0, 0x14(r1)` reload, and ours stores before it. The
assignment target and the call are both right; only the interleaving of the store with the
frame-pointer teardown differs. That is a scheduler decision, and the source spelling that
would flip it was not found. It was already 0% counted (not a matched function), so it does
not gate the item.

### Still unmatched after this run (12 of 39)

`fn_802CCA38` (752 B) and `fn_802CC4E4` (1364 B) are the Perlin body and its 4d form — the
two biggest functions here and the ones the four wrappers call, so they are the natural next
item. Then `fn_802CCF88` (344 B), `fn_802CDD54` (324 B), `fn_802CD988` (316 B),
`fn_802CCE4C` (316 B), `fn_802CD2E4` (720 B), `fn_802CD5B4` (808 B), `fn_802CBCA0` (964 B),
`fn_802CB918` (904 B), `fn_802CB608` (784 B), `fn_802CD0E0` (260 B), `fn_802CDC50` (260 B).

`fn_802CB608` and `fn_802CC064` are already in `docs/research/port_link_gap_list.md` as
port-undefined, so some of this unit is on the port's critical path too.

## The Prime 1 donor

`/run/media/odran/Leo/projects/Restored-projects/Chatgpt/prime-ref/src/Kyoto/Math/RMathUtils.cpp`
is **not usable as a donor for this item**. It has 157 lines and stops at `FloorPowerOfTwo`:
it contains no `Noise*`, no `EaseInOut`, no `PhongBlob`, no `SolveQuadratic` and none of the
`fn_802CC*` helpers. Grepping it for `Noise` returns only unrelated hits in `NESemu/`. Every
function in this note was read out of the retail object in
`build/G2ME01/asm/Kyoto/Math/RMathUtils.s`. The Echoes engine forked from Prime 1's, but this
unit diverged well past the shared part — the item's premise that Prime 1 is a donor here
does not hold.

## Verification

- `./tools/goal_check.sh build/goal/item.json` → `PASS` (run after every landing; the
  `TARGET_PC` guard was added in response to its one failure).
- `python3 tools/check_decl_order.py --unit RMathUtils` → `ok: 1 unit(s) checked, none emits
  its functions out of retail order`. The new functions are declared **descending** by
  retail offset: `fn_802CC120` (0x802CC120) after `FloorPowerOfTwo` (0x802CC194);
  `fn_802CCE28` (0x802CCE28) and `fn_802CCE1C` (0x802CCE1C) after `GetBezierPoint`
  (0x802CD1E4); the four `Noise*` (0x802CC458-0x802CC4BC) before `BaryToWorld` (0x802CC3DC).
- `tools/unit_fit.sh Kyoto/Math/RMathUtils.cpp` → `no extra functions: our object defines
  only what the retail unit object does`. `.text` is 8360 bytes short, so the unit is not
  close to flipping.
- `docs/HANDOFF.md` was modified by the **judge's own** `check_docs_claims --write`
  (`MP_GATE_DOCS_WRITE=1` in `goal_check.sh`), not by me; I reverted it so this diff is the
  source file alone. The driver rewrites those counts from the tree anyway.

## NEW

NEW: progress-unit-rmathutils-noise-body | progress | Kyoto/Math/RMathUtils | fn_802CCA38 (752 B) and fn_802CC4E4 (1364 B) are the Perlin body and its 4d form; the four CMath::Noise wrappers added here now forward to them, so they are the next target and the only remaining callers of an undefined symbol this unit introduces
NEW: port-item-fn-802cc120-signature | port | fn_802CC120 | src/MetroidPrime/CRagDoll.cpp:19 declares it extern "C" int fn_802CC120(uint) but the retail object takes a signed int (cmpwi then srw); the parameter type must be reconciled before the port's call can be trusted for v >= 0x80000000
