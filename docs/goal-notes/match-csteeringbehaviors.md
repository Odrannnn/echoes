# match-csteeringbehaviors

Lane 1, worktree `../wt-mp2-goal-L1`. Item: take `MetroidPrime/CSteeringBehaviors` to
`Matching`. One function was left:
`ProjectLinearIntersection(const CVector3f&, float, const CVector3f&, const CVector3f&,
const CVector3f&, CVector3f&)` - retail 0x800F9D04, 464 bytes, 1.21%.

## Result: 12/13 -> **13/13** functions matched, 100.00% fuzzy; the unit still cannot flip

`./tools/goal_check.sh build/goal/item.json` -> **PARTIAL** (exit 3): every check green except
`flip_test.sh`, and `target rose: main/MetroidPrime/CSteeringBehaviors: 12 -> 13 / 13 functions`,
`no asm added`. `matched 12296 -> 12297`, `linked 5863 -> 5863` (unchanged, as expected: the unit
stays `NonMatching`).

The previous run's `progress-unit-csteeringbehaviors` note (docs/goal-notes/, 2026-10-02) concluded
this function could not be written because the solver it needs, `fn_802CB918`, is unwritten. **That
reason was wrong.** The solver does not have to be written to *call* it: retail's DOL link already
contains a definition, in `build/G2ME01/obj/Kyoto/Math/RMathUtils.o`
(`fn_802CB918` is a global `T` there at object offset 0x5e8). Declaring it `extern "C"` and calling
it - the repo's existing pattern for a retail function owned by another unit, cf.
`src/MetroidPrime/CActorField25.cpp:49` - is enough, and the function then matches.

## What I changed

`src/MetroidPrime/CSteeringBehaviors.cpp` only, plus the two derived doc rows below.

- `extern "C" int fn_802CB918(const float* coefficients, float* roots);` (file top, after the
  `FLT_MAX` block), with the note that it lives in RMathUtils.cpp's claim.
- The `return false;` stub replaced by retail's body: five quartic coefficients on the stack, the
  solver call, then a loop over the roots that keeps the **last** positive one (there is no `break` -
  retail falls through `addi r4,r4,4` / `bdnz` after the stores, so a later positive root
  overwrites `intersection`; `found` just stays 1).

Retail's arithmetic, read off the disassembly and confirmed by the 100% match:

| stack | value | retail instruction |
|---|---|---|
| `sp+0x18` | `delta.MagSquared()` | `stfs f31,24(r1)` after `fmadds f31,f30,f30,f3` |
| `sp+0x1C` | `CVector3f::Dot(delta, velocity) * 2.f` | `lfs f13,-27184(r2)` = 2.0f at 0x8041B990 |
| `sp+0x20` | `velocity.MagSquared() + Dot(delta, acceleration) - speed * speed` | `fnmsubs f1,f1,f1,f2` |
| `sp+0x24` | `Dot(velocity, acceleration)` | `fmadds f3,f12,f8,f0` |
| `sp+0x28` | `acceleration.MagSquared() * 0.25f` | `lfs f4,-27180(r2)` = 0.25f at 0x8041B994 |
| `sp+0x8`  | `roots[4]`, `r4 = sp+0x8`, `r3 = sp+0x18` | `addi r4,r1,8`, `addi r3,r1,24` |

`r3 = sp+0x18` and `r4 = sp+0x8` pin the **ascending** coefficient order: `fn_802CB918` reads
`coefficients[4]` (`lfs f9,16(r3)`) and, when it is zero, copies `coefficients[0..3]` out and drops
to `fn_802CBCA0` - so it is a degree-4 polynomial solver with a cubic special case, and it returns
the number of real roots. The loop guard is `mtctr r3` / `cmplwi r3,0` / `ble`, and the body is
`intersection = position + velocity * t + 0.5f * t * t * acceleration` (`lfs f1,-27176(r2)` = 0.5f
at 0x8041B998, then `fmuls f4,f1,f8` / `fmuls f9,f4,f8` = `(0.5f*t)*t`), tested against
`lbl_8041B980` = 0.0f.

Three spellings mattered and are worth recording, because the first two each scored ~71%:

1. **`float coefficients[5] = { ... }` does not work** (98.97% -> measured **71.38%**). With or
   without `const`, mwcceppc 2.7 hoists the aggregate initializer into a 0x14-byte `.rodata`
   template and copies it (`lis r4,0` / `addi r8,r4,0` / five `lwz` + `stw`), instead of computing
   the five values. Five separate `coefficients[i] = ...` assignments are what retail's object does.
   `rstl::reserved_vector<float, 5>` + `push_back` is much worse (**38.64%**): `mCount` is not dead,
   so every push becomes `slwi` / `lwz 28(r1)` / `addi r3,1` / `stw` / `stfsx`, and the frame drops
   to 96 bytes.
2. **`const bool result = found; return result;` is required** for the tail. `return found;` gives
   `mr r3,r31`; retail has `clrlwi r3,r31,24`. With the donor's
   `const bool result = found; return result;` (Prime 1, prime-ref lines 328-330) the two match.
3. **The loop counter and bound must be `uint`** (retail `typedef unsigned int`). With `int`,
   mwcceppc emits `cmpwi r3,0`; retail has `cmplwi r3,0`. `const uint rootCount` + `for (uint i = 0;
   i < rootCount; ++i)` is what closes the last two instructions. 99.48% -> **100.00%**.

The Prime 1 donor at `../prime-ref/src/MetroidPrime/CSteeringBehaviors.cpp:301-330` is the logic
source for all of this; only the coefficient container and the loop bound differ from MP2's shape.

## What still stops the flip (the reason this item comes back)

The unit's object emits `.sdata` (40 B) and `.sdata2` (56 B) that **splits.txt does not claim for
it**, so the flip inserts 56 new bytes into the DOL's `.sdata2` in the wrong place.

`./tools/unit_fit.sh MetroidPrime/CSteeringBehaviors.cpp`:

    .text      claimed   5780   ours   5780   retail   5780   fits
    .sdata     claimed      -   ours     40   <- NOT CLAIMED BY splits.txt; the bytes live in a neighbour
    .sdata2    claimed      -   ours     56   <- NOT CLAIMED BY splits.txt; the bytes live in a neighbour
    no extra functions: our object defines only what the retail unit object does

`config/G2ME01/splits.txt:532` claims only `.text start:0x800F90C0 end:0x800FA754`. Measured by
flipping the unit by hand, rebuilding and diffing `main.dol` against the good build:

- `.sdata2` grows 0x54c0 -> 0x5500 (+0x40 = 64 after alignment), recorded in the DOL header at
  `0xCA`; the file grows 3969024 -> 3969088.
- Our 56 bytes land at **0x8041B940**, which is where `auto_11_8041B940_sdata2.o`'s data starts.
  Every later `.sdata2` address moves by 64, so ~12.5k bytes of the DOL differ (mostly relocated
  address halves) and `sha1sum build/G2ME01/main.dol` = `3ac596da9464eb7177a3c6f309dae04a0e61c9a7`
  instead of `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`; `flip_test.sh` then reports all 86 RELs
  differing as well.

Where retail actually keeps this unit's float constants, found by searching all 986
`build/G2ME01/src/**.o` and 759 `build/G2ME01/obj/**.o` for the first four words:

    00000000 7f7fffff 3727c5ac 40800000

Exactly one object has it: `build/G2ME01/obj/auto_11_8041B940_sdata2.o`, at its offset 0x40 =
**0x8041B980..0x8041B9B8, 56 bytes** - dtk's auto-split placeholder for the unclaimed `.sdata2`
gap (no `splits.txt` entry names 0x8041B980..0x8041B9A0). The 56 bytes are the right multiset and
the right size, but **not the right order**:

    retail  0.0f FLT_MAX 1e-5f 4.0f 2.0f 0.25f 0.5f M_PIF (double) 1.0f FLT_EPSILON 0.2f 0.0f
    ours    0.0f FLT_MAX 1e-5f 4.0f 2.0f 0.25f 0.5f 1.0f FLT_EPSILON 0.0f (double) M_PIF 0.2f

(mwcceppc emits a duplicated 0.0f in ours and one unreferenced word at `.sdata2+0x24`.) So adding
`.sdata2 start:0x8041B980 end:0x8041B9B8` to this unit's `splits.txt` claim would fix *ownership*
but not the bytes, and mwldeppc placed our contribution at 0x8041B940 rather than at the claimed
address in the no-claim build. **I did not try the `splits.txt` variant** - it is a config change
with an uncertain payoff, and the order difference above says the flip needs more than that.

This is the same shape as `match-ccharlayoutinfo`: an object-layout wall no C++ spelling reaches,
so the correct outcome is 13/13 matched with the unit left `NonMatching`.

## Also updated (both derived, both gate-enforced)

The port link has to name `fn_802CB918` now that our object calls it, and nothing of ours defines
it - its sibling `fn_802CB608` has been on the list since 2026-10-01 for the same reason.

- `docs/research/port_link_gap_list.md` - regenerated with `python3 tools/link_gap.py --write-list`;
  the diff is one line, `- fn_802CB918` after `fn_802CB608`. `python3 tools/link_gap.py` reports
  `283 MISSING symbol(s), all accounted for`, measured over 745 objects.
- `docs/research/port_link_gap.md` - the derived table's `unmangled: fn_/lbl_/globals` row 55 -> 56
  (rows then sum to 283), plus a dated 2026-10-02 entry in the existing style.

Without these, `check_docs_claims.py` fails and the gate fails with it (`stale: the gap table says
unmangled: fn_/lbl_/globals is 55, the generated list has 56`).

## Verified

- `./tools/goal_check.sh build/goal/item.json` -> **PARTIAL**, all checks green but the flip
  (`gate.sh` incl. DOL sha1, 86 RELs, report diff, module wiring, docs claims, port probe;
  `check_symbol_names.py`; `decomp_build.sh`'s `All:`).
- `./tools/decomp_build.sh MetroidPrime/CSteeringBehaviors` ->
  `100.00% fuzzy, 100.00% matched (13 / 13 functions)`, unit `matched_code 5780 / 5780`;
  `All: 34.74% fuzzy, 28.29% matched, 12.90% linked (12297 / 28465 functions)`
  (was 34.73 / 28.28 / 12296).
- `python3 tools/check_decl_order.py --unit MetroidPrime/CSteeringBehaviors` -> `none emits its
  functions out of retail order`.
- `./tools/unit_fit.sh MetroidPrime/CSteeringBehaviors.cpp` -> `.text` fits, no extra functions.
- `./tools/probe_sources.sh` -> `752 files, 0 failed, 0 errors; link: LINKED (288 undefined,
  0 duplicates)`.
- `sha1sum build/G2ME01/main.dol` -> `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010` (unit stays
  `NonMatching`, so the retail object is what links).

No asm, no new symbols, no `tools/`, `build/goal/` or `configure.py`/`splits.txt`/`files.cmake`
edits. `docs/HANDOFF.md`'s state block is rewritten by the build; I reverted it (the judge
regenerates it).

## NEW

None. The remaining `.sdata2`-ownership question is the rest of *this* item, not separable work
that raises a count on its own; `progress-quartic-solver-rmathutils` (filed by
`progress-unit-csteeringbehaviors`) is still the right next item and is no longer on this
function's critical path.