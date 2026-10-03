# reclaim-scriptcoin-source-less-rel-units — ScriptCoin (module 58), `match` on `ScriptCoin/MetroidPrime/ScriptObjects/CScriptCoinRest`

**Result: the unit is `Matching`, 2/2 functions, module sha1 held. `goal_check.sh` PASS, exit 0.**

## The blocker was the *name*, not the code

The item's reason says the claim "cannot be split" and that 24 functions are unreachable. Both are
half right, and the half that was wrong is why the item could not be worked at all:

`configure.py:3001` declared the unit **unqualified**:

```
Object(NonMatching, "MetroidPrime/ScriptObjects/CScriptCoinRest.cpp"),
```

but the queue's target is **module-qualified**, `ScriptCoin/MetroidPrime/ScriptObjects/CScriptCoinRest`.
`tools/goal_check.sh:173-181` resolves a `match` target by searching `configure.py` for
`Object(<state>, "<target>.cpp")` and `<target>.cp` / `<target>.c`. Run against this tree it returns
nothing, so the judge takes the `note "match target ... has no Object(...) entry in configure.py"`
branch and **fails the item before it looks at any code** — no flip, no partial path, nothing an
agent writes to `src/` can change that. Measured, the resolution as the judge computes it:

```
$ python3 - "ScriptCoin/MetroidPrime/ScriptObjects/CScriptCoinRest"   # goal_check.sh:173-181 verbatim
resolved: []
```

`IngBoostBallGuardian` is the tree's existing answer: all eleven of its carved units are declared
**and** named in `splits.txt` as `IngBoostBallGuardian/MetroidPrime/ScriptObjects/...cpp`, with
`source=` carrying the real path. I followed that arrangement. **This is the generalisable finding:
a `match` item whose target names a REL unit has to be written module-qualified in `configure.py`
*and* `splits.txt`, or it is unjudgeable.** A `progress` item on `module:<M>` escapes it (the judge
sums over `units` starting with `<M>/`), which is why eleven sibling items never noticed.

## What I measured first

`build/goal/judge/report.base.json` vs `build/report.json`, summed over units named `ScriptCoin/…`
(the judge's own definition for a `module:` target):

```
baseline  units=7  matched=6  total=37
   CScriptCoinRel 4/4 complete   CScriptCoin 1/1 complete   CScriptCoinTouchBounds 1/1 complete
   CScriptCoinRest 0/17          CScriptCoinThink 0/2      CScriptCoinTail 0/5
   auto_00_000000A0_text 0/7
```

Headline: `matched 13470 -> 13472`, `linked 6518 -> 6520`, `All: 13472 / 28465 functions`.
`total_functions` **28465 -> 28465**: the module still has 37 text symbols and
`preplf 37 text symbols, plf 37, 0 dropped by -strip_partial`. No unit outside `ScriptCoin/` changed.

The reason's "only a stale `.o` keeps the module hashing" is not what is happening. The module hashes
**from `config.yml`**: `build/G2ME01/ScriptCoin/ScriptCoin.rel` =
`06fbf0e2b013de05a334008cb76e08d6843f4988`, which is what `config/G2ME01/config.yml` records, and it
did so before I touched anything. A `NonMatching` entry with no source is the arrangement
"Why the `NonMatching`-with-no-source trick is legal" (`RUNNING_THE_DECOMP.md:308`) prescribes:
`dtk` fills the claimed range from retail, so the claim was never what kept the module alive. What it
did keep alive was **17 functions in one unnameable unit**, which is the part worth fixing.

## The claim: one old range, three units

`0x1BA4..0x36A4` was 17 functions in a single unit. A unit cannot claim two discontiguous ranges
(`dtk dol split` dies with a link-order cycle), so the range became three contiguous units. The two
no-source ones are legal and keep retail's bytes; the third is ours.

| unit | range | state |
|---|---|---|
| `MetroidPrime/ScriptObjects/CScriptCoinRestHead.cpp` | `.text 0x1BA4..0x32B8`, `.rodata 0..0x68`, `.data 0..0xD8` | `NonMatching`, no source |
| **`ScriptCoin/MetroidPrime/ScriptObjects/CScriptCoinRest.cpp`** | **`.text 0x32B8..0x3374`** | **`Matching`, source written** |
| `MetroidPrime/ScriptObjects/CScriptCoinRestTail.cpp` | `.text 0x3374..0x36A4` | `NonMatching`, no source |

The `.rodata`/`.data` claims move with the head unit: **a `Matching` unit's claim must be exactly
what its own object reproduces**, and `src/…/CScriptCoinRest.cpp` emits no `.rodata` and no `.data`.
Leaving them on the Matching unit would take 320 bytes out of the link and break the sha1.

`audit_rel_claim.py ScriptCoin` confirms every claim is honest — 10/10, 2/2, 5/5 functions against
the objects dtk emitted, `0 claim(s) with a problem`.

## The two functions

`0x32B8 fn_58_32B8` (0x40) and `0x32F8 fn_58_32F8` (0x7C) are `rstl::reserved_vector<float, 8>`'s
fill-all constructor and its `resize`. The bodies are **`fn_800D02F8`/`fn_800D0338` in
`src/MetroidPrime/Player/CMorphBall.cpp:245-264` unchanged** — that file's
`reserved_vector<float, 15>` pair is the same two statements over the same 4-byte element, and it
documents every spelling that matters: `resize` spelled out rather than called (calling it emits a
weak outlined instantiation and leaves a forwarder objdiff cannot pair), the fill as
`rstl::uninitialized_fill_n` rather than a memberwise loop, `data() + count` for the
`(self + count*4) + 4` base, and **the value by pointer, not by value** (retail's `lfs f0,0(r5)`
dereferences `r5`).

**`mw_version="GC/2.7"` is load-bearing.** Under the module's default `GC/1.3.2`, `fn_58_32F8` keeps
`r6` for both the remaining count and the cursor (`or r7,r0,r0` copies the count out, then
`stfs f0,0(r6)`..`+0x1c` fills through it) and hoists `lfs f0,0(r5)` above the cursor arithmetic,
pushing `beq .tail` 0x14 further out than retail's and widening every displacement in the function.
2.7 keeps the cursor in `r5` where retail has it. Same per-object override
`CIngBoostBallGuardianF78.cpp` uses.

## The two 0x3C destructors are still not claimable

`fn_58_327C` (0x327C) and `fn_58_3374` (0x3374) sit immediately either side of this pair and are
**referenced by nothing in the module** — the `FORCEACTIVE` block in
`build/G2ME01/ScriptCoin/ldscript.lcf` lists 11 text symbols and neither is among them. Nothing in
the module calls them, so mwldeppc dead-strips a `Matching` object that defines them and the module
comes out short. They stay in the head and tail claims. This is structural fact 3 in the module
recipe, re-measured here; the earlier lane (`progress-scriptcoin-unreferenced-dtors`) recorded that
`force_active:` for both still leaves the `.rel` 32 bytes long, so the cause is not yet understood.

## Gates, as the judge ran them

```
$ ./tools/goal_check.sh build/goal/item.json
goal_check: item reclaim-scriptcoin-source-less-rel-units (match) target=ScriptCoin/MetroidPrime/ScriptObjects/CScriptCoinRest
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 13470 -> 13472   linked 6518 -> 6520
  ok    check_symbol_names.py
  ok    All:  37.55% fuzzy, 30.98% matched, 13.85% linked (13472 / 28465 functions)
  ok    flip_test ScriptCoin/MetroidPrime/ScriptObjects/CScriptCoinRest.cpp: PASS, Object(Matching) in configure.py
goal_check: PASS reclaim-scriptcoin-source-less-rel-units
```

Plus, measured directly:

```
sha1sum build/G2ME01/main.dol              6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
86 REL hashes vs config/G2ME01/config.yml  86 match, 0 differ
./tools/unit_fit.sh …/CScriptCoinRest.cpp  .text claimed 188 ours 188 retail 188 fits
                                          no extra functions
./tools/flip_test.sh …/CScriptCoinRest.cpp PASS -> kept as Matching  (kept: 1/1 failed: 0 skipped: 0)
python3 tools/check_symbol_names.py        checked 585 units; 0 declared names are missing
python3 tools/check_decl_order.py          ok: 1188 units checked, 37 permuted, all accounted for
python3 tools/check_files_cmake.py         every configured DOL object is in files.cmake or excluded
python3 tools/check_module_wiring.py       174 units in 80 modules; 8 reserved ranges
python3 tools/audit_rel_claim.py ScriptCoin 0 claim(s) with a problem
                                            preplf 37 text symbols, plf 37, 0 dropped by -strip_partial
```

`build/report.json` reads `ScriptCoin/ScriptCoin/MetroidPrime/ScriptObjects/CScriptCoinRest`:
`m=2 t=2`, `complete: true` — the doubled prefix is the module name the report puts in front of the
qualified unit path, the same shape `IngBoostBallGuardian/IngBoostBallGuardian/…` already has.

`docs/HANDOFF.md` and `docs/RUNNING_THE_DECOMP.md` show as modified in `git status`; those are
`gate.sh`'s own writes (`MP_GATE_DOCS_WRITE=1` inside `goal_check.sh`) and are not mine.

## What is still open in this module

29 functions of 37 unmatched. `CScriptCoinRestHead` (0x1BA4..0x32B8, 10) and `CScriptCoinRestTail`
(0x3374..0x36A4, 5) are now nameable, so a `match` item on either can be written; `CScriptCoinThink`
(2), `CScriptCoinTail` (5) and `auto_00_000000A0_text` (7) are unchanged by this item. **This item
unblocks them only for the two ranges it split out** — the other three units were already
nameable and nothing about them changed.

Note for whoever picks up the tail: the review queue's `progress-scriptcoin-fn58-35d0-rodata-58`
wants `fn_58_35D0` (0x35D0..0x36A4, in `CScriptCoinRestTail` now) and needs `.rodata 0x58..0x68`,
which still sits on `CScriptCoinRestHead`. That split is now a two-line change to a claim that has a
name.

## Config changes, as a list

- `config/G2ME01/rels/ScriptCoin/splits.txt`: `MetroidPrime/ScriptObjects/CScriptCoinRest.cpp`
  `0x1BA4..0x36A4` (+ its `.rodata 0..0x68` and `.data 0..0xD8`) replaced by three entries —
  `MetroidPrime/ScriptObjects/CScriptCoinRestHead.cpp` `0x1BA4..0x32B8` with the `.rodata`/`.data`,
  `ScriptCoin/MetroidPrime/ScriptObjects/CScriptCoinRest.cpp` `0x32B8..0x3374`,
  `MetroidPrime/ScriptObjects/CScriptCoinRestTail.cpp` `0x3374..0x36A4`. The other four entries are
  unchanged.
- `configure.py`: the one `CScriptCoinRest` entry replaced by `CScriptCoinRestHead` (`NonMatching`,
  no source), `ScriptCoin/MetroidPrime/ScriptObjects/CScriptCoinRest.cpp` (`Matching`,
  `source="MetroidPrime/ScriptObjects/CScriptCoinRest.cpp"`, `mw_version="GC/2.7"`) and
  `CScriptCoinRestTail` (`NonMatching`, no source). `CScriptCoinRel`, `CScriptCoin`,
  `CScriptCoinThink`, `CScriptCoinTouchBounds` and `CScriptCoinTail` are unchanged.
- `files.cmake`: added `src/MetroidPrime/ScriptObjects/CScriptCoinRest.cpp`.
- `src/MetroidPrime/ScriptObjects/CScriptCoinRest.cpp`: new, the two bodies.
- `config/G2ME01/config.yml`: **no change** — no `force_active:` entry was needed, because both
  functions are reachable from the module's own retail bytes (`fn_58_32B8` from `fn_58_28AC` at
  0x2D8C, `fn_58_32F8` from `fn_58_32B8`).

NEW: reclaim-scriptcoin-rest-head-fn | match | ScriptCoin/MetroidPrime/ScriptObjects/CScriptCoinRestHead | .text 0x1BA4..0x32B8 (10 functions) is now a nameable NonMatching-no-source unit, so a match item can claim a subrange of it - fn_58_1BA4 (0x94) and fn_58_27D0 (0xDC) are both in the module's FORCEACTIVE list and are the reachable ones to start on