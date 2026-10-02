# progress-twin-rel-snakeweedswarm - SnakeWeedSwarm (module 71) template tail, +6 matched

`kind: progress`, `target: module:SnakeWeedSwarm`. Worktree `../wt-mp2-goal-L8` on `goal/lane-8`.

## Result (measured, not recalled)

| | before | after |
| --- | --- | --- |
| `build/report.json` `measures.matched_functions` | 13394 | **13400** (+6) |
| `SnakeWeedSwarm/*` matched summed (the judge's `module:` rule) | 9 | **15** (+6) / 74 functions |
| `SnakeWeedSwarm/MetroidPrime/ScriptObjects/CSnakeWeedSwarmVecTail` | did not exist | **6 / 6, `fuzzy 100.000`** |
| `report.json` `measures.total_functions` | 28465 | 28465 (unchanged) |
| all 86 REL sha1s vs `config/G2ME01/config.yml` | ok | **ok, 0 bad** |
| `cmp build/G2ME01/SnakeWeedSwarm/SnakeWeedSwarm.rel orig/G2ME01/files/RelProd/...` | - | **byte-identical** |
| `tools/unit_fit.sh MetroidPrime/ScriptObjects/CSnakeWeedSwarmVecTail.cpp` | - | `.text claimed 784 ours 784 retail 784 fits`, **no extra functions** |

`./tools/goal_check.sh build/goal/item.json` - **PASS**, run three times (once before the final
comment-only edit, twice after; identical each time):

```
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 13394 -> 13400   linked 6442 -> 6448
  ok    check_symbol_names.py
  ok    All:  37.47% fuzzy, 30.90% matched, 13.77% linked (13400 / 28465 functions)
  ok    target rose: module:SnakeWeedSwarm: 9 -> 15 / 74 functions
  ok    no asm added
goal_check: PASS progress-twin-rel-snakeweedswarm
```

`gate.sh` reports the move as what it is, not as a loss:
`per-function diff  SPLIT  SnakeWeedSwarm/auto_00_000000DC_text: 6 function(s) moved into
SnakeWeedSwarm/MetroidPrime/ScriptObjects/CSnakeWeedSwarmVecTail (exact count match)`.

## What I claimed

One new unit, `MetroidPrime/ScriptObjects/CSnakeWeedSwarmVecTail.cpp`, holding
`config/G2ME01/rels/SnakeWeedSwarm/splits.txt`'s new range

```
MetroidPrime/ScriptObjects/CSnakeWeedSwarmVecTail.cpp:
	.text       start:0x00003A34 end:0x00003D44
```

six adjacent functions and nothing else (`0x310` = 784 bytes, and the object's `.text` is 784
bytes, so the object fills the claim exactly). This is the item's **longest run** and it sits
immediately below `REL_Setup.cpp`'s `0x3D44`, so nothing had to be left between it and an
existing claim.

| addr | size | what it is | twin | twin's source |
| --- | --- | --- | --- | --- |
| `fn_71_3A34` | 0x98 | `rstl::vector`'s `reserve`, 0xC-stride block; copies through `fn_71_3ACC`, no destroy loop | `reserve__Q24rstl48vector<11SConnection,...>Fi` | `src/MetroidPrime/CEntity.cpp` |
| `fn_71_3ACC` | 0x3C | that block's out-of-line `uninitialized_copy`: 3 floats/element, no prologue, returns the advanced `out` | `uninitialized_copy<pointer_iterator<CVector3f>,...>` | `src/Kyoto/Particles/CParticleElectric.cpp` |
| `fn_71_3B08` | 0xB8 | `reserve`, 0x24-stride block; copies through `fn_71_3BC0`, then walks the old range with an empty body before the free | `reserve__Q24rstl59vector<w,CGlyph>,...>Fi` | `src/Kyoto/Text/CRasterFont.cpp` |
| `fn_71_3BC0` | 0x68 | that block's `uninitialized_copy`, one out-of-line element copy (`fn_71_B50`) per element | `uninitialized_copy<pointer_iterator<pair<w,CGlyph>>,...>` | `src/Kyoto/Text/CRasterFont.cpp` |
| `fn_71_3C28` | 0xCC | `reserve`, 4-stride block; element copy **inlined** with a per-iteration destination null test, then an empty destroy walk | `reserve__Q24rstl60vector<CGameArea::ELayerPhase,...>Fi` | `src/MetroidPrime/CGameArea.cpp` |
| `fn_71_3CF4` | 0x50 | `rstl::rc_ptr<rstl::vector<int> >::ReleaseData`: `subic. r0,r3,1 / stw / bgt`, then the object's `__dt__10CModelDataFv` and the refcount's `Free` | `ReleaseData__Q24rstl53rc_ptr<rstl::vector<int>>Fv` | `src/MetroidPrime/main.cpp` |

All six are written as `extern "C"` free functions over typed structs, **not** as `rstl::vector`
template instantiations, and that is forced by the module: its `symbols.txt` names them
`fn_71_*`, and objdiff pairs functions **by name** against that file, so a mangled
`reserve__Q24rstl...Fi` would score against nothing. This is the arrangement
`CIngBlobSwarmVecTail.cpp`, `CPlantScarabSwarmTail.cpp` and `CSandBossRelTail.cpp` already use at
100%, and it is the answer to the item's warning about member twins: the twin's *body* is what
carries over, the twin's *spelling* cannot.

**The twins' own sources were not needed as text.** The shapes are one template - `rstl::vector`'s
`reserve` plus its `uninitialized_copy` helper - and this tree's `include/rstl/construct.hpp:118`
and `include/rstl/algorithm.hpp` already have the statement shapes. What they could not give is
this module's strides and this module's callees, which come from
`build/G2ME01/SnakeWeedSwarm/asm/auto_00_000000DC_text.s`. `CPlantScarabSwarmTail.cpp` is the
closest existing file and three of its six findings carried over unchanged; the ones specific to
this module are below.

The four carve files are all in the change: `config/G2ME01/rels/SnakeWeedSwarm/splits.txt`,
`configure.py` (`Object(Matching, "MetroidPrime/ScriptObjects/CSnakeWeedSwarmVecTail.cpp",
mw_version="GC/2.7")`, one `Object(...)` per line, inside the existing `Rel("SnakeWeedSwarm", [...])`
block), `files.cmake`, and the source's own header comment. Definitions are in descending retail
address order.

## Three spellings that were measured, not guessed

1. **The two iterators are taken by value, in a one-word class with a converting constructor** -
   `CPlantScarabSwarmTail.cpp`'s `SStateIter`/`SIntIter`. That is what produces retail's four
   stores of the two iterator arguments at r1+0x8/0xc/0x10/0x14 and the argument registers
   `addi r3,r1,0x14` / `addi r4,r1,0xc` / `mr r5,r31` in all three `reserve`s. Written over raw
   pointers instead, all four stores and 0x10 of frame go: measured 92.31% on `fn_71_3BC0` and
   the same shape loss on the two callers before this spelling was adopted.
2. **The empty destroy walk must be over a named element type, not `char*`.** Written over
   `char*`, MWCC strength-reduces the empty loop to `subf / mtctr / cmplw / beq / bdnz` and
   `fn_71_3B08` measures **92.50%**; over a 36-byte `struct SState` it emits retail's
   `addi r4,r4,0x24 / cmplw r4,r0 / bne` and the function goes to **100.00%**. The element is
   named only for its size - nothing in this file reads a member of it. Note the 4-stride
   `fn_71_3C28`'s identical empty walk over `int*` never had this problem, so the trigger is
   the pointer type, not the stride.
3. **`mw_version="GC/2.7"` is load-bearing, and only for one function.** Under the module default
   GC/1.3.2 the six compile to 4 or 5 of 6 at 100% with `fn_71_3BC0` at **92.31%** (24 of its 26
   words) - 1.3.2 hoists `lwz r31,0x0(r3)`, the load of `begin`, to *after* `mr r29,r4` where
   retail has it before. Switching that one `Object(...)` to GC/2.7 and changing nothing else
   gives **6 / 6 at `fuzzy 100.000`**. The same knob and the same finding are recorded for
   `CPlantScarabSwarmTail.cpp` (its `fn_49_2D40`) and `CLumiteRelTail.cpp` (`fn_39_778`), so this
   is a property of these module-tail copies rather than of this module.
   For the record, `fn_71_3BC0` was also tried as: raw pointers with `last` hoisted (82.12%), raw
   pointers re-reading `end.mCur` each iteration (88.46%), the same with `cur`/`dst` declared in
   the other order (92.31%), and `SStateIter` carrying `operator!=`/`operator++`/`operator*`
   with the loop in `construct.hpp:118`'s exact shape (92.31%). The last one is what shipped; the
   2.7 switch is what closed the last two words.

## What did NOT get claimed, and why

`fn_71_B50` (0xB50, 0x54) is **left retail**, and it is the element copy `fn_71_3BC0` calls -
`cmplwi dest,0 / beqlr` and then 0x24 bytes of member-by-member float copy. It is not in this
run (it is at 0xB50, not adjacent to 0x3A34..0x3D44), so claiming it would need a second
discontiguous range, which the recipe forbids. It needs no `force_active:` entry: it is called
from a function this claim owns, and `CIngBlobSwarmVecTail.cpp` records the measurement that a
kept call chain is enough.

No `force_active:` entry was needed for this module either, and that is measured, not assumed:
all six functions are referenced from the module's own data and code (`fn_71_133C` calls all
three `reserve`s; `fn_71_3CF4` has seven `bl` sites), and the `.rel` is `cmp`-equal to retail
with no `force_active:` list added to `config/G2ME01/config.yml`.

## Gates

- `python3 tools/check_symbol_names.py` - `checked 585 units; 0 declared names are missing`
- `python3 tools/check_files_cmake.py` - `every configured DOL object is either in files.cmake or
  excluded with a reason`; the file is listed and its host branch is empty by design (bodies are
  inside `#ifdef __MWERKS__`), the arrangement `CPlantScarabSwarmTail.cpp` uses, because
  `fn_71_B50` is this module's own unclaimed middle and a flat host link does not have it.
- `python3 tools/check_decl_order.py --unit MetroidPrime/ScriptObjects/CSnakeWeedSwarmVecTail.cpp`
  - `ok: 0 unit(s) checked` (it does not resolve a path out of a `Rel(...)` block), so the
  descending declaration order is verified by the module's sha1 instead, as
  `CPlantScarabSwarmTail.cpp` records.
- `python3 tools/check_raw_offsets.py` - `ok: 199 raw-offset site(s) in 84 file(s), all documented
  in raw_offsets.md`. **This file adds none**: every member access goes through a named struct
  field, so no `docs/research/raw_offsets.md` change was needed (unlike `progress-twin-rel-sporb`,
  which needed a new section).
- `python3 tools/check_docs_claims.py` - reports three claims disagreeing with the tree. Measured
  against `build/report.base.json` on the **clean** tree, two of the three are already failing
  (`**147 units of our own code in 80 modules**` and the port probe's `877 files`); the third,
  `REL units   1865 / 11739 functions`, is the HANDOFF state-block count this change moves, and
  the judge rewrites that block from the tree. `gate.sh` inside `goal_check.sh` passes it.
- `tools/check_module_wiring.py`, `tools/gen_module_order.py --check`, `tools/probe_sources.sh`,
  the port link and the 86 REL hashes: run by `tools/gate.sh` inside the judge, all green.
- No `asm` added; the only new lines under `src/` are C++ and comments.

## Note for the next run on this module

The unclaimed middle still holds ~50 functions between `0xDC` and `0x3A34`. The other runs from
the item's twin list that are still open, longest first, all sit in the same `rstl`-shaped family
and were not attempted here: `0xF84..0x10EC` (4 adjacent, two `single_ptr`/vector teardowns and
a `vector<i>` destructor), `0x372C..0x3834` (3 adjacent, `CStreamAudioManager`/`CScannableObjectInfo`
destructors), `0x29C0..0x2B84` (4 adjacent, the same two-word teardown shape four times over),
then the three singles `0x1574..0x163C`, `0x26FC..0x274C`, `0xB18..0xB50`. **The destructor
family (`0x29C0..0x2B84`, `0x372C..0x3834`) is the cheapest next step**: those are the shape
`progress-twin-rel-sporb`'s `SporbDtors.cpp` already reproduces at 100% with no source reading at
all, and four of them are adjacent.

One structural fact this item re-confirms rather than discovers, since it cost the lane before:
**a REL claim has to be `Matching` before the module's hash check means anything** (a
`NonMatching` object contributes retail bytes wearing the unit's name), and **the module's copy
of the object goes stale silently** - `build/G2ME01/config.json` has to be removed and the build
re-run for the split edge to re-fire after editing the source. Both are recorded in
`progress-twin-rel-sporb`'s notes.