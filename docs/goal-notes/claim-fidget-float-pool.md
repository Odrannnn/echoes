# claim-fidget-float-pool — PASS 2026-09-30 (lane 6)

**No source change was needed. This item is already satisfied by `afb51fb`.** The judge says so:

```
$ ./tools/goal_check.sh build/goal/item.json
goal_check: item claim-fidget-float-pool (match) target=MetroidPrime/Player/CFidget
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 9938 -> 9938   linked 4896 -> 4896
  ok    check_symbol_names.py
  ok    All:  30.61% fuzzy, 22.73% matched, 11.74% linked (9938 / 28465 functions)
  ok    flip_test MetroidPrime/Player/CFidget.cpp: PASS, Object(Matching) in configure.py
  goal_check: PASS claim-fidget-float-pool
```

`sha1sum build/G2ME01/main.dol` -> `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`.

## Why the item is a stale re-file, not work

`docs/goal-notes/match-cfidget.md:182` filed this item as a `NEW:` line in **run 1** of
`match-cfidget` - whose own text says so: *"the fix for this unit is in the delivered diff, so the
general shape of the problem (which other units' pools are unclaimed) is what a follow-up should
sweep"*. The fix landed in that same commit, `afb51fb match: match-cfidget` (5 files, 2026-09-29):

- `config/G2ME01/splits.txt:962-964` - CFidget's claim gained `.sdata2 start:0x8041D0C8 end:0x8041D0F0`
- `configure.py:481` - `Object(NonMatching -> Matching, "MetroidPrime/Player/CFidget.cpp")`

`match-cfidget` run 2 then passed its own judge. So the blocker this item names was gone before the
item was dispatched. Measured on this tree:

- `configure.py:481` reads `Object(Matching, "MetroidPrime/Player/CFidget.cpp")`
- `build/report.json` `main/MetroidPrime/Player/CFidget`: `fuzzy_match_percent 100.0`,
  `matched_functions 4 / total_functions 4`, `metadata.complete: true`
- `./tools/flip_test.sh MetroidPrime/Player/CFidget.cpp` -> `PASS -> kept as Matching`
- `build/G2ME01/obj/MetroidPrime/Player/CFidget.o` now carries `.text 0x2d0` **and** `.sdata2 0x28`
  (it carried no `.sdata2` at all before the re-split - that was the whole blocker)

I did not "fix" anything, because there was nothing to fix. A `match` item in
`tools/goal_check.sh` has no `CODE_CHANGED` requirement (only `progress` and `port` have one), so
an already-`Matching` target is a legitimate pass.

## The sweep the reason asked for - and a false positive that cost me most of this item

I built the detector the reason describes: for every retail object, take the *data* relocations
(`R_PPC_EMB_SDA21`, `ADDR16_HA/LO`, `ADDR32`, ... - not `REL24`/`BR24`, which are calls), keep the
ones naming a local `lbl_XXXXXXXX` label, and test the address against every section claim in
`config/G2ME01/splits.txt`. That is 147 main-DOL units whose data relocations hit an address no
**named** claim covers, and at the top of it by `fuzzy_match_percent` were exactly the CFidget
signature: `CStateMachineFactory` and `CAnimTreeDoubleChild`, both 100.0% and both `NonMatching`.

**That test is wrong, and here is why - do not repeat it.** dtk generates **`auto_*` data-only
units** for the gaps, and they are *not* in `splits.txt`; they appear only in `build/report.json`.
There are 1065 of them on this tree (`bss` 94, `text` 562, `data` 142, `rodata` 127, `sdata2` 68,
`sdata` 27, `sdata2`/`sbss` 43, `sbss2` 1, `init` 1), and they own the ranges my detector called
unclaimed. Every one of the three top hits is inside one:

| probe | "unclaimed" by splits.txt | actually inside |
|---|---|---|
| `0x803AA230` | `.rodata` gap `0x803A9F4C..0x803AA590` (0x644) | `main/auto_06_803A9F4C_rodata` = `0x803A9F4C + 0x644` = `0x803AA590` |
| `0x80418A78` | `.sdata` gap `0x80418A30..0x80418AA0` (0x70) | `main/auto_09_80418A30_sdata` = `0x80418A30 + 0x70` = `0x80418AA0` |
| `0x8041E378` | `.sdata2` gap `0x8041E360..0x8041E3A0` (0x40) | `main/auto_11_8041E360_sdata2` = `0x8041E360 + 0x40` = `0x8041E3A0` |

CFidget's own leftover, `main/auto_11_8041D0F0_sdata2` = `0x8041D0F0 + 0x2c` = `0x8041D11C`, starts
*after* the new claim, which is what "the re-split carves the range out of the auto unit" looks
like in `report.json`. **So: "no `splits.txt` entry claims this address" is not a blocker. A
`splits.txt` entry claiming it *and our object reproducing it there* is.** `docs/HANDOFF.md:325`
already says the mechanism - "everything else unclaimed so `dtk` fills it from retail" - and
`HANDOFF.md:51,55` record the trap being paid for twice before CFidget (`CAnimTreeNode`'s
placement-new string, `CSoundPOINode`'s vtable).

## What actually blocks the units that are at 100% - the real finding

The genuinely useful sweep is not "whose pool is unclaimed" but **"which units sit at 100.0% and
still cannot flip"**, because those are stuck units whose matched count is real and unbankable.
`build/report.json` has **12** units at `fuzzy_match_percent 100.0` that `configure.py` still has
`NonMatching`. Applying the `unit_fit.sh` rule without a build - compare the symbol tables of
`build/G2ME01/src/<unit>.o` (ours) with `build/G2ME01/obj/<unit>.o` (retail) - splits them cleanly:

**Six emit functions retail does not define, and their `.text` is longer by exactly that code.**
This is the rule in `docs/goal-unit-prompt.md`: "a unit whose object emits functions the retail
object does not define can never be `Matching`, however good it looks".

| unit | report | `.text` ours / retail | extra functions | flip test |
|---|---|---|---|---|
| `MetroidPrime/ScriptObjects/CScriptCameraWaypoint` | 5 fns | 472 / 428 | 1 | already queued `match-cscriptcamerawaypoint` |
| `MetroidPrime/BodyState/CABSFlinch` | 6 fns | 1408 / 800 | 19 | already queued `match-cabsflinch` |
| `MetroidPrime/Factories/CStateMachineFactory` | 9 fns | 1476 / 1072 | 4 | **measured FAIL** (below) |
| `Kyoto/Animation/CAnimTreeDoubleChild` | 18 fns | 5452 / 4216 | 11 | **measured FAIL** (below) |
| `Kyoto/Animation/CCharLayoutInfo` | 28 fns | 7084 / 4784 | 22 | already queued `match-ccharlayoutinfo` |
| `Kyoto/Particles/CParticleGen` | 3 fns | 540 / 228 | 7 | **measured FAIL** (below) |

The extras are overwhelmingly **destructors and template ctors that retail does not emit** -
`__dt__4IObjFv`, `__dt__31CObjOwnerDerivedFromIObjUntypedFv`,
`__dt__Q24rstl15auto_ptr<4IObj>Fv`, `__dt__Q24rstl23rc_ptr<13CAnimTreeNode>Fv`,
`__dt__Q24rstl66basic_string<...>Fv`, `__dt__Q24rstl430red_black_tree<...>Fv` and so on. Retail's
objects simply do not define them, so declaring them costs us bytes the unit has no room for. For
`CStateMachineFactory` the arithmetic is exact: 4 extra dtors = 0x180 = 384 bytes, and
`1476 - 1072 = 404` (the remaining 20 is alignment). That is the whole `.text` excess, so the extra
functions - not a pool - are the blocker.

### The three flip tests I ran (all reverted cleanly)

```
$ ./tools/flip_test.sh MetroidPrime/Factories/CStateMachineFactory.cpp
    build failed:
      FAILED: [code=1] build/G2ME01/ok
      build/G2ME01/main.dol: FAILED
  FAIL  -> reverted (tree rebuilt: DOL 6ef9b491d0cc08bc81a124fdedb8bfaec34d0010)

$ ./tools/flip_test.sh Kyoto/Animation/CAnimTreeDoubleChild.cpp
    build failed:
      FAILED: [code=1] build/G2ME01/ok
      build/G2ME01/main.dol: FAILED
      build/G2ME01/AIMannedTurret/AIMannedTurret.rel: FAILED
      ... (8 RELs)
  FAIL  -> reverted (tree rebuilt: DOL 6ef9b491d0cc08bc81a124fdedb8bfaec34d0010)

$ ./tools/flip_test.sh Kyoto/Particles/CParticleGen.cpp
    build failed:
      FAILED: [code=1] build/G2ME01/main.elf
      ### mwldeppc.exe Linker Error:
      Errors caused tool to abort.
  FAIL  -> reverted (tree rebuilt: DOL 6ef9b491d0cc08bc81a124fdedb8bfaec34d0010)
```

Two of the three fail on `build.sha1` (the DOL moves, because our `.text` is longer), one fails at
the **link** instead - a different, louder symptom of the same extra symbols. All three left the
tree exactly as they found it: `git status --short` empty, DOL sha1 back to retail's.

## A third class: `.text` exactly equal, no extra functions, still cannot flip

`MetroidPrime/Tweaks/CTweakAutoMapper` is 68/68 at 100.0%, its `.text` is **exactly** retail's
(1036 = 1036), it emits **no** function retail does not define - and it still cannot flip:

```
$ ./tools/flip_test.sh MetroidPrime/Tweaks/CTweakAutoMapper.cpp
      build/G2ME01/AIMannedTurret/AIMannedTurret.rel: FAILED
      ... (8 RELs, plus main.dol)
  FAIL  -> reverted (tree rebuilt: DOL 6ef9b491d0cc08bc81a124fdedb8bfaec34d0010)
```

So neither "extra functions" nor "unclaimed data" explains it, and I did not find what does. That
makes it a **third** blocker class, distinct from the six above, and 68 bankable functions sit
behind it. `Kyoto/Graphics/CLight` (19 fns, `.text` 1604 = 1604) and `Kyoto/Math/CQuaternion`
(22 fns, `.text` 5684 = 5684) are in exactly the same measured position and are **not** queued and
**not** flip-tested by me. `MetroidPrime/CMemoryDrawEnum`,
`Kyoto/Particles/CParticleSpawnRandom` and `Kyoto/Particles/CParticleSpawnSystem` report no
functions at all. I stopped there: 109 functions behind an unexplained wall is worth a lane, but
it is not this item's work and I will not file a `NEW:` whose reason I have not measured.

## NEW

Three of the six blocked units are not in the 81-item queue, and all three are real units whose fix
raises `linked` by 9, 18 and 3 functions respectively. Their blockers are measured, not guessed.

NEW: match-cstatemachinefactory-extras | match | MetroidPrime/Factories/CStateMachineFactory | 9/9 at 100% but flip FAILs on build.sha1; our object emits 4 dtors retail does not define (__dt__4IObjFv, __dt__31CObjOwnerDerivedFromIObjUntypedFv, __dt__Q24rstl15auto_ptr<4IObj>Fv, __dt__Q24rstl53auto_ptr<41TObjOwnerDerivedFromIObj<13CStateMachine>>>Fv) making .text 1476 vs retail 1072

NEW: match-canintreedoublechild-extras | match | Kyoto/Animation/CAnimTreeDoubleChild | 18/18 at 100% but flip FAILs on build.sha1; our object emits 11 functions retail does not define (CAnimTreeNode virtuals, rc_ptr/dtors, CAnimTreeEffectiveContribution copy ctor) making .text 5452 vs retail 4216

NEW: match-cparticlegen-extras | match | Kyoto/Particles/CParticleGen | 3/3 at 100% but the LINK fails (mwldeppc Linker Error); our object emits 7 functions retail does not define (GetDrawFlags, GetGeneratorRate, SetDrawFlags, SetGeneratorRate, ShouldDraw, ~CParticleGen, ~list<CWarp>) making .text 540 vs retail 228

## Lessons for the next lane

1. **"No `splits.txt` entry claims this address" is not a blocker.** dtk's 1065 `auto_*` units own
   the gaps and are invisible in `splits.txt`. Test coverage against `build/report.json`'s
   sections, not against the split file - or just run `flip_test.sh`, which is the authority.
2. **A unit at 100.0% that will not flip is usually extra functions, not extra data.** Check
   `nm` on `build/G2ME01/src/<unit>.o` against `build/G2ME01/obj/<unit>.o` first; the `.text` size
   delta is the giveaway, and it should equal the sum of the extra functions' sizes.
3. **A detector that finds 147 hits and ranks two known-fine units at the top is measuring the
   wrong thing.** The top of that list was wrong in every case I checked. Verify a new detector
   against a unit whose correct answer is already known before believing its output - CFidget
   itself should have dropped out of the list, and the ranges it flagged all turned out to be
   owned by auto units.
4. **`.text` size equal and no extra functions is not sufficient to predict a flip.**
   `CTweakAutoMapper` is the counter-example, and it is why I did not file a `NEW:` for `CLight`
   and `CQuaternion` despite their identical measured shape: I have a counter-example, so the
   rule "looks flippable" does not fire, and guessing a reason would put a wrong item in the queue.

Scratch scripts used for the sweep were left in `.tmp/opencode/` (gitignored, not in the diff):
`pool_gap.py` (the splits.txt-coverage detector, kept for the record despite being wrong),
`why_not_flip.py` (the `unit_fit` symbol-table comparison, which is the one that paid off).
