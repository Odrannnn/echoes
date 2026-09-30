# progress-prime1-cbodycontroller — `MetroidPrime/BodyState/CBodyController`

**Outcome: the item passes. `matched_functions` for the unit rose 26 → 28 of 31**; two
functions reached 100% and the third-and-fourth went from 58.6%/68.0% to 96.0%/95.5%. The
unit stays `NonMatching` and was not flipped. The whole gate is clean except the docs
state block, which the judge rewrites itself (`MP_GATE_DOCS_WRITE=1`).

`src/MetroidPrime/BodyState/CBodyController.cpp` is the only file changed. No header, no
config, no `tools/`, no docs.

## Measured

```
./tools/gate.sh build/goal/judge/report.base.json
  configure ok | ninja + build.sha1 ok | hashes vs config.yml ok | report ok
  per-function diff  matched 9890 -> 9892  linked 4895 -> 4895
                     (+2 functions at 100%, 0 units newly linked; no WORSE/GONE/UNLINKED)
  module wiring ok | dol_read ok | gs offsets ok | raw offsets ok | decl order ok
  files.cmake ok | module order ok | port probe ok | port link gap ok | reach stubs ok
  docs claims FAIL - only 'matched 9892 / 28465' and 'DOL units 8481 / 16726' are stale
                     in the HANDOFF state block; the judge rewrites those (--write)
./tools/decomp_build.sh  -> All: 30.47% fuzzy, 22.42% matched, 11.74% linked (9892 / 28465)
sha1sum build/G2ME01/main.dol -> 6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
./tools/probe_sources.sh -> 749 files, 0 failed, 0 errors; LINKED (250 undefined, 0 duplicates)
python3 tools/check_symbol_names.py -> checked 503 units; 0 declared names are missing
python3 tools/check_decl_order.py --unit MetroidPrime/BodyState/CBodyController -> ok
```

Per function, from `build/report.json` (`main/MetroidPrime/BodyState/CBodyController`):

| function | before | after | Prime 1's source |
|---|---|---|---|
| `FaceDirection` (348B) | 99.586% | **100%** | needed its spelling (see below) |
| `UpdateFrozenInfo` (468B) | 95.137% | **100%** | needed two small edits |
| `FaceDirection3D` (568B) | 68.042% | 95.472% | needed restructuring to if/else |
| `FaceDirectionOnSurface` (656B) | 58.622% | 96.049% | not in Prime 1 at all |
| `HasBodyState` (48B) | 83.167% | 83.167% | identical, could not be helped |

Unit `fuzzy_match_percent` 90.838 → **98.873**, `matched_code_percent` 60.574 → 75.977.

## What each function needed

**`FaceDirection` - Prime 1's source is right, its spelling is load-bearing.** The bodies
were already the same computation; the only difference was which stack slot each
`CUnitVector3f` temporary got. Retail passes the *normalized* direction as arg1 and the
actor's forward as arg2; with our spelling 2.7 did the opposite. Pulling Prime 1's three
named locals out (`normalized`, `forward`, `angle`) and spelling the second unit vector with
the three-float constructor `CUnitVector3f(normalized[kDX], normalized[kDY], normalized[kDZ])`
instead of `CUnitVector3f(normalized, kN_No)` fixed both the slots and the store order:
**99.586 → 100**. Note this is *not* an argument swap - the arg order is unchanged from
what the tree had, and it is Prime 1's own source.

**`UpdateFrozenInfo` - 95.137 → 100% with two one-token edits.** (1) `if (mActor) { mActor->... }`
re-loaded the pointer for the call; `if (CActor* ent = mActor) { ent->... }` keeps it in r3
(95.137 → 96.162). (2) `mCmdMgr.DeliverCmd(...)` → `CommandMgr().DeliverCmd(...)` - same
object, but the accessor flips the allocator onto retail's r30/r31 assignment and the
scratch reuse in the `CBCAdditiveReactionCmd` block (96.162 → **100**). Prime 1 writes
`CommandMgr()`, so this is Prime 1's spelling too.

**`FaceDirection3D` - 68.042 → 95.472%.** Retail has two full copies of the
`ScalarVector` + `RotateInOneFrameOR` tail, one per branch of `if (dot < -0.99981f)`, and
Prime 1 writes them out; our tree had a `?:` ternary, which 2.7 merged into one tail.
Splitting the ternary into Prime 1's two branches took it 68.0 → 94.6, and splitting the
3-way `if (mFrozen || !a || !b) return;` guard into Prime 1's `if (mFrozen) return;` plus
`if (a && b) {...}` (which is what gives retail `beq`/`beq` instead of our `beq`/`bne`+`b`)
gave 95.4. `GetUp()` is 0.08 better than Prime 1's `GetColumn(2)` (95.472 vs 95.390) because
`GetColumn(int)` is an out-of-line call in this repo and `GetUp()` inlines to the three
`lfs`s; kept `GetUp()`.

**`FaceDirectionOnSurface` - 58.622 → 96.049%.** Not in Prime 1 (Echoes-only, and the
header still says the name is a guess). The same two fixes as `FaceDirection3D` - the guard
split and the if/else with duplicated tails - took it 58.6 → 85.8, and then dropping the
`up` local from the `AxisAngle` call (calling `actor->GetTransform().GetUp()` again inside
the branch, which is what retail does) took it to 96.05, because keeping `up` live across
three calls forced four callee-saved doubles. One more object is needed: retail really does
build `CUnitVector3f desired(projected)` and never read it back (the out-of-line
`__ct__13CUnitVector3fFRC9CVector3f` call is in the bytes), and the dot product and
`ShortestRotationArcClamped` both take the **un-normalized** projection. Kept, with a
comment, because without it the function drops to 94.23%.

## The residue, exactly

`.text` is now **short by 24 bytes of real code**, and both short functions are short by
exactly the same thing. Measured with `nm -S` on both objects:

```
ours 5516 = 5272 (the 31 shared functions) + 244 (3 COMCAT destructors we emit, pre-existing)
retail 5296
FaceDirectionOnSurface  ours=644 retail=656     <- 12 bytes
FaceDirection3D         ours=556 retail=568     <- 12 bytes
every other function    byte-equal size
```

Retail's `AxisAngle` branch materialises the by-value `GetUp()` result into a dead
`CVector3f` at `0x38(r1)` and then copies it into the parameter slot at `0x44(r1)` - six
stores of the same three floats. mwcceppc 2.7 elides the copy and uses `0x38` for the
argument directly, three stores. The 12 bytes and the frame size (`-0xe0` vs retail's
`-0xf0`) follow from that. Spellings tried for it, all measured, none changed the code at
all (2.7 folds every one of them):

* `CUnitVector3f(CVector3f(actor->GetTransform().GetUp()), CUnitVector3f::kN_No)` - 95.472
* `const CVector3f upVec = ...GetUp();` then `CUnitVector3f(upVec, kN_No)` - 95.472
* `const CUnitVector3f up(...); ... AxisAngle(up, angle)` - 90.425 (worse)
* `const CVector3f upVec = ...GetUp(); const CUnitVector3f axis(upVec, kN_No); AxisAngle(axis, ...)`
  - 90.425 (worse)

`HasBodyState` (48B, 83.167% = 10 of 12 instructions) is two instruction slots of pure
prologue scheduling and nothing else: retail emits `lwz r3, 0x0(r3)` *before* the LR spill
`stw r0, 0x14(r1)`, 2.7 always spills first. `(int)state` (Prime 1's spelling) and a named
`const CAnimData& anim` local both leave it at 83.167% with identical code. 12 instructions
cannot be steered from the source.

## Note for the next run

`unit_fit.sh` now reports `.text over by 220` where it used to say `over by 28`. That is not
a regression: before this change the two `FaceDirection*` bodies were ~24 bytes *under*
retail each because 2.7 had merged the two `RotateInOneFrameOR` tails, and the 28-byte
over-run was an accident of that under-run. The bodies are now retail's shape, and the
remaining gap is the 24 bytes above. The 3 extra functions (`__dt__CBCAdditiveReactionCmd`,
`__dt__rstl::basic_string`, `__dt__CBodyStateCmd`, 244 bytes) are pre-existing and
unchanged by this item.
