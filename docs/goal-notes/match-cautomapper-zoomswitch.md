# match-cautomapper-zoomswitch (lane 7)

## What I did

`CAutoMapper::ProcessMapZoomInput` is now **100% / 488 bytes** (was 90.852% / 488).

One edit, in `src/MetroidPrime/CAutoMapper.cpp` (the `mZoomState` switch in
`ProcessMapZoomInput`, lines 1038-1059): the `kZS_None` / `kZS_In` cases were written as one
fall-through label list. They are now two separate case bodies with identical text.

That is the whole job the `reason` described, and it is also literally all retail does: retail has
two *distinct* code blocks, `.L_8008DC88` (case 0) and `.L_8008DCA8` (case 1), with the same five
instructions each. Writing them as one fall-through let mwcceppc see a two-value case set
{0, 2} instead of {0, 1, 2}, so it built the dispatch tree around 2:

| | ours (before) | retail |
|---|---|---|
| test 1 | `cmpwi r0,2` / `beq case2` | `cmpwi r0,1` / `beq case1` |
| test 2 | `bge default` | `bge` -> `cmpwi r0,3` / `bge default` / `b case2` |
| test 3 | `cmpwi r0,0` / `bge case0+1` | `cmpwi r0,0` / `bge case0` |
| case bodies | 2 (15 instrs, 60 B) | 3 (23 instrs, 92 B) |

Splitting the cases restores the 9-instruction decision tree (case set {0,1,2} -> test the middle
value first), which moves the whole 92-byte body region into place with **zero** other change.
Function size went 444 -> 488, i.e. exactly the 12 bytes of extra dispatch plus the 32 bytes of
the duplicated body. Every branch displacement in the switch matches retail with no further edits.

## Measured

- `./tools/decomp_build.sh MetroidPrime/CAutoMapper.cpp` -> green; `All: 34.47% fuzzy, 27.78%
  matched, 12.89% linked (12216 / 28465 functions)`.
- `build/report.json`, unit `main/MetroidPrime/CAutoMapper`: `matched_functions` **79 -> 80 / 100**,
  `.text` fuzzy 92.54189 -> 92.63799. `CAutoMapper::ProcessMapZoomInput` 90.852 -> **100.0**.
- `./tools/flip_test.sh MetroidPrime/CAutoMapper.cpp` -> **FAIL** (see below), tree rebuilt green,
  `DOL 6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`.
- `./tools/goal_check.sh build/goal/item.json` -> `PARTIAL match-cautomapper-zoomswitch - flip_test
  FAIL, but the target rose; commit it and keep the item`. All `ok` lines, including
  `no judge-owned path touched`, `gate.sh`, `no asm added`, `target rose: 79 -> 80 / 100`.
- Independent of objdiff: I dumped our object's `.text` and retail's byte column for
  `0x8008DBEC..0x8008DDB4` and compared the 488 bytes. The only 12 differing words are the
  relocation sites in the unlinked object (`lwz r6,0(0)` + `bl 0` pairs: 7 calls and 6 `@sda21`
  loads). Every other word, including all switch branch displacements, is byte-identical.

`docs/HANDOFF.md`'s state block was rewritten by `decomp_build.sh` itself, not by me.

## What still stops the flip

The unit has 20 other unmatched functions, so it cannot flip in one item. Biggest first
(all from `build/report.json`, after this change):

```
 78.263  10620  CAutoMapper::Update(float, CStateManager&)
 94.174   4932  CAutoMapper::Draw(const CStateManager&, const CTransform4f&, float) const
 94.940   3360  CAutoMapper::ProcessControllerInput(const CFinalInput&, CStateManager&)
 96.959   1548  CAutoMapper::ProcessMapPanInput(const CFinalInput&, const CStateManager&)
 99.921   1528  CAutoMapper::ProcessMapRotateInput(const CFinalInput&, const CStateManager&)
 95.165   1240  CAutoMapper::UpdateHintNavigation(float, CStateManager&)
 95.897   1092  CAutoMapper::SAutoMapperRenderState::InterpolateWithClamp(...)
 95.425    876  CAutoMapper::GetDesiredMiniMapCameraDistance(const CStateManager&) const
 97.861    748  CAutoMapper::CheckLoadComplete()
 98.251    732  CAutoMapper::ProcessMapScreenInput(const CFinalInput&, CStateManager&)
 99.231    728  CAutoMapper::FindClosestVisibleArea(...) const
 97.561    720  CAutoMapper::CheckDummyWorldLoad(CStateManager&)
 93.670    704  CAutoMapper::SetupHintNavigation()
 99.010    384  CAutoMapper::UpdateTempleKeys(const CStateManager&)
 69.524    252  rstl::vector<rstl::auto_ptr<IWorld>, ...>::erase(...)
 95.966    236  CAutoMapper::GetAreaHintDescriptionString(unsigned int)
 60.500    176  rstl::list<CAutoMapper::SAutoMapperHintLocation, ...>::do_insert_before(...)
 91.184    152  CAutoMapper::GetAreaPointOfInterest(const CStateManager&, int) const
 27.212    132  rstl::vector<rstl::auto_ptr<IWorld>, ...>::~vector()
  0.792     96  rstl::vector<rstl::auto_ptr<IWorld>, ...>::clear()
```

`Update` alone is 10620 bytes, so this unit needs several more `progress` items, not one.

## Lesson worth keeping

**MWCC does not merge switch cases that are written separately, and it will happily generate a
different dispatch tree if you merge them.** Two case labels with byte-identical bodies stay two
blocks in the `.text` and pull the middle-value test into the decision tree; one fall-through
label list collapses them and the dispatch collapses with them. When a switch's *dispatch* is what
is missing from a diff but the bodies look right, the fix is usually to **un-merge** the cases, not
to touch the case values or the conditions. Retail's `.L_8008DC88` / `.L_8008DCA8` / `.L_8008DCC8`
triple is the tell.