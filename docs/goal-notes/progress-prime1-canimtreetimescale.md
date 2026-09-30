# progress-prime1-canimtreetimescale

`kind: progress`, target `Kyoto/Animation/CAnimTreeTimeScale`. The unit stays `NonMatching`;
`flip_test.sh` was run **as a diagnostic only** to characterise what is left, and it was reverted
(§ "What stops the flip"). Prime 1's `src/Kyoto/Animation/CAnimTreeTimeScale.cpp` was adapted to
this repo's headers and member names, changing only what the measured diff showed.

## Result

`main/Kyoto/Animation/CAnimTreeTimeScale`: **6/19 -> 18/19** functions at 100%,
matched code 7.68% -> 99.01%, fuzzy 26.44% -> 99.68%. `.data` stayed 100% (120/120 bytes).
`All:` 30.61% fuzzy / 22.74% matched / 11.74% linked (9939) -> 30.68% / 22.82% / 11.74% (9951).

Per function, from `build/report.json` against the judge's baseline
`build/goal/judge/report.base.json` (recorded on `4def401`):

| function | before | after | Prime 1's source |
| --- | --- | --- | --- |
| `VAdvanceView` | 3.81% | **100%** | unchanged but for `SAdvancementDeltas`/`SAdvancementResults` naming; one measured edit (§ `SAdvancementDeltas()`) |
| `VGetTimeRemaining` | 23.86% | **100%** | unchanged |
| `VGetSteadyStateAnimInfo` | 11.39% | **100%** | unchanged |
| `VClone` | 95.00% | **100%** | one measured edit - the `clone_reader` wrapper (§ below) |
| `VGetBestUnblendedChild` | 96.60% | **100%** | same wrapper; Prime 1's `if (child) ... return child;` spelling kept as is |
| `VGetContributionOfHighestInfluence` | 60.42% | **100%** | unchanged: the four separate locals (`weight`, `name`, `info`, `time`) are what retail's evaluation order needs |
| `VGetBoolPOIList` | 1.79% | **100%** | unchanged, including `listOut[iterator + i]` |
| `VGetInt32POIList` | 1.79% | **100%** | unchanged, including `listOut[i + iterator]` (retail really does differ from the Bool spelling here) |
| `VGetParticlePOIList` | 1.79% | **100%** | unchanged |
| `VGetSoundPOIList` | 1.79% | **100%** | unchanged |
| `VSimplified` | 1.56% | **100%** | unchanged, with the `simplify_reader` and `clone_reader` wrappers |
| `GetRealLifeTime` | 3.71% | **100%** | unchanged; `rstl::min_val` on two `float`s is fine here - no in-place helper was needed |
| `fn_802A8800` | n/a (0%) | 67.14% | not in Prime 1 by that name; see below |

**All 12 functions named in `item.json` are at 100%, unchanged from Prime 1 apart from the
renames.** Nothing anywhere got worse: `tools/report_diff.py build/goal/judge/report.base.json
build/report.json` prints `+12 functions at 100%` and `no regression`. The diff adds no `asm`.
Two files touched: `src/Kyoto/Animation/CAnimTreeTimeScale.cpp`,
`include/Kyoto/Animation/CTimeScaleFunctions.hpp`.

## The three decisions, and the measurements behind them

### `TimeScaleIntegral` / `FindUpperLimit` added to `IVaryingAnimationTimeScale`

Prime 1's `CTimeScaleFunctions.hpp` has two inline wrappers this one lacks:

```cpp
CCharAnimTime TimeScaleIntegral(const CCharAnimTime& lower, const CCharAnimTime& upper) const;
CCharAnimTime FindUpperLimit(const CCharAnimTime& lower, const CCharAnimTime& root) const;
```

They are non-virtual and add no data, so the vtable and `CHECK_SIZEOF(..., 0x4)` are unchanged;
`include/Kyoto/Animation/CTimeScaleFunctions.hpp` is included by nothing but
`CAnimTreeTimeScale.hpp`, so only this unit is affected. Retail's code passes *addresses of stack
temporaries* holding each `GetSeconds()` to the virtual (see `build/G2ME01/asm/.../CAnimTreeTimeScale.s`
at `0x802A5050`), which is what binding a const-ref to a by-value `GetSeconds()` produces. Writing
`CCharAnimTime(mTimeScale->VTimeScaleIntegral(a.GetSeconds(), b.GetSeconds()))` inline in the
`.cpp` instead would be the same code; the header form is Prime 1's and reads better.

**No new undefined symbol appears in the port.** These are *virtual* calls through
`object_owner<IVaryingAnimationTimeScale>`, so nothing is referenced by name. Measured: the port's
`link_gap.py` stays at 246 MISSING and `link_check.sh` at 250 undefined, both unchanged.

### `SAdvancementDeltas()` does not initialise here - name the two members

`VAdvanceView`'s else branch is Prime 1's `CAdvancementResults res(CCharAnimTime(0.f),
CAdvancementDeltas());`. With that spelling the function reaches **92.50%**, and the diff starts at
the seventh float of the deltas: retail loads `sZeroVector`/`sNoRotation` and stores seven floats
in place, ours emits `bl` to an out-of-line empty constructor. The cause is a header difference,
not a Prime 1 spelling: Prime 1's `CAdvancementDeltas()` is
`CAdvancementDeltas() : mPosDelta(CVector3f::Zero()), mRotDelta(CQuaternion::NoRotation()) {}`,
while this repo's `SAdvancementDeltas() {}` initialises nothing, because `CVector3f()` and
`CQuaternion()` are both `{}`.

Fixed locally rather than in the shared header, because every unit that default-constructs
`SAdvancementDeltas` would move:

```cpp
SAdvancementResults res(CCharAnimTime(0.f),
                        SAdvancementDeltas(CVector3f::Zero(), CQuaternion::NoRotation()));
```

**92.50% -> 100%.** Fixing `SAdvancementDeltas()` in `include/Kyoto/Animation/IAnimReader.hpp` is
the real defect and is out of scope for a `progress` item on this unit; it needs a lane that can
re-measure every unit that constructs one.

### `clone_reader` / `simplify_reader`, and `fn_802A8800`

Retail calls weak out-of-line `IAnimReader::Clone` and `::Simplified` instead of inlining the
vtable dispatch. Here both are inline one-liners in `IAnimReader.hpp` with no definition anywhere
in the tree, so naming them directly would be an undefined reference. Two file-static forwarders
reproduce the calls. Measured:

| spelling | unit | `VClone` | `VGetBestUnblendedChild` | `VSimplified` |
| --- | --- | --- | --- | --- |
| `mChild->VClone()` / `mChild->VSimplified()` | 15/19 | 95.00% | 96.60% | 96.17% |
| the two wrappers | **18/19** | 100% | 100% | 100% |

They are `static`, unlike the same pair in `CAnimTreeTransition.cpp`: mwcceppc still emits them out
of line (measured identical, 18/19) and the symbol becomes local `t` instead of global `T`, which
keeps two global `clone_reader` definitions out of the link once both units flip.

`fn_802A8800` is the third thing that needed a decision, and `flip_test.sh` is what found it.
Retail has no name for a weak out-of-line `IVaryingAnimationTimeScale::Clone` at `0x802A8800`
(a vtable dispatch to slot `0x14`, 56 bytes), and **dtk's auto unit `auto_03_802B2090_text` *calls*
it**, from `fn_802B2484`. `mTimeScale->Clone()` already makes mwcceppc emit those exact bytes, but
under the mangled name `Clone__26IVaryingAnimationTimeScaleCFv`, so objdiff has nothing to pair
(0%) *and* the auto unit's reference does not resolve. Flipping the unit fails:

```
FAILED: [code=1] build/G2ME01/main.elf
### mwldeppc.exe Linker Error:
#   undefined: 'fn_802A8800'
```

So it is defined as well, `extern "C"` at its own retail offset (between `VGetParticlePOIState`
and `VSimplified` in the file, which is what the reverse-source-order rule requires):

| spelling | unit | fuzzy | link with our object |
| --- | --- | --- | --- |
| `mTimeScale->Clone()` only, `fn_802A8800` undefined | 18/19 | 99.01% | **`undefined: 'fn_802A8800'`** |
| callers call `fn_802A8800(mTimeScale)` directly | 15/19 | 99.25% | links |
| **callers keep `mTimeScale->Clone()`, `fn_802A8800` also defined** | **18/19** | **99.68%** | links |

The middle row is the trap: making the three callers use the named symbol is what costs each of
them 2-3%. The body is retail's, not a stub, and the `extern "C"` trick is the one the `fn_` carve
units under `src/Kyoto/Animation` already use.

## What stops the flip

`tools/flip_test.sh Kyoto/Animation/CAnimTreeTimeScale.cpp`, with the above in place: the link now
completes, and the failure moved to the hashes.

```
    build failed:
      FAILED: [code=1] build/G2ME01/ok
      build/G2ME01/main.dol: FAILED
      ... all 86 RELs FAILED
  FAIL  -> reverted
```

All 19 functions objdiff can pair are at 100% (`complete_code_percent: 100.0`), so this is a
section-layout difference, not a code one. `tools/compare_unit.sh` on the flipped build shows what
our object carries that the retail-derived one does not:

- `.sdata` **36 bytes** - the five `CCharAnimTime` constants (`ZeroFlat`/`Infinity` and friends).
  MWCC materialises them in every TU that includes `CCharAnimTime.hpp`, and mwldeppc keeps the
  unreferenced words; retail loads them from a shared `.sdata` (`lbl_80418A88`/`lbl_80418A8C`).
  The header's own comment predicts exactly this. Defining `CCHARANIMTIME_LOCAL_CONSTANTS` for
  this TU was measured: it drops `.sdata` to 0 but grows `.sdata2` from 4 to 20 bytes and changes
  nothing else, so it is not the fix and was not kept.
- `.rodata` 8 bytes and `.sbss` 1 - the `rstl::string_l("")` literal `CreatePrimitiveName` returns
  and one flag byte. Retail keeps the string in a shared pool at `lbl_803AEDF0`.
- `.text` is 1896 bytes larger than retail's 5676, but that is 21 COMDAT weak copies (inline
  virtual destructors, `rstl` template destructors) that mwldeppc is expected to discard; the
  same set the CAnimTreeTransition lane measured. `tools/unit_fit.sh` lists them.

**No `NEW:` filed for the flip.** Two independent things are in the way - the per-TU `CCharAnimTime`
constant pool and a `match` item on the section layout - and neither has a spelling that raises a
count on its own, so a lane would spend an hour to re-derive what is written above.

## Gates

All run at the end, on this tree, after the final edit:

- `sha1sum build/G2ME01/main.dol` -> `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010` (expected)
- all 86 REL sha1s in `config/G2ME01/config.yml` match the disc
- `./tools/decomp_build.sh` -> `All: 30.68% fuzzy, 22.82% matched, 11.74% linked (9951 / 28465
  functions)`, no FAILED
- `python3 tools/check_symbol_names.py` -> `checked 503 units; 0 declared names are missing from
  their object`
- `python3 tools/report_diff.py build/goal/judge/report.base.json build/report.json` ->
  `matched 9939 -> 9951   linked 4896 -> 4896   (+12 functions at 100%, 0 units newly linked)`,
  `no regression`
- `python3 tools/check_decl_order.py --unit Kyoto/Animation/CAnimTreeTimeScale` -> clean
- `./tools/probe_sources.sh` -> `749 files, 0 failed, 0 errors; link: LINKED (250 undefined,
  0 duplicates)`
- `python3 tools/link_gap.py --rebuild` -> `246 MISSING symbol(s), all accounted for` (unchanged)
- `./tools/link_check.sh` -> `unique undefined symbols 250`, `duplicate definitions 0` (both
  unchanged from the recorded baseline)
- `python3 tools/check_docs_claims.py` flags only the two derived counts in the `docs/HANDOFF.md`
  state block (`9951 / 28465` and the DOL sub-count), which the judge rewrites from the tree.
- `config/G2ME01/splits.txt` untouched; `report.json` `total_functions` still **28465**

## Codegen rules this unit paid for

- Retail calls `IAnimReader::Clone` and `::Simplified` out of line; this tree's inline wrappers
  have to be reached through a file-static forwarder to reproduce the `bl`. Worth 3 functions.
- Retail's `SAdvancementResults` default is `CCharAnimTime::ZeroFlat()` **with** the deltas
  (`CVector3f::Zero()`, `CQuaternion::NoRotation()`); this repo's `SAdvancementDeltas()` does not
  initialise, so the members must be named in any unit that default-constructs one.
- A `CCharAnimTime` static constant used by name from a `.sdata` pool is not free: MWCC puts those
  36 bytes in *every* TU that includes the header and mwldeppc keeps them, so a unit whose
  `.sdata` split is smaller than 36 bytes cannot flip. The opt-in
  `CCHARANIMTIME_LOCAL_CONSTANTS` trades them for `.sdata2` bytes, not for nothing.
