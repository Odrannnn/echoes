# progress-unit-cmetroidareacollider (lane 11, `wt-mp2-goal-L11`, 2026-10-02)

`kind: progress`, `target: WorldFormat/CMetroidAreaCollider`. The unit stays `NonMatching`;
`flip_test.sh` was not run (the item says not to decide on it). This is the sixth attempt on this
unit; the five earlier ones are in `docs/goal-notes/progress-prime1-cmetroidareacollider.md`, and
nothing below re-tries their spellings.

## Result, measured

`build/goal/judge/report.base.json` (the judge's baseline) against the regenerated
`build/report.json`, unit `main/WorldFormat/CMetroidAreaCollider`:

| | before | after |
|---|---|---|
| `matched_functions` | **41 / 58** | **42 / 58** |
| `matched_code` | 9524 | 9556 |
| `matched_code_percent` | 39.47936 | 39.612003 |
| `fuzzy_match_percent` | 49.95241 | 49.98259 |
| `total_code` | 24124 | 24124 (unchanged) |

Whole build, from `build/report.json`: `matched_functions` 12519 -> **12520**,
`matched_code` 1906260 -> **1906292**, `linked` 5892 (unchanged), `complete_units` 766 (unchanged).
`tools/report_diff.py` on the two reports: **"no regression"**, with one `+100%` line and one
`RENAMED` line (the rename is reported, not counted as a loss).

A second unit moved, without changing its count: `main/MetroidPrime/CGameCollision`'s
`BuildAreaCollisionCache` **86.738464 -> 91.676926** (+4.94 points), because the call site below now
passes the area id the way retail does.

## What landed: `__ct__Q220CMetroidAreaCollider16COctreeLeafCacheFRC12CAreaOctTreei` 77.25 -> 100.00 (32 B)

Retail `.text:0x80249034` is eight instructions:

```
stw  r5,0(r3)      li r6,0        stw r4,4(r3)     stw r6,8(r3)
lbz  r0,2316(r3)   rlwimi r0,r6,7,24,24            stb r0,2316(r3)   blr
```

With the old declaration `COctreeLeafCache(const CAreaOctTree&, TAreaId)` ours started
`lwz r0,0(r5) / li r5,0 / stw r0,0(r3)` - **r5 was the address of a caller-side temporary**, not the
value. 6 of the 8 instructions differed, and no spelling of the *body* can fix that: the difference
is the argument, not the code.

Three measurements say the parameter is a scalar, not a by-value class:

1. **Retail's callee** stores the argument register straight through (`stw r5,0(r3)`).
2. **Retail's caller** (`build/G2ME01/obj/MetroidPrime/CGameCollision.o`, `BuildAreaCollisionCache`
   at 0x3f60-0x3f70) is `lwz r4,260(r31) / addi r3,r1,44 / lwz r5,4(r31) / lwz r4,8(r4) / bl` - the
   area id is *loaded into r5* and passed with no temporary. A by-value `TAreaId` call does
   materialise one: `AreaLoaded__13CStateManagerF7TAreaId`'s retail caller (CGameArea.o
   0x51c0-0x51d4) is `lwz r0,4(r31) / addi r4,r1,16 / stw r0,12(r1) / stw r0,16(r1) / bl`. The two
   calls genuinely differ, so the two symbols' manglings cannot both be right.
3. **Our own `TAreaId`** (`include/MetroidPrime/TGameTypes.hpp:26`, user-provided ctors) is not a
   POD, so mwcceppc passes it as a pointer everywhere - which is exactly why `SetAreaId` and
   `AreaLoaded` match today (they receive pointers) and this ctor could not.

Spelled `int`, the emitted body is byte-identical to retail's eight instructions (checked with
`powerpc-eabi-objdump` on `build/G2ME01/src/WorldFormat/CMetroidAreaCollider.o`, then by objdiff at
100.00%).

### The changes, per file

* `include/WorldFormat/CMetroidAreaCollider.hpp:121` - declaration becomes
  `COctreeLeafCache(const CAreaOctTree& octTree, int areaId)`, with the ABI reasoning in a comment.
* `src/WorldFormat/CMetroidAreaCollider.cpp:845` - definition follows the declaration; comment
  records retail's address, size and the 77.25% -> byte-exact measurement.
* `src/MetroidPrime/CGameCollision.cpp:234` - the one call site: `area->GetId()` ->
  `area->GetId().Value()`. Required by the signature, and it is what made that function gain 4.94
  points.
* **`config/G2ME01/symbols.txt:10324` (config change, 1 line)**: renamed
  `__ct__Q220CMetroidAreaCollider16COctreeLeafCacheFRC12CAreaOctTree7TAreaId` ->
  `...FRC12CAreaOctTreei` - the name MWCC emits for the `int` parameter, read out of our own object
  with `powerpc-eabi-nm`, which is the documented rename mechanism
  (`docs/RUNNING_THE_DECOMP.md`, "Rename the retail symbol instead"). The old mangling was a project
  label; the bytes contradict it. Nothing else references that name: `grep COctreeLeafCache
  config/G2ME01/rels/*/symbols.txt` is empty, and the gate's `hashes vs config.yml` is ok.
  This is the second such label corrected in this unit (attempt 5 corrected the Edge function's
  `9CVector3f` -> `RC9CVector3f`).

## Gates, all run on the final tree

```
./tools/goal_check.sh build/goal/item.json
goal_check: item progress-unit-cmetroidareacollider (progress) target=WorldFormat/CMetroidAreaCollider
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 12519 -> 12520   linked 5892 -> 5892
  ok    check_symbol_names.py
  ok    All:  35.33% fuzzy, 29.17% matched, 12.91% linked (12520 / 28465 functions)
  ok    target rose: main/WorldFormat/CMetroidAreaCollider: 41 -> 42 / 58 functions
  ok    no asm added
goal_check: PASS progress-unit-cmetroidareacollider
```

Also: `sha1sum build/G2ME01/main.dol` = `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`;
`python3 tools/check_symbol_names.py` = `checked 525 units; 0 declared names are missing from their
object`; `python3 tools/check_decl_order.py --unit WorldFormat/CMetroidAreaCollider` = `ok: 1
unit(s) checked, none emits its functions out of retail order`; `tools/gate.sh` prints
`GATE PASS  23a843aa+5 changed` with `hashes vs config.yml  ok` and `port probe  ok`. No `asm` was
added. `docs/HANDOFF.md` was rewritten by the gate's own `MP_GATE_DOCS_WRITE=1` (state block
12519 -> 12520 and DOL units 10951 -> 10952); no other doc was edited, and no `NEW:` line is filed.

## Reusable finding (a codegen rule, so not filed as `NEW:`)

**A by-value 4-byte class parameter is passed as a pointer to a caller-side temporary, so a retail
body that stores the argument register straight through proves the parameter is a scalar.** When a
mangled name in `symbols.txt` claims a class parameter and retail's own caller passes the value in a
register (`lwz rN,off(base)` with no `addi rN,r1,off` + `stw`), the mangling is a project label and
the fix is to write the scalar spelling, compile, read the emitted name and rename the `symbols.txt`
entry. The cheap check for any candidate is to print the nine instructions before each retail `bl`
to the symbol (`.tmp/opencode/callers.py <substring>` this run; it walks `build/G2ME01/obj/**/*.o`).
Worth running over the other 24 currently-unmatched functions in the tree whose mangled names carry
a by-value `7TAreaId` - most are constructors in `CCollisionActor`, `CWeapon`, `CGameLight`,
`CScriptDock`, `CAnimData`, `CModelData`, `CCollisionActorManager`, `CParticleDatabase` and
`CScriptObjectLoaderHelper` - but a register check cannot prove a name, only refute one, and those
functions have their own diffs; that sweep is its own item, not this one.

## What is still open in the unit (measured this run, nothing changed by this run)

16 of 58 functions are below 100.00%; `total_code` is 24124 and unchanged.

| score | bytes | function | state after this run |
|---|---|---|---|
| 79.07 | 992 | `MovingAABoxCollisionCheck_Edge` | closest. Attempt 3 measured the "one table index per operand" spelling at 77.50% (worse than the shared index, 78.31 -> 79.07 with the by-ref `dir` signature). No new spelling tried this run, so no `WALL:` line. |
| 74.41 | 1280 | `AABoxCollisionCheck_Cached(COctreeLeafCache)` | frame 800 vs retail 688; ours saves f15-f31, retail f19-f31 - several extra live floats. |
| 65.28 | 348 | `AABoxCollisionCheck` | **checked this run**: retail's tail is `mr r4,r31 / addi r3,r1,8 / bl fn_8012753C / addi r3,r1,8 / addi r4,r1,44 / bl AABoxCollisionCheck_Internal`, and ours is the *same call shape* (`addi r3,r1,44 / bl GetRootNode__12CAreaOctTreeCFv`), because a member returning a class by value takes the hidden return pointer in r3 and `this` in r4. So `GetRootNode` is **not** the difference here - unlike `CGameCollision`, which had to call `fn_8012753C` explicitly. The 65.28% is FPR allocation (retail saves f29/f30/f31, ours f28-f31) plus a 64-byte larger frame; `min`/`max` by value vs by reference was already measured equal (65.15 / 65.28). |
| 23.25 | 1696 | `__ct__CMovingAABoxComponents` | largest remaining; a rewrite from disassembly (statics + guard bytes, the `GetEdge` + 12-iteration edge loop, `mDominantAxis`, `jumptable_803B8A08`, the reciprocal block) per attempt 3. |
| 0.88 | 4168 | `MovingSphereCollisionCheck_Cached` | still the stub `dOut = d; return false;`. **Best remaining target**: real Prime 1 source, no new infrastructure needed. Attempt 4's lane had it at 42.77% with one identified 87-instruction hole and the untried idea of spelling `outsideEdges` as three separate `bool` locals rather than `bool[3]`. |
| 13.64 / 2.33 / 2.13 / 1.35 / 0.98 | 232/172/188/296/408 | `BuildCollisionCache`, `ReserveTriangles`, `AddTriangle`, `CacheAllNodes`, `CacheNodes` | still blocked on the missing `CCollisionCache` class (forward-declared only, `include/WorldFormat/CCollisionCache.hpp:7`). |
| 0.45 / 1.28 / 1.26 / 1.97 / 1.62 | 1256/436/444/284/2276 | the five `*_Cached(const CCollisionCache&, ...)` overloads | same blocker. |
| none (unpaired) | 92 | `fn_80248E04` | unreachable: a ctor's name is fixed by the mangler and the reference member forbids `*self = *other` (attempt 2). Do not spend a run on it. |
