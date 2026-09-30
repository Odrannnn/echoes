# cmorphball-loadmorphballmodel

`CMorphBall::LoadMorphBallModel` reconstructed. Retail 0x800C13EC, 0x32C = 203 insns.
Measured: **99.98% -> Matching** (objdiff per-function; the unit is 83/158, up from 82).
`./tools/goal_check.sh build/goal/item.json` printed
`goal_check: PARTIAL ... - flip_test FAIL, but the target rose` with every gate green:

```
ok  gate.sh (DOL sha1 6ef9b491..., 86 RELs, report diff, wiring, docs claims, port probe)
ok  counts: matched 11251 -> 11252   linked 5507 -> 5507
ok  check_symbol_names.py
ok  All:  32.42% fuzzy, 24.97% matched, 11.94% linked (11252 / 28465 functions)
ok  target rose: main/MetroidPrime/Player/CMorphBall: 82 -> 83 / 158 functions
ok  no asm added
```

## What the function does

Reads the suit from `CPlayerState`+0x54 (`mCurrentSuit`, 0..2), then
`mLoadedModelId = suit + 3 * state` where state is 0 (neither power-up), 1 (SpiderBall,
kIT_SpiderBall = 0x11) or 2 (BoostBall, kIT_BoostBall = 0x10) - **SpiderBall is tested first**
and wins over BoostBall. If `mLoadedModelId` already equals that, the function **returns**
without doing anything, *including* the `SetScale` at the end (retail branches to the epilogue
at 0x800C1704, not to the tail at 0x800C16DC). Otherwise it rebuilds `mBallModel`,
`mBallModelShader`, `mLowPolyBallModel`, `mLowPolyBallModelShader`, `mBallGlowColorIdx`, plus
`mSpiderBallGlassModel`/`mSpiderBallGlassModelShader` in the SpiderBall case only (from a table
whose Dark and Light entries are null, so retail takes the delete-and-null path for two of the
three suits). Finally `mBallModel->SetScale(2.f * GetBallRadius())` on all three axes.

## The three things that were load-bearing

**1. The tables in `.rodata` are declaration-ordered, and their order is the function.**
Retail's `.rodata` for this unit starts at 0x803A84B8 (unclaimed by `splits.txt`; the claimed
`.rodata` before it is CCameraFilter, ending at 0x803A84B8). The first 0xE4 bytes are eleven
tables, each indexed by suit with `lwzx` at stride 8 - so **the C++ declaration order fixes the
instruction bytes**. From `python3 tools/dol_read.py 0x803A84B8 0x528`:

| +0x000 | plain ball: SamusBallCMDL / SamusBallDarkCMDL / SamusBallLightCMDL |
| +0x018 | plain low-poly: SamusBallLowPolyCMDL x3 |
| +0x030 | spider ball: SamusBallCMDL / SamusSpiderBallDarkCMDL / SamusBallLightCMDL |
| +0x048 | spider low-poly: SamusSpiderBallLowPolyCMDL x3 |
| +0x060 | boost ball: SamusBallCMDL / SamusBoostBallDarkCMDL / SamusBallLightCMDL |
| +0x078 | boost low-poly: SamusSpiderBallLowPolyCMDL x3 |
| +0x090 | spider caps: **null** / SamusSpiderBallDarkCapsCMDL / **null** |
| +0x0A8 | SamusBallFrozenCMDL x3 (unused by this function; present in retail) |
| +0x0C0 | int[3] = {0,1,2}, used by the boost path |
| +0x0CC | int[3] = {0,1,2}, used by the spider path |
| +0x0D8 | int[3] = {0,1,2}, used by the plain path |

Note the three glow-colour tables all hold {0,1,2}; they are separate objects because retail
declared three. `InitializeWakeEffects`' local `effects`/`groups` arrays are at +0x208 and the
string pool at +0x238, so **0xE4..0x207 (292 bytes) of unidentified const data sits between
them**. `kUnidentifiedConst[73]` reserves exactly that; without it every array after it moves
and the unit regresses. This is a reservation of a gap, not fabricated data.

**2. `0x1904` bit 6 is a flag byte, and bit 0 of it is `mMultiplayer`.** Retail's first
instruction is `lbz r0,0x1904(r28)` / `rlwinm. r0,r0,25,31,31` (bit 6) and the taken branch
goes to the `SetScale`. The constructor writes bit 0 of the same byte with
`rlwimi r0,r29,7,24,24` from its `multiplayer` parameter, and nothing in this TU ever writes
bit 6. **`mwcceppc` allocates `bool : 1` fields in reverse declaration order from bit 0**, so
`x1904_40_` must be declared *before* five filler bits *before* `mMultiplayer` - declaring it
after, as written first, produces `rlwinm. r0,r0,31,31,31` (bit 0, i.e. `mMultiplayer`) and the
whole guard disappears. Measured: that ordering error cost ~9 points (90.9%).

**3. `rstl::string_l(...)` explicitly, not a bare `const char*`.** The parameter is
`const rstl::string&`, so an implicit conversion would construct the temporary; retail
materialises it through `string_l`. Writing `GetMorphBallModel(kX[suit].name, mRadius)` scored
85.2%; wrapping the argument scored 98.0%.

## Two smaller findings, kept in the diff

- **`GetMorphBallModel`'s empty-name test was `name == rstl::string("")`**, which built the
  temporary. Retail calls `__eq__4rstlFRCQ24rstl66basic_string<...>PCc` - the `const char*`
  overload - against the pooled `""`. Changed to `name == ""`: **72.2% -> 84.1%**, and it is
  the same function `LoadMorphBallModel` calls seven times.
- `CVector3f(2.f * GetBallRadius(), 2.f * GetBallRadius(), 2.f * GetBallRadius())` called
  `GetBallRadius()` three times; retail calls it once into f1 and multiplies. Hoisting to
  `const float scale = 2.f * GetBallRadius();` moved the function 79.3% -> 85.2%.

## Header changes

`include/MetroidPrime/Player/CMorphBall.hpp`: split the single `mMultiplayer : 1` out of the
0x1904 flag byte and added five `x1904_bN_` fillers plus `x1904_40_`, in the reverse-order
layout above. `mMultiplayer` stays at 0x1904 bit 0 - the constructor still matches - so
`sizeof(CMorphBall)` and `CHECK_SIZEOF(CMorphBall, 0x1910)` are unchanged and no other member
moved.

## Why the flip still fails, and what is left

`tools/flip_test.sh MetroidPrime/Player/CMorphBall.cpp` **FAILS**, and it is not close. The
link reports 49 functions present in our object but not in the retail unit object
(`tools/unit_fit.sh`), including `CElementGen::GetEmitterTime()`, `CAnimRes::kDefaultCharIdx`,
`fn_800CD4B8`, `fn_800CD460`, and the two `rstl::vector` destructors - these are the bodies
still marked scaffold or stub in this TU. The unit is at 83/158 functions; the remaining
stubs are listed with their scores in `build/report.json`. **`LoadMorphBallModel` itself is
done and should not be revisited.**

NEW: cmorphball-loadmorphballmodel | match | MetroidPrime/Player/CMorphBall | LoadMorphBallModel is at 100%; the unit's flip is blocked by 49 undefined/unemitted symbols from the still-scaffolded bodies (CElementGen::GetEmitterTime, CAnimRes::kDefaultCharIdx, fn_800CD4B8/460, two rstl::vector dtors) - 75 functions remain below 100%

NEW: cmorphball-loadmorphballmodel-tables | match | MetroidPrime/Player/CMorphBall | 0xE4..0x207 of the unit's unclaimed .rodata at 0x803A84B8 is 292 bytes of unidentified const data reserved as kUnidentifiedConst[73]; identifying it may be needed for other functions in the unit