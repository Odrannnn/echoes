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
---

# match-cautomapper-zoomswitch (lane 6, second attempt)

## What I did

**Re-measured first: the job the `reason` names was already done.** Lane 7's change is in this
tree (commit `3da3b1ae progress: match-rstl-string-eq-out-of-line`), so
`CAutoMapper::ProcessMapZoomInput` is at 100% here and the unit sat at **81 / 100**. That is only
partly done, so per the brief I did the rest rather than writing `STALE:`. Four more functions now
match, all of them `rstl`/list template members that our build emitted with the wrong *shape*:

| function | retail | before | after |
|---|---|---|---|
| `clear__Q24rstl61vector<rstl::auto_ptr<IWorld>, rmemory_allocator>` | 0x8008BE50, 96 B | 0.79% | **100%** |
| `__dt__Q24rstl61vector<rstl::auto_ptr<IWorld>, rmemory_allocator>` | 0x8008BF30, 132 B | 27.21% | **100%** |
| `erase__...FQ24rstl142pointer_iterator<...>Q24rstl142pointer_iterator<...>` | 0x8008C000, 252 B | 69.52% | **100%** |
| `do_insert_before__Q24rstl73list<CAutoMapper::SAutoMapperHintLocation, ...>` | 0x800907EC, 176 B | 60.50% | **100%** |

Four explicit specializations in `src/MetroidPrime/CAutoMapper.cpp`, nothing else in the tree
touched. They are the header's own bodies (include/rstl/vector.hpp:130,256,273 and
include/rstl/list.hpp:110) with one line each spelled as a direct call instead of through
`rstl::destroy`; no behaviour is removed, no initialisation is dropped.

## The three vector members: mwceppc inlines `destroy(begin, end)`; retail does not

Retail calls `fn_8008BEB0` - the out-of-line `destroy_impl` loop - out of line from exactly these
three places, and each of them builds its iterator pair on the stack and `bl`s it. Ours inlined
the loop at all three, so `clear` was 23 instructions where retail's is 24, and the two `erase`s
were missing a `bl`.

The loop is only 14 folded instructions, far under `-pragma "inline_max_size(125)"`, so this is
not a size threshold - mwceppc just inlines `destroy` -> `destroy_impl` when both are plain
`inline` templates. Spelling the call directly is what turns them back into calls.

Two spellings were measured and rejected, both worth not re-trying:

- **Specialize `rstl::destroy(It, It)` for this iterator type** (keeping `fn_8008BEB0`'s body as
  `rstl::destroy_impl`). Compiles, and all three rise - but they stop at 95.00% / 96.36% /
  94.94% and the matched count does not move: retail loads `end()` *first* (into the register
  `mItems` was in, then reloads `mItems`), ours loads `begin()` first and keeps both at once, and
  the two `addi r3/r4, rN` land on the other frame slot pair. Extra nesting does not fix it.
- **Specialize `rstl::destroy_impl(It, It)` instead.** Same three percentages, and it costs
  `fn_8008BEB0` itself: it is called at 100% at HEAD, its body must stay a call to the *generic*
  `destroy_impl`, so specialising that one forces the loop to be written out by hand, and the
  hand-written loop scores 87.25%. Net -1. Do not.

The winning shape is the thin one: `fn_8008BEB0(begin(), end());` written straight into a
specialized `clear`/`~vector`, and `fn_8008BEB0(first, last);` into a specialized `erase`. With
named locals (`iterator last = end(); iterator first = begin();`) mwceppc CSEs the two `mItems`
loads and it stops at 94.96% - one instruction *worse* than going through `destroy`.

**MWCC parses an explicit specialization's return type without `typename`, and will not accept a
nested-class return type spelled out**: `typename vector<...>::node*` and even
`list<A, B>::node*` at namespace scope are "declaration syntax error". The two that worked are
`typename vector<...>::iterator` (no `typename` needed either, but harmless) and a file-scope
`typedef` for the list. `SAutoMapperHintLocation` also has to be written
`CAutoMapper::SAutoMapperHintLocation` in the declarator - inside `namespace rstl` the bare name
does not resolve.

## `do_insert_before`: mwceppc outlines `create_node` here and only here

`list<T>::do_insert_before` calls `create_node`. mwceppc inlines that for the hint-STEP list (which
already matched retail at 100%) and *outlines* it for the hint-LOCATION list: our object carried a
`create_node__...HintLocation...` symbol that retail's object does not define at all, and
`do_insert_before` was 112 bytes where retail's is 176. Retail inlines the allocation and the
value copy in all four of its list instantiations.

The specialization writes `create_node`'s four lines in instead of calling it. That alone gets
87.77%; the last four instructions need `n->mPrev` read into a local *before* the allocation,
because retail's `lwz r31,0(r4)` is at 0x80090818, before the `bl` at 0x8009081C. With
`nn->mPrev = n->mPrev;` the load lands after the call and the function is 42 instructions to
retail's 44. With `node* const prev = n->mPrev;` first, it is 44 and matches.

## Measured

- `./tools/decomp_build.sh MetroidPrime/CAutoMapper` -> green. `All: 34.58% fuzzy, 27.94% matched,
  12.89% linked (12243 / 28465 functions)`; base was `27.93%` / `12239`.
- `build/report.json`, unit `main/MetroidPrime/CAutoMapper`: `matched_functions` **81 -> 85 / 100**,
  `.text` fuzzy 92.63799 -> **93.461296**. The four functions above are the only rows in the whole
  report that changed, and every one of them went **up**.
- `./tools/goal_check.sh build/goal/item.json` -> **PARTIAL match-cautomapper-zoomswitch - flip_test
  MetroidPrime/CAutoMapper.cpp: FAIL, but the target rose; commit it and keep the item**. All `ok`
  lines: `no judge-owned path touched`, `gate.sh (includes DOL sha1, 86 RELs, report diff, wiring,
  docs claims, port probe)`, `counts: matched 12239 -> 12243  linked 5860 -> 5860`,
  `check_symbol_names.py`, `no asm added`, `target rose: 81 -> 85 / 100`.
- `python3 tools/check_decl_order.py --unit MetroidPrime/CAutoMapper` -> `ok`. This one mattered:
  with all four specializations written in one block near the top of the file it reports
  `would break on a flip`, because mwcceppc emits *explicitly defined* members in source order
  rather than in the implicit-instantiation tail. Each specialization is now declared where retail's
  address puts it - `erase` and `clear` beside `GetAreaHintDescriptionString` and `fn_8008BEB0`,
  `do_insert_before` just after `SetupTeleportNavigation`. **`~vector` cannot be:** `CAutoMapper`'s
  own destructor implicitly instantiates it long before retail's ordering would put it, and
  mwcceppc then reports the specialization as *redefined*. It stays near the top, which is where its
  implicit instantiation already put it, and the order is still `ok`.
- `python3 tools/check_symbol_names.py` -> `checked 525 units; 0 declared names are missing`.
- `./tools/unit_fit.sh MetroidPrime/CAutoMapper.cpp` -> `.text claimed 46452, ours 47940, over by
  1488`; `35 function(s) present in ours but not in the retail unit object, 3156 bytes`. Both are
  pre-existing COMDAT weak copies, and this change *removes* one of them
  (`create_node__...HintLocation...`).

`docs/HANDOFF.md`'s state block was rewritten by `decomp_build.sh`, not by me.

## What still stops the flip

15 functions, unchanged from lane 7's list minus the four above. `Update` is 78.263% / 10620 bytes
on its own, so this unit needs several more items:

```
  78.263  10620  CAutoMapper::Update(float, CStateManager&)
  91.184    152  CAutoMapper::GetAreaPointOfInterest(const CStateManager&, int) const
  93.670    704  CAutoMapper::SetupHintNavigation()
  94.174   4932  CAutoMapper::Draw(const CStateManager&, const CTransform4f&, float) const
  94.940   3360  CAutoMapper::ProcessControllerInput(const CFinalInput&, CStateManager&)
  95.165   1240  CAutoMapper::UpdateHintNavigation(float, CStateManager&)
  95.425    876  CAutoMapper::GetDesiredMiniMapCameraDistance(const CStateManager&) const
  95.966    236  CAutoMapper::GetAreaHintDescriptionString(CAssetId)
  96.959   1548  CAutoMapper::ProcessMapPanInput(const CFinalInput&, const CStateManager&)
  97.561    720  CAutoMapper::CheckDummyWorldLoad(CStateManager&)
  97.861    748  CAutoMapper::CheckLoadComplete()
  98.251    732  CAutoMapper::ProcessMapScreenInput(const CFinalInput&, CStateManager&)
  99.010    384  CAutoMapper::UpdateTempleKeys(const CStateManager&)
  99.231    728  CAutoMapper::FindClosestVisibleArea(...) const
  99.921   1528  CAutoMapper::ProcessMapRotateInput(const CFinalInput&, const CStateManager&)
```

`ProcessMapRotateInput` at 99.921% is **not** one instruction away: `bytescmp` reports 74 differing
words of 382 and every one of them is a relocation field (`bl`, and `lfs`/`lfd` of the `@sda21`
constants). Same size, same instruction stream - do not spend a run on it.

The two small ones measured here, for whoever picks them up:

- **`GetAreaPointOfInterest` (91.184%)** - register allocation only, and a good example of why:
  retail holds `mWorld` in r30 and the `CMapArea*` in r31, ours holds the world in r31 and the area
  in r30, which costs two instructions (`mr r0,r3` / `mr r30,r0`) where retail has `mr r31,r3`.
  38 instructions to ours' 39, everything else identical. Nothing structural to fix; it needs a
  different spelling of the expression, and I did not find one.
- **`GetAreaHintDescriptionString` (95.966%)** - one instruction too many, and it is real: ours
  hoists `addi r8,r30,32` to the top of the `i` loop and then does `add r8,r0,r8` inside, where
  retail does one `add r8,r0,r30`. Both reach the same addresses (r30 is the `i * 48` stride and the
  constant 32 is folded in early instead). Retail also loads `mHintLocations.mStart` (584(r3))
  before the `locations` loop and ours reloads it inside, which is a shuffle, not an extra. The
  shape to try is `hint.GetLocations()` returning the vector by value rather than by reference, so
  the data pointer is already offset; the `SHintLocation` field offsets themselves agree
  (retail 36/44, ours 4+32/12+32).

## Lesson worth keeping

**A template member that our build inlines and retail does not is not a register-allocation
problem - spell the call.** `clear`, `~vector`, `erase` and `do_insert_before` all had exactly the
right instructions in the wrong *shape*, and every one went to 100% with a one-line change of
spelling and no other edit. The tell is the instruction *count* differing (96 vs 92 bytes, 132 vs
128, 176 vs 112): a matching count with a different shape means something was inlined or outlined
that should not have been.
