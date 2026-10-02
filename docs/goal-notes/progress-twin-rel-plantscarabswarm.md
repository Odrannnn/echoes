# progress-twin-rel-plantscarabswarm - module:PlantScarabSwarm

Lane 13, 2026-10-02. Result: **PASS** (`./tools/goal_check.sh build/goal/item.json`).

```
goal_check: item progress-twin-rel-plantscarabswarm (progress) target=module:PlantScarabSwarm
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 13308 -> 13315   linked 6356 -> 6363
  ok    check_symbol_names.py
  ok    All:  37.36% fuzzy, 30.79% matched, 13.66% linked (13315 / 28465 functions)
  ok    target rose: module:PlantScarabSwarm: 10 -> 17 / 71 functions
  ok    no asm added
goal_check: PASS progress-twin-rel-plantscarabswarm
```

## What landed

New `Matching` unit `PlantScarabSwarm/MetroidPrime/ScriptObjects/CPlantScarabSwarmTail`, claiming
**`.text 0x2C00..0x2F2C`** - the whole 7-function run the item lists as its longest, 812 bytes,
**7/7 functions at 100.00%, `complete_units: 1`** in `build/report.json`, `fuzzy_match_percent`
100.00. The module's summed `matched_functions` went **10 -> 17 of 71** (5 head + 7 new + 5
`REL_Setup`).

| addr | name | size | what it is |
| --- | --- | --- | --- |
| 0x2C00 | `fn_49_2C00` | 0x54 | two-word own-pointer teardown; frees the pointee then the object |
| 0x2C54 | `fn_49_2C54` | 0x58 | `single_ptr<CModelData>` teardown: `__dt__10CModelDataFv(p,1)` then `Free(self)` |
| 0x2CAC | `fn_49_2CAC` | 0x94 | `CActorParameters` copy ctor: `bl fn_49_2D40` + 0x3C..0x5C |
| 0x2D40 | `fn_49_2D40` | 0x7C | `CLightParameters` copy ctor, 0x3C bytes member for member |
| 0x2DBC | `fn_49_2DBC` | 0xA4 | `rstl::vector<int>::reserve` - the 4-byte block's |
| 0x2E60 | `fn_49_2E60` | 0xAC | the 0x24-byte block's `reserve` |
| 0x2F0C | `fn_49_2F0C` | 0x20 | two-pointer forwarder to the 0x24 block's `destroy_impl` (0x2F2C) |

Per function, measured: each of the seven was **0.00% before** (they sat in
`auto_00_000000D8_text`, which is retail's bytes and never scores) and **100.00% after**. No twin's
source is used unchanged - every body carries this module's own callees and offsets.

**Two spellings that do *not* work, both measured on this run**, so the next one skips them:
- Including `MetroidPrime/CActorParameters.hpp` and emitting the real copy ctor through a
  placement new gives the right bytes **but the wrong symbol**: the object defines
  `__ct__16CLightParametersFRC16CLightParameters` / `__ct__16CActorParametersFRC16CActorParameters`,
  and this range's two names in `symbols.txt` are `fn_49_2D40` / `fn_49_2CAC`, so objdiff pairs
  nothing (the reason `CAtomicAlpha7E0.cpp` writes its C name). Renaming the two entries would work
  and was not needed once the struct spelling below was found.
- A whole-struct assignment (`*self = *other`, or a `SLightParms` struct of the same shape) does not
  produce the copy ctor at all: mwcceppc emits a *weak* `__as__11SLightParmsFRC11SLightParms`
  (the copy-assignment operator, 0x78 bytes, unpipelined) plus a framed 0x20-byte thunk that calls
  it - 31 of 31 words wrong. The memberwise body is what gives retail's schedule.

Files: `src/MetroidPrime/ScriptObjects/CPlantScarabSwarmTail.cpp` (new),
`config/G2ME01/rels/PlantScarabSwarm/splits.txt` (the claim),
`configure.py` (`Object(Matching, ..., mw_version="GC/2.7")` in the existing `Rel(...)` block),
`config/G2ME01/config.yml` (`force_active`), `files.cmake` (one line, comment).

Independent of the judge: all 86 module sha1s equal `config/G2ME01/config.yml`
(`modules ok 86 bad 0`), `PlantScarabSwarm.rel` is `67240808f42d66482dfaa11daa09a9994cd63e92` -
the value `config.yml` already records - and is `cmp`-identical to
`orig/G2ME01/files/RelProd/PlantScarabSwarm.rel`; `main.dol` is still
`6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`.

```
tools/audit_rel_claim.py PlantScarabSwarm
  ok CPlantScarabSwarmRel.cpp  0x0..0xD8      5/5
  ok CPlantScarabSwarmTail.cpp 0x2C00..0x2F2C 7/7
  ok REL/REL_Setup.cpp         0x32A0..0x3444 5/5
  0 claim(s) with a problem; preplf 71 text symbols, plf 71, 0 dropped by -strip_partial
tools/unit_fit.sh MetroidPrime/ScriptObjects/CPlantScarabSwarmTail.cpp
  .text claimed 812 ours 812 retail 812 fits; no extra functions
probe_sources.sh  856 files, 0 failed, 0 errors; link LINKED (286 undefined, 0 duplicates)
```

## The four measurements that decided the shapes

**1. `mw_version="GC/2.7"` is the whole of the copy constructors' diff.** The same source under the
module default GC/1.3.2 schedules the memberwise copies as a load/store ladder; 2.7 emits retail's
2-deep pipeline ("load the next member, then store the previous one"). `fn_49_2D40` is **0 of 31
words** under 2.7 and **28 of 31** under 1.3.2 - the same split `CSandBossRelTail.cpp` and
`CLumiteRelTail.cpp` record for their modules' tails, which is why 2.7 is the cheapest first move
on one of these.

**2. `force_active` is required for two of the seven.** `fn_49_2C00` and `fn_49_2C54` are the only
members of the run with no `bl` anywhere in the module, so mwldeppc dead-stripped them: the first
build linked **0xAC = their 0x54 + 0x58 bytes short** (`.text` 13208 against retail's 13380) with
everything at 100%. `config.yml`'s per-module `force_active:` list (dtk 1.8.4) is the fix. The other
five are reachable: `fn_49_2CAC` from 0x23E8, `fn_49_2DBC` from 0x2560, `fn_49_2E60` from 0x1840,
`fn_49_2D40` from `fn_49_2CAC`, `fn_49_2F0C` from `fn_49_2E60`.

**3. `fn_49_2CAC` has to return the receiver.** Retail reloads r3 with `this` after the `bl`
(`mr r3,r30`) and then uses **r4** as the copy's scratch register, where a `void fn(void*, const
void*)` uses r3 and drops the reload: **0 of 37 words vs 15 of 37**. `return self;` (pointer return
type) is the whole difference, and it is not a stub - the stores are identical either way.

**4. The 4-byte `reserve`'s four frame stores come from a by-value iterator class.**
`fn_49_2DBC` is `reserve__Q24rstl36vector<i,Q24rstl17rmemory_allocator>Fi`'s own twin (DOL
0x80052220, 0xA4 bytes there too, matched in `src/MetroidPrime/CWorld.cpp`), and it is the one
member of the run whose element copy is *inlined*
while the two iterator arguments are still materialised at r1+0x8/0xc (`end`) and r1+0x10/0x14
(`begin`). Retail's register allocation - `in` in r5, `end` in r3, `out` in r4 - and its 0x30 frame
follow from that. A raw-pointer `for` loop compiles to **0x90 against 0xA4 bytes, 36 instructions
against 41** - the four stores, the extra `b` and 0x10 of frame are the whole difference; `SIntIter`
plus an `inline` `uninitialized_copy` over it is what reproduces them. `fn_49_2E60` is the same situation with a 0x24 stride and an out-of-line callee, and is
byte-for-byte `Carve801FF5A0.cpp`'s `fn_801FF5A0` (a DOL carve with the same body) - copy that
file's `SStateIter`/block spelling rather than re-deriving it.

## Host branch and reviewer-facing notes

The bodies are inside `#ifdef __MWERKS__` with an empty host branch (the `CLumiteRelTail.cpp` /
`CSandBossRelTail.cpp` arrangement), so `files.cmake` can list the file - required because
`check_files_cmake.py` fails a configured `Matching` object that is neither listed nor EXCLUDED -
without adding an undefined reference: two of its callees (`fn_49_2F2C`, `fn_49_2FBC`) are the
module's own unclaimed middle and no host object defines them. The port's undefined count is
measured unchanged at 286.

`flip_test.sh` cannot test a REL unit (it resolves REL unit paths under `extern/musyx/src/`) and
`check_decl_order.py --unit` reports `0 unit(s) checked` for one; both are the pre-existing tool
limits `docs/goal-notes/progress-twin-rel-scriptgui.md` and `...-digitalguardian.md` record. The
acceptance test here is the module sha1 against `config.yml`, which holds.

The linker prints `FORCEACTIVE symbol 'lbl_49_rodata_50' is either not a global symbol or doesn't
exist. Ignored.` for this module - **pre-existing, not from this change**: it is the long-standing
`.rodata` claim on `REL/REL_Setup.cpp`, and `build/goal/rebase-build.log` carries the identical
warning for module 2's `lbl_2_rodata_B8`.

## What is left in this module (54 unmatched functions)

The item's twin list had 25 entries; 7 landed and 18 remain, in ranges of their own (one
contiguous range per file, so only these are single-file claims):

| range | fns | bytes | note |
| --- | --- | --- | --- |
| 0xBA0..0xC24 | 2 | 0x84 | `fn_49_BA0` + `fn_49_BE8` |
| 0xFB4..0x1024 | 2 | 0x70 | `fn_49_FB4` + `fn_49_FD4` |
| 0x1024..0x10D0 | 2 | 0xAC | `fn_49_1024` + `fn_49_107C`, the `bit_vector`/`vector<int>` dtors the ScriptGui note calls "writable as members" |
| 0x1F8C..0x2048 | 2 | 0xBC | `fn_49_1F8C` + `fn_49_2010` |
| 0x217C..0x2254 | 2 | 0xD8 | `fn_49_217C` + `fn_49_2200` |
| 0x288C..0x290C | 3 | 0x80 | `fn_49_288C` + `fn_49_28C4` + `fn_49_28E4` |
| 0x311C..0x323C | 2 | 0x120 | `fn_49_311C` + `fn_49_3184` |

The remaining three are isolated single functions - `fn_49_1EA8` (0x20), `fn_49_2A48` (0x6C),
`fn_49_2B08` (0x64) - each its own file, so 15 + 3 = the 18 twins left (in 10 units). Nothing was
measured on any of them this run - this is a map, not a wall - and the same two rules transfer: try
`GC/2.7` first, and check the module's FORCEACTIVE list before blaming the spelling for a short
module.

No `NEW:` filed: the remaining work is this same module and this same target, which the driver
requeues as-is (a `NEW:` restating the current item is excluded by the instructions). No `WALL:` -
nothing was left at a sub-100% score.

Note for the driver: `docs/HANDOFF.md` and `docs/RUNNING_THE_DECOMP.md` show as modified in
`git status`; those are `goal_check.sh`'s own gate rewriting the derived counts, not edits of mine.
