# progress-prime1-cpausescreen

`kind: progress`, target `MetroidPrime/CPauseScreen` (unit stays `NonMatching`; `flip_test.sh` not
run, as the item says).

## Result

`matched_functions` for `main/MetroidPrime/CPauseScreen`: **4 -> 6** (of 84). Project-wide
`matched_functions` **9977 -> 9979**. No function anywhere got worse (measured by diffing every
`(unit, function)` fuzzy-match percentage in `build/report.base.json` against `build/report.json`:
0 worse, 2 better).

## What I changed

One file, `src/MetroidPrime/CPauseScreen.cpp`, two lines: the `looped` argument of the two
`CSfxManager::SfxStart` calls in `SetPanSound` and `SetZoomSound` went from `true` to `false`.

## Per function, before % -> after %

| function | before | after | note |
|---|---|---|---|
| `SetPanSound__12CPauseScreenFb` | 99.97 | **100.00** | matched |
| `SetZoomSound__12CPauseScreenFb` | 99.97 | **100.00** | matched |
| every other function in the unit | unchanged | unchanged | 0 worse, verified |

## Prime 1's source was not usable for this unit

The item's `reason` points at Prime 1's `src/MetroidPrime/CPauseScreen.cpp` as a reference. It is a
**different class**, not an older version of the same one:

- Prime 1's `CPauseScreen` is the pause-subscreen container. Its constructor takes
  `(int subscreen, const CDependencyGroup& suitDgrp, const CDependencyGroup& ballDgrp)` and it owns
  `rstl::auto_ptr<CPauseScreenBase> mScreens[2]`, `mStrgPauseScreen` ("STRG_PauseScreen") and a
  `FRME_PauseScreen` frame. It switches between LogBook / Options / Inventory.
- Echoes' `CPauseScreen` is the scan-network screen. Its constructor takes no arguments, `CHECK_SIZEOF`
  is 0x56c, and it owns `CScanTree mScanTree`, eleven model slots, twelve history rows and a
  `CQuitGameScreen`.

The four names the seeder paired across the two (`InitializeFrameGlue`, `CheckLoadComplete`, `Draw`,
`__dt__`) are name collisions between unrelated functions, and the seeder itself measured **0 with the
same size**. So there was no source to adapt; I worked from the target's own disassembly instead.

## The two matches, and how they were found

Both were at 99.97% - one 4-byte instruction away. objdiff's per-function JSON has `left` = retail
and `right` = our build (verified by disassembling both objects directly, not assumed).

```
=== SetPanSound__12CPauseScreenFb  ours=(...,'144',99.97)  retail=(...,'144',99.97)
**   17 retail=li r9, 0x0   ours=li r9, 0x1
```

`r9` is `SfxStart`'s 6th argument, `bool looped`
(`include/Kyoto/Audio/CSfxManager.hpp:195`). Retail passes `0`. The other 35 instructions were
already identical, so this was the whole difference. Same for `SetZoomSound`.

**Lesson (general, not GameCube-specific):** when two functions sit at 99.9%, do not read them as
"nearly done, probably a register-allocation detail". Filter the diff to instructions that are *not*
branches and not symbol names, and the residue is almost always a single wrong literal. Branch
targets differ in every such function and are pure address noise.

## What is blocked, with the evidence

### `__dt__12CPauseScreenFv` - 98.38%, and the remaining 1.6% is not source-tunable

The destructor is a compiler-generated member teardown. Three real differences remain, none of which
a source edit reaches:

1. **Two outlined teardowns.** Retail calls `fn_80038F0C` and `fn_8020D820`; we emit weak inline
   copies of `~rstl::vector<CLight>` and `~rstl::list<rstl::pair<bool,TToken<CTexture>>>`. Retail's
   linker kept a different copy. Same shape as the `construct.hpp` comment at line 65 ("Declared
   before its inline definition ... so an out-of-line copy is weak rather than local") - that trick
   was applied to `destroy_impl` but these two instantiations still land differently.
2. **`x1fc_` element type.** Retail destroys `__dt__Q24rstl36vector<i,...>` (`vector<int>`); we emit
   `vector<unsigned>`. The header has `rstl::vector<CAssetId> x1fc_` and `CAssetId` is `typedef uint`
   (`include/Kyoto/SObjectTag.hpp:10`). Changing the member's type to `int` is a real fix but it
   changes a class layout, which the item forbids without a measured diff at every use, and `x1fc_`
   has no user in the unit to confirm against.
3. **`mModelTokens` teardown.** Retail loads each `optional_object<CToken>` and calls
   `__dt__6CTokenFv` with `r4 = -1` and no null test; we emit a `cmplwi`/`beq` null guard and `r4 = 0`.
   This is `optional_object`'s destructor shape (`include/rstl/optional_object.hpp:22`, whose
   `clear()` is deliberately commented out with the note "Makes ~CScriptHudMemo match"). Changing it
   here would move every other `optional_object` user.

I did not file a `NEW:` for this: the blocker is three independent codegen questions, not one
reachable function, and none of the three is a single spelling away.

### The three functions the item named

`InitializeFrameGlue` (0.14%, 2940 bytes), `CheckLoadComplete` (0.15%, 3848 bytes) and
`Draw` (0.26%, 1512 bytes) are `// TODO:` stubs in this repo. Their retail bodies are 1-4 KB each and
reference `CTweakGui` accessors, `CScanTree`, the GUI widget tree and the model pipeline. Prime 1's
versions of the same three names are unrelated code (see above), so they give no starting point. This
is a several-day job per function, not one item.

### `SetFog` (2.70%, 148 bytes) - fully decoded, but its callees are unnamed

This one I *could* read completely off the disassembly, and the body is short:

```
if (enabled) {
  CColor& c = <fn_802161C8>();   // lwz r3,0(r3); addi r3,r3,0x5a8  -> a CColor at tweak+0x5a8
  float startz = <fn_802161D4>(); // lfs f1,0x5a4(r3)
  float endz   = <fn_802161E0>(); // lfs f1,0x5a0(r3)
  CGraphics::SetFog(kRFM_PerspRevExp, endz, startz, c);   // li r3,0x2
} else {
  CGraphics::SetFog(kRFM_None, <lbl_8041D794>, <lbl_8041D794>, CColor::Black());  // li r3,0x0
}
```

The blocker is the three accessors. `fn_802161C8/D4/E0` live in `main/auto_03_80213CB8_text` and have
**no GC name**; that object has 30 named `CTweakGui` accessors but not these three, and
`src/MetroidPrime/Tweaks/` has no `CTweakGui::` definition at all - `CAutoMapper.cpp:1723` already
calls `GetMapBackgroundCycleTime()` as an undefined symbol that resolves into that auto unit. So
writing this body means inventing three names in a unit that is `NonMatching` and unnamed, i.e.
guessing identifiers the linker cannot check. That is the "plausible-looking stand-in" the brief
warns about, so I left it alone and recorded it instead.

## Gates (all re-run after the change)

```
sha1sum build/G2ME01/main.dol   -> 6ef9b491d0cc08bc81a124fdedb8bfaec34d0010   (expected)
./tools/probe_sources.sh        -> 749 files, 0 failed, 0 errors; LINKED (250 undefined, 0 duplicates)
python3 tools/check_symbol_names.py -> 503 units, 0 missing
./tools/decomp_build.sh         -> All: 30.73% fuzzy, 22.92% matched, 11.74% linked (9979/28465)
all 86 RELs                     -> cmp-identical to orig/G2ME01/files/RelProd/
```

`All:` did not fall (9977 -> 9979 matched functions). The diff adds no `asm`, and no initialisation
was removed.

## Notes for the next run

- The helper I used is at `.tmp/opencode/od.py` (gitignored, not part of the diff). It runs
  `objdiff-cli diff -p . -u main/MetroidPrime/CPauseScreen -o - <symbol>` and prints a
  retail-vs-ours side-by-side. **Left is retail, right is ours** - the opposite of the usual
  intuition, and getting it backwards sends you after phantom bugs.
- Retail symbols for unnamed functions can be disassembled straight out of
  `build/G2ME01/obj/<unit>.o`; the DOL itself is not objdump-readable (`file format not
  recognized`), so use the split object.
- Two lines of the 84 functions in this unit are within one instruction of 100% and both are now
  taken. The next cheapest candidates by size are `fn_80205BE4` (32 B), `fn_8020AFFC` (40 B),
  `fn_80205C04` (40 B) - but all are unnamed retail functions with no declaration to write against.
