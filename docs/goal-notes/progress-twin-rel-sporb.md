# progress-twin-rel-sporb - Sporb (module 76) destructor run, +7 matched functions

`kind: progress`, `target: module:Sporb`. Worktree `../wt-mp2-goal-L2` on `goal/lane-2`.

## Result (measured, not recalled)

| | before | after |
| --- | --- | --- |
| `build/report.json` `measures.matched_functions` | 13345 | **13352** (+7) |
| `Sporb/*` matched functions summed (the judge's `module:` rule) | 19 | **26** (+7) |
| `Sporb/MetroidPrime/ScriptObjects/SporbDtors` | did not exist | **7 / 7, `complete: true`** |
| `report.json` `measures.total_functions` | 28465 | 28465 (unchanged) |
| all 86 REL sha1s vs `config/G2ME01/config.yml` | ok | **ok, 0 bad** |
| `tools/unit_fit.sh MetroidPrime/ScriptObjects/SporbDtors.cpp` | - | `.text claimed 480 ours 480 retail 480 fits`, no extra functions |

`./tools/goal_check.sh build/goal/item.json` - **PASS**, run twice. The first run failed on one gate
step only (`raw-offsets`, because the new file needs its own `##` section in
`docs/research/raw_offsets.md`); after adding that section and re-deriving the total line from the
tool:

```
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 13345 -> 13352   linked 6393 -> 6400
  ok    check_symbol_names.py
  ok    All:  37.40% fuzzy, 30.84% matched, 13.70% linked (13352 / 28465 functions)
  ok    target rose: module:Sporb: 19 -> 26 / 282 functions
  ok    no asm added
goal_check: PASS progress-twin-rel-sporb
```

## What I claimed

One new unit, `MetroidPrime/ScriptObjects/SporbDtors.cpp`, holding
`config/G2ME01/rels/Sporb/splits.txt`'s new range

```
MetroidPrime/ScriptObjects/SporbDtors.cpp:
	.text       start:0x00011B34 end:0x00011D14
```

which is seven adjacent functions and nothing else (`0x1E0` = 480 bytes, and the object's `.text`
is 480 bytes, so the object fills the claim exactly):

| addr | size | what it is | twin | twin's source |
| --- | --- | --- | --- | --- |
| `fn_76_11B34` | 0x7C | `CGameProjectile`'s deleting destructor: `__vt__15CGameProjectile`, then +0x238 / +0x1F8, then `__dt__7CWeaponFv(this, 0)`, then `Free` on flag > 0 | `__dt__15CGameProjectileFv` | `src/MetroidPrime/Weapons/CGameProjectile.cpp` |
| `fn_76_11BB0` | 0x70 | `CCameraShakerData`'s implicit destructor: three `CMayaSpline` members at +0xA0/+0x5C/+0x18 | `__dt__17CCameraShakerDataFv` | `src/MetroidPrime/TypesMatch.cpp` |
| `fn_76_11C20` | 0x58 | `~CMayaSpline`: the member at +0x8 | `__dt__11CMayaSplineFv` | `src/MetroidPrime/Factories/Carve80032774.cpp` |
| `fn_76_11C78` | 0x54 | the leaf: `Free(*(void**)(self + 0xC))`, then itself on flag > 0 | `__dt__Q24rstl36vector<i,...>Fv` | `src/MetroidPrime/main.cpp` |
| `fn_76_11CCC` | 0x1C | three floats at +0x584..+0x58F copied to the hidden return pointer | - (same shape as this module's own `fn_76_370`) | - |
| `fn_76_11CE8` | 0x0C | the flag byte at +0xC cleared | `GetImpactParticle__17CEnergyProjectileFR13CStateManager` | `src/MetroidPrime/TypesMatch.cpp` |
| `fn_76_11CF4` | 0x20 | thunk to the imported `Render__17CEnergyProjectileCFRC13CStateManager` | `__sys_free` (same 32-byte forwarder shape) | `src/MetroidPrime/main.cpp` |

All seven were written as `extern "C"` free functions over `void*` + literal offsets, the
arrangement `CFogOverlayRel.cpp` (`fn_23_0`) and `CFlyerSwarmRelTail.cpp` (`fn_21_192C`,
`fn_21_18D4`) already use at 100%. **The twins' own sources were not needed as text**: the shapes
are deleting destructors, and the three load-bearing details are already recorded in those two
module files - the flag is a `short` (`extsh.`, not `cmpwi`), the return type is a pointer (the
trailing `mr r3,r30`), and the flag handed to a member teardown is the literal `0`/`-1`.
`fn_76_11CF4`'s twin `__sys_free` is **not** the function it is: it only shares the 32-byte
forwarder shape. The real callee is named by the module's own import table
(`build/G2ME01/Sporb/Sporb.preplf`), which is where the class comes from.

The four carve files are all in the change: `config/G2ME01/rels/Sporb/splits.txt`,
`configure.py` (`Object(Matching, "MetroidPrime/ScriptObjects/SporbDtors.cpp")`, one `Object(...)`
per line, inserted between the existing `SporbAccessors` entry and the block's `]`), `files.cmake`,
and the source's own header comment. Definitions are in descending retail address order.

## What did NOT get claimed, and why

The item's longest run is `0x11A50..0x11CCC` (six destructors). I wrote all six; **five are exact
and `fn_76_11AB0` is not**, so I stopped the claim at `0x11B34` and left `0x11A50..0x11B34` as
retail bytes rather than claim a range my object does not reproduce. See the `WALL:` line.

`fn_76_11A50` (0x60, the module class's own deleting destructor) measured **100.00%** in both
spellings tried - it is `CFogOverlayRel.cpp`'s `fn_23_0` shape exactly - so it is waiting only on
its neighbour.

## Two build facts worth keeping (both cost me time, both are general)

1. **A `NonMatching` REL unit makes the module's sha1 check vacuous.** I had the unit
   `NonMatching` while iterating, and the sha1 held with an object that was 32 bytes short for the
   claim. The reason: `dtk dol split` does **not** link a `linked False` object's bytes - it writes
   a *reference* object into `build/G2ME01/<Module>/obj/<unit>.o`, recognisable by a
   `.note.split` section and a `.text` already sized to the claimed range. So a `NonMatching` claim
   is retail bytes wearing our unit's name, and only objdiff's per-function numbers see the
   difference. **A REL claim has to be `Matching` before its hash check means anything.**
2. **The module's copy of our object goes stale silently.** `build.ninja` has a single `split` edge
   (`build/G2ME01/config.json`), so ninja re-runs it only when `config/G2ME01/config.yml` changes.
   Editing `src/MetroidPrime/ScriptObjects/SporbDtors.cpp` recompiles
   `build/G2ME01/src/.../SporbDtors.o` but leaves
   `build/G2ME01/Sporb/obj/MetroidPrime/ScriptObjects/SporbDtors.o` (and therefore the linked
   `.rel`) at the previous contents - `cmp` between the two is the check. `touch`ing
   `config/G2ME01/config.yml` was **not** enough; `rm build/G2ME01/config.json` and rebuild is
   what forced it. objdiff reads `build/G2ME01/src/...o`, the link reads the module copy, so the two
   can disagree and the report still looks current.

## Gates

- `python3 tools/check_symbol_names.py` - `checked 585 units; 0 declared names are missing`
- `python3 tools/check_files_cmake.py` - `every configured DOL object is either in files.cmake or
  excluded with a reason`; my file is listed, and its host branch is `#ifdef __MWERKS__`, so the
  flat port link gains nothing (the arrangement `CFlyerSwarmRelTail.cpp` uses, and the reason
  `fn_76_5334`, `__dt__17CProjectileWeaponFv`, `__dt__7CWeaponFv` and
  `Render__17CEnergyProjectileCFRC13CStateManager` cannot be listed as host symbols).
- `python3 tools/check_decl_order.py` - `ok: 1132 unit(s) checked, 37 permuted, all 37 accounted
  for`; it does not resolve a path out of a `Rel(...)` block, so the descending order in this unit
  is verified by the module's sha1, as `CFlyerSwarmRelTail.cpp` records.
- `python3 tools/check_raw_offsets.py` - `ok: 192 raw-offset site(s) in 81 file(s), all documented
  in raw_offsets.md`. **This file's change includes `docs/research/raw_offsets.md`**: six sites
  (`+0x584`, `+0xA0`, `+0x5C`, `+0x18`, `+0x238`, `+0x1F8`) and a new `## src/MetroidPrime/
  ScriptObjects/SporbDtors.cpp (6 sites)` section classifying them kind A, plus the total line
  re-derived from the tool (185 in 79 -> 192 in 81). Without it the gate fails on `raw-offsets`.
- `python3 tools/check_docs_claims.py` - `docs claims agree with the tree`.
- `tools/check_module_wiring.py`, `tools/gen_module_order.py --check`, `tools/probe_sources.sh`,
  the port link and the 86 REL hashes: run by `tools/gate.sh` inside the judge, all green.
- No `asm` added; the only new lines under `src/` are C++ and comments.

WALL: fn_76_11AB0 94.70% - retail puts the vtable `lis` in r3 and the guarded member address in r0
(addic. r0,r30,0x528 / beq / addi r3,r30,0x528 recomputed after the store); every free-function
spelling puts the `lis` in r4 and the address in r3, and none reproduces the recompute.

NEW: none filed. The two functions left behind are reachable (`fn_76_11A50` is already at 100%), so
they belong to the `module:Sporb` item again rather than to a new one.