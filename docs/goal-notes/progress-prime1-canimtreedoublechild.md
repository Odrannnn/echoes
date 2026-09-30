# progress-prime1-canimtreedoublechild

`kind: progress`, target `Kyoto/Animation/CAnimTreeDoubleChild`. The unit stays `NonMatching`;
`tools/flip_test.sh` was **not** run (the item brief says not to run it to decide). Adapted Prime 1's
`src/Kyoto/Animation/CAnimTreeDoubleChild.cpp` to this repo's own headers and member names, changing
only what the measured diff showed. Two files touched, no `asm`:
`src/Kyoto/Animation/CAnimTreeDoubleChild.cpp`, `include/Kyoto/Animation/IAnimReader.hpp`.

## Result

`main/Kyoto/Animation/CAnimTreeDoubleChild`: **10/18 -> 18/18** functions at 100%,
matched code 27.32% -> **100%**, fuzzy 55.15% -> **100%**, matched data already 100%.
`All:` 30.43% fuzzy / 22.35% matched / 11.74% linked (9874) -> **30.46% / 22.40% / 11.74% (9882)**.

`build/report.json`, per function, before -> after (every name in `item.json`'s list matched a Prime 1
name; all eight are now at 100%):

| function | before | after | Prime 1's source |
| --- | --- | --- | --- |
| `VGetBoolPOIList` | 96.39% | **100%** | small edit - `int x` + `if (x > capacity) x = capacity;` instead of `uint count` + `rstl::min_val` |
| `VGetInt32POIList` | 96.39% | **100%** | same edit |
| `VGetParticlePOIList` | 96.39% | **100%** | same edit |
| `VGetSoundPOIList` | 96.39% | **100%** | same edit |
| `VGetContributionOfHighestInfluence` | 51.19% | **100%** | unchanged apart from naming - needs the two local weights and the explicit `CAnimTreeEffectiveContribution(...)` rebuild of the winner |
| `VGetNumChildren` | 64.28% | **100%** | unchanged apart from naming - needs `int` accumulator, `mB` first |
| `AdvanceViewBothChildren` | 5.05% | **100%** | unchanged apart from naming - body was a TODO stub here, restored whole |
| `VGetBestUnblendedChild` | 84.79% | **100%** | small edit - needs `if (!best) return child; return best;`, not `best ? best : child` |

The ten already-matched functions did not move.

No function anywhere got worse: `python3 tools/report_diff.py build/report.base.json build/report.json`
prints `no regression` and lists only the eight `+100%` lines. The `All:` line rose, it did not fall.

## What was verified

```sh
sha1sum build/G2ME01/main.dol    # 6ef9b491d0cc08bc81a124fdedb8bfaec34d0010  (as pinned)
./tools/probe_sources.sh         # 749 files, 0 failed, 0 errors; link LINKED (250 undefined, 0 duplicates)
python3 tools/check_symbol_names.py  # checked 503 units; 0 declared names are missing
./tools/decomp_build.sh          # All: 30.46% fuzzy, 22.40% matched, 11.74% linked (9882 / 28465)
python3 tools/check_decl_order.py    # ok: 957 unit(s) checked, 31 permuted, all accounted for
python3 tools/check_files_cmake.py   # every configured DOL object is either in files.cmake or excluded
```

The 86 REL sha1s re-hashed against `config/G2ME01/config.yml` independently (the `gate.sh` step 3
snippet, run by hand): `ok`. `tools/check_docs_claims.py` reports the two HANDOFF state-block counts
now stale, which is expected for a `progress` item - the judge rewrites the derived counts from the
tree and discards edits to the docs.

`./tools/unit_fit.sh Kyoto/Animation/CAnimTreeDoubleChild.cpp`: our object is 1236 bytes over the
claimed `.text` range and emits 13 functions the retail unit object does not define
(`__ct__/__dt__30CAnimTreeEffectiveContribution`, `Depth__20CAnimTreeDoubleChildCFv`,
`ReleaseData__Q24rstl23rc_ptr<13CAnimTreeNode>`, the `rstl` destructors, ...). These are COMDAT weak
copies and inline virtuals the retail linker drops; per the script's own text that is not a verdict.
`flip_test.sh` decides, and the item brief says not to run it, so the flip question is open.

## The one non-obvious thing: how `AdvanceViewBothChildren` reached 100%

Prime 1's source compiled here to **96.32%** - 422 retail instructions against 408 ours, with the
whole structure aligned. The mnemonic-level diff (`difflib` over `objdump -d` of the retail range
0x802a6ed8+0x698 against our object) showed exactly three blocks:

1. the FPR save/restore prologue, one `psq_st` pair out of order (register pressure);
2. **seven `stfs` that retail has and we did not**, at 0x802a7170, right after
   `leftOffset += ...` and before the `CQuaternion` copy for `operator*`;
3. the same seven again in the right-child loop, at 0x802a73ec.

Those seven stores are retail materialising a *copy* of the deltas struct into a stack slot, and then
reading the fields back out of the copy. `SAdvancementDeltas deltas = result.mDeltas;` followed by
`deltas.mPosDelta` / `deltas.mRotDelta` lets mwceppc forward the copy away; reading through
`deltas.GetOffsetDelta()` / `deltas.GetOrientationDelta()` - the two const-reference accessors Prime 1
has on `CAdvancementDeltas` and this repo's `SAdvancementDeltas` did not - makes it keep the local.
That is the only change to a shared header in this diff: **two inline const-reference accessors added
to `SAdvancementDeltas` in `include/Kyoto/Animation/IAnimReader.hpp`**, mirroring Prime 1's names.
They add no symbol, no layout change, no `.text` movement; `report_diff.py` confirms nothing else moved.

A dead end worth recording so the next run does not retry it: adding
`const SAdvancementDeltas& GetAdvancementDeltas() const` to `SAdvancementResults` and writing
`result.GetAdvancementDeltas()` instead of `result.mDeltas` is **worse** (96.22%, not 96.32%) - the
accessor on the *results* is not what keeps the local alive. The accessors have to be on the deltas
struct itself.

## Notes for the next attempt

- The other three files that call `AdvanceViewBothChildren` (`CAnimTreeTransition.cpp`,
  `CAnimTreeSequence.cpp`, `CMetaAnimPhaseBlend.cpp` and friends) were blocked on this stub and
  should now move; that is a separate item, not filed as `NEW:` here.
- Prime 1's `CAnimTreeDoubleChild` constructor and destructor also call
  `CCharAnimMemoryMetrics::AddToTotalSize` / `SubtractFromTotalSize`. Those two are already at 100%
  in this unit **without** the calls, so retail's Echoes build does not have them - do not port them.
