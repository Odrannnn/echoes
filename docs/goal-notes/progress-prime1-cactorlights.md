# progress-prime1-cactorlights

`kind: progress`, `target: MetroidPrime/CActorLights`. The unit stays `NonMatching`; `flip_test.sh`
was not run and the unit was not flipped.

## Result, measured

`build/report.json` for `main/MetroidPrime/CActorLights`: **7 -> 10 of 21 functions at 100%**.
Tree-wide `matched_functions` **9940 -> 9944**; `linked` 4896 unchanged (expected - the unit did not
flip). `tools/goal_check.sh build/goal/item.json` -> `PASS`, every line `ok`.

| function | before | after | Prime 1's source |
| --- | --- | --- | --- |
| `__dt__12CActorLightsFv` | 0.00% | **100%** | Prime 1 has the same empty `{}` body; our dtor was wrong only because of the trait below |
| `AddOverflowToLights__12CActorLightsFRC6CLightRC9CVector3ff` | 93.49% | **100%** | **matched unchanged** after the guard shape and locals lost their `const` |
| `BuildFakeLightList__12CActorLightsFRCQ24rstl42vector<6CLight,...>RC6CColor` | 0.00% | **100%** | **needed one edit**: its `for (i = 0; i < 4; ++i) { if (i == lights.size()) break; ... }` loop, not our `i < 4 && i < lights.size()` |
| `MoveAmbienceToLights__12CActorLightsFRC9CVector3f` | 69.31% | 71.09% | Prime 1's two-step `max_val` form; the `const` on the local had to go too |
| `IsLightExcluded` | 4.00% | 4.00% | no Prime 1 counterpart (see blocker) |
| `BuildDynamicLightList` | 8.72% | 5.32% | stub; see "percentage drops" below |
| `BuildFaceLightList` | 11.81% | 6.26% | stub; see "percentage drops" below |
| `ActivateLights` / `BuildAreaLightList` | 0.37% / 0.22% | unchanged | see blocker |

## What the diff shows, for the next run

Three things, all in `include/Kyoto/Graphics/CLight.hpp` and `src/MetroidPrime/CActorLights.cpp`.

**1. `CLight` needs `is_trivially_destructible`, and only that half of the trait.**
Retail's `~CActorLights` (0x800DE64C, 0x3C bytes) is the `CMemory::Free` stub and nothing else: it
destroys no element and loops over nothing, and `clear()` is a bare `stw` of 0. Without
`RSTL_DECLARE_TRIVIALLY_DESTRUCTIBLE(CLight)`, `reserved_vector<CLight, 4>::clear()` and
`~reserved_vector` inline an element loop in every caller.

**Do not declare the constructible half in the shared header.** `RSTL_DECLARE_TRIVIALLY_CONSTRUCTIBLE(CLight)`
in `CLight.hpp` scores the destructor but makes mwcceppc *outline* `push_back` wholesale - measured
`AddOverflowToLights` 96.16% -> 91.72%, `BuildFakeLightList` 96.91% -> 72.29%, and our object grows an
out-of-line `push_back__Q24rstl26reserved_vector<6CLight,4>FRC6CLight` that retail does not have. The
same specialisation as a `rstl::construct< CLight >` specialisation does the same thing. Put it in the
**.cpp** instead - a file-local `rstl::construct_impl< CLight >` in `CActorLights.cpp` is what took
both callers to 100% and is where it stays. This is the "per call site, not in the shared header"
rule from `RUNNING_THE_DECOMP.md` (`rstl::construct<T>` in assignment form), one instance of it.

Retail's element copy is a plain 80-byte memberwise move (the unnamed `fn_80045E18`, ten lfd/stfd
pairs), *not* CLight's copy constructor - retail's `AddOverflowToLights` calls both, the copy ctor
(0x80038C9C) for the local and the memberwise copy for the `push_back`.

**2. `AddOverflowToLights`: an early `return` guard, and non-`const` locals.** Retail branches
`fcmpo`/`blt` straight to the epilogue on `mag < 0.001f` and `lha`/`cmpwi 1`/`bge` on
`mMaxAreaLights`. Our `if (mag >= 0.001f && mMaxAreaLights > 0) { ... }` produced `cror eq,gt,eq` +
`bne` + `cmpwi 0` + `ble` instead. The `const` on `scaledColor`, `useColor` and `overflowLight` also
had to go (93.49% -> 96.16% on that one change).

**3. `MoveAmbienceToLights`: `max_val` in two steps, `const` off.** Prime 1's
`float maxComponent = rstl::max_val(useColor[kDX], useColor[kDY]); maxComponent = rstl::max_val(maxComponent, useColor[kDZ]);`
(69.31% -> 71.09%). What is left is pure register allocation - retail picks `f2` for the first
`max_val` and `f1` for the second, we pick `f1` and `f0`, and there is a duplicated `fmr` around the
`fcmpo`. Not reached; see `WALL:`.

## Percentage drops, and why they do not fail the item

`BuildDynamicLightList` 8.72% -> 5.32% and `BuildFaceLightList` 11.81% -> 6.26%. Both are TODO stubs
that only got *shorter* (the removed element loops were dead weight, and their few coincidentally
matching bytes went with them). `tools/report_diff.py` reports both as `WORSE` and still exits 0,
because the rule is that a drop only fails in a unit that was `Matching` in the baseline and this one
was not. `goal_check.sh` passed with both lines present.

## Blocked, with the evidence

**`IsLightExcluded` (4.00%, 140 bytes) needs a class that does not exist in this repo.** Retail
0x800DCDC0 calls `GetObjectById(CStateManager, TUniqueId)` and then
`TCastToPtr<CScriptDynamicLight>(CEntity*)` at 0x800997CC. `CScriptDynamicLight` has **no header**:
`grep -rn CScriptDynamicLight include/ src/` finds only three lines in `src/MetroidPrime/TypesMatch.cpp`
(`TYPES_MATCH_CLASS` at 257, `TYPES_MATCH_IMPL` at 512, `CAST_TO_IMPL` at 775). The body is fully
readable - it is `mLayer2` (byte 672 bit 31) selecting between two enable bits of the light at
offset 1090, and `kInvalidUniqueId` short-circuiting - but it cannot be written without the class,
and standing up a `CScriptDynamicLight` header plus its `CActor`-derived layout is its own item.

**`BuildDynamicLightList` (1352 bytes) and `BuildFaceLightList` (1064 bytes) need new
`CStateManager` members.** Retail's `BuildDynamicLightList` reads `mgr+5820` and `mgr+5828` as the
light-ID array and the light-object array, and indexes them directly
(`lhzx r0,r6,r28` / `add r22,r0,r28+4` / `lwz r3,28(r22)` for the light type). This repo's
`CStateManager` has only `GetObjectListById`; there is no `GetDynamicLightList` and no member at those
offsets, so Prime 1's `mgr.GetDynamicLightList()` body cannot be spelled at all. That is a
`CStateManager` job (a shared, heavily-matched unit), not a `CActorLights` one.

**`ActivateLights` (1076 bytes) and `BuildAreaLightList` (3476 bytes) are stubs and Prime 1's source
does not transfer.** Both need Echoes-only behaviour that Prime 1 has no code for: `ActivateLights`
converts special dynamic lights (it reads the ambient as a packed `CColor` with `psq_l` on 660/662/665,
handles `kLT_Hard`=5 lights, and calls `SetGXRegister1Color`), and `BuildAreaLightList` needs the
`SLightValue`/`alloca` sort plus the PVS vis-set walk. These are multi-hour items; I took the bounded
slice rather than half-writing a 3476-byte function.

## Gates

```
goal_check.sh build/goal/item.json   -> PASS (all lines ok; target rose 7 -> 10 / 21)
sha1sum build/G2ME01/main.dol        -> 6ef9b491d0cc08bc81a124fdedb8bfaec34d0010  (unchanged)
tools/probe_sources.sh              -> 749 files, 0 failed; link LINKED (250 undefined, 0 duplicates)
python3 tools/check_symbol_names.py  -> 503 units, 0 declared names missing
./tools/decomp_build.sh              -> All: 30.62% fuzzy, 22.75% matched, 11.74% linked (9944 / 28465)
86 RELs                              -> 86/86 match config.yml, 86/86 cmp-equal to orig
python3 tools/report_diff.py         -> exit 0 (+3 at 100%, 2 percentage drops in the NonMatching unit)
```

`flip_test.sh` deliberately not run: this is a `progress` item, so the unit stays `NonMatching`.
`docs/HANDOFF.md` shows a diff - that is `tools/gate.sh` rewriting the state block, not my edit; the
judge discards it.

WALL: MoveAmbienceToLights 71.09% - the structure and the constants are right; the remainder is which
`f` register each `max_val` and `fmr` lands in, and no source spelling tried moved it past 71.09%.
