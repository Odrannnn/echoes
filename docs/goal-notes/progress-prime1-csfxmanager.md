# progress-prime1-csfxmanager

`kind: progress`, target `Kyoto/Audio/CSfxManager`. The unit stays `NonMatching`; no `flip_test`
was run. Prime 1 (`/run/media/odran/Leo/projects/Restored-projects/Chatgpt/prime-ref`) was used as
the reference for *shape* only — no Prime 1 header, no layout change except the one return type
measured below.

## Result

| | before | after |
|---|---|---|
| `matched_functions` | 61 / 159 | **91 / 159** |
| `matched_code` | 2712 / 22800 (11.89%) | **8280 / 22800 (36.32%)** |
| `fuzzy_match_percent` | 51.17% | **62.52%** |

Whole build, from `tools/gate.sh build/goal/judge/report.base.json`:

```
per-function diff   matched  9668 -> 9698   linked 4894 -> 4894   (+30 functions at 100%, 0 units newly linked)
GATE PASS  c28f8f1+3 changed
```

`0 units newly linked` and `linked 4894 -> 4894` is expected: the unit is `NonMatching`, so its
functions are measured, not linked. I also diffed `report.json` against the baseline function by
function: **0 functions anywhere got worse**. DOL sha1 `6ef9b491…` and all 86 RELs unchanged;
`tools/probe_sources.sh` 744 files, 0 failed; `check_symbol_names.py` 0 missing.

The +30 at 100% is larger than the +30 in this unit (61->91) because the header change below also
fixed functions in two other units; the per-unit count is 91.

## Files

- `src/Kyoto/Audio/CSfxManager.cpp`
- `include/Kyoto/Audio/CSfxHandle.hpp` — one line, `uint GetIndex()` -> `int GetIndex()`

## The one non-CSfxManager change, and why it is right

`CSfxHandle::GetIndex()` returned `uint`. Every retail bounds check on a handle index compiles to
`clrlwi. r5,r3,20` + `blt` — a **signed** compare on the 12-bit index — so the return type has to
be signed for the `< 0` half of the test to exist at all. Prime 1's own header already reads
`int GetIndex() const { return mID & 0xFFF; }`; this is the fork agreeing, not a Prime 1 import.
Nothing else in the class changed; `CHECK_SIZEOF(CSfxHandle, 0x4)` still holds and the build
reproduces retail.

## Per function

Prime 1's source used **unchanged** for these (only Echoes' own member names substituted):

| function | before | after | note |
|---|---|---|---|
| `IsLowPassAreaFilterEnabled` | 99.50% | **100%** | unchanged source; the fix was `empty()` -> `size() > 0` (signed) |
| `IsLowPassEnabled` | 99.50% | **100%** | same |
| `StopAndRemoveAllEmitters` | 94.00% | **100%** | retail re-loads `mSounds[j]` per use; dropped the cached pointer |
| `AllocateCSfxWrapper` | 75.78% | **100%** | Prime 1's `ret == nullptr && size != capacity` shape verbatim |
| `AllocateCSfxEmitterWrapper` | 75.78% | **100%** | same |
| `GetAreaVolume` | 63.68% | **100%** | iterator loop; `area == it->mArea` operand order matters |
| `SetAreaVolume` | 49.03% | **100%** | `break` + `if (it == end())`, not two `return`ing loops |
| `ShouldApplyLowPass` | 83.11% | **100%** | `if (...) return true;` then a separate `bool ret` |
| `RemoveLowPassFilter` | 28.96% | **100%** | iterator loop, `id == it->mId` |
| `RemoveLowPassAreaFilter` | 28.96% | **100%** | same |
| `GetAudible` (emitter) | 91.58% | **100%** | tail must be an `int` narrowed to `short`, not a ternary |
| `TurnOffChannel` | 82.85% | **100%** | no cached pointer; re-index per call |
| `KillAll` | 54.76% | **100%** | Prime 1's two independent `if`s + the `kSC_Game` aux block |
| `GetRank` | 0.00% | **100%** | Prime 1 verbatim + `i < 4` (retail ignores `mListeners.size()`) |
| `CSfxEmitterWrapper::IsPlaying` | 52.66% | **100%** | Prime 1's `if (!IsLooped()) { bool ret; ... }` |
| `AddEmitter` (volume overload) | 79.78% | **100%** | `volume > 20 ? volume : 21`, not `max_val(v, 21)` |
| `LoadTranslationTable` | 97.62% | **100%** | `if (mTranslationTable) delete …`; retail calls the dtor only when non-null |
| `TranslateSFXID` | 51.84% | **100%** | early-return shape, non-`const` local, explicit `static_cast<ushort>` |
| `UpdateLowPassFilters` | 70.89% | **100%** | `const bool keep = !it->mTimed \|\| …` materialised as a variable |
| `UpdateLowPassAreaFilters` | 70.89% | **100%** | same |
| `AddLowPassFilter` | 88.57% | **100%** | `if (size < capacity) { … return id; } return 0;` |
| `AddLowPassAreaFilter` | 88.57% | **100%** | same |
| `SetDuration` | 36.37% | **100%** | bounds check inlined instead of calling `IsQueued` |
| `SetIgnoreAreaLowPass` | 33.83% | **100%** | same |
| `SfxPan` | 57.81% | **100%** | same; `!handle` guard first |
| `SfxSpan` | 57.81% | **100%** | same |
| `IsPlaying` (manager) | 35.90% | **100%** | Prime 1's `IsHandleValid`-then-`IsPlaying` tail |
| `IsQueued` | 62.89% | **100%** | split the `!handle` test out; signed index |
| `SetChannel` | 99.10% | **100%** | `const ESfxChannels current = mCurrentChannel;` kept live in a register |
| `TurnOnChannel` | 51.95% | **100%** | `bool anyActive` with `break`, not a loop with an inner `break` |
| `PitchBend` | 30.30% | **100%** | Prime 1's shape, minus its `Update(0.f)` retry (Echoes has none) |
| `SfxVolume` | 53.53% | **100%** | inlined bounds check; 78.87% at one point, 100% with the check split |
| `UpdateEmitter` | 48.82% | **100%** | `sound->GetEmitter().mX = …` re-called per store, no bound reference |

Needed a small edit away from Prime 1 (Echoes' code genuinely differs, or the shape had to be
matched to bytes):

- `GetAudible` — Echoes returns `static_cast<short>(bool)` for the last bucket, not
  `kSA_Aud1/kSA_Aud0`; Prime 1 returns the enum pair. **This is the biggest divergence between the
  two games in this unit and the reason a straight copy does not match.**
- `KillAll` — added `fn_80340E08(lbl_804152DC); fn_80340F9C(lbl_804152DC);` under
  `#ifndef TARGET_PC`, following the pattern `Initialize`/`Update` already use for the same
  receiver. Without it KillAll is 96% (the aux block is 4 instructions). It does not touch the
  port: `link_check.sh` stays at 250 undefined, 0 duplicates.
- `GetStudio` — the `studios[]` table is a stack local here, so it did not match; 23.24% now.
- `AddEmitter` (parm-data overload) — Prime 1 checks `mSfxId == 0xFFFFFFFF || == 0xFFFF` in
  `#else` (NONMATCHING-off) branches; Echoes emits both compares unconditionally, so the constants
  are written out. 84.96% now.
- `SfxStart` — same `0xFFFFFFFF` compare. 58.86% now.
- `TurnOnChannel` — Echoes writes `mCurrentChannel` **before** `mDoUpdate`; Prime 1 writes
  `mDoUpdate` first. Order taken from the bytes.
- `GetRank` / `TurnOnChannel` listener loops are bounded by the literal `4`, not by
  `mListeners.size()` — retail indexes all four slots unconditionally.

Did not help (measured, reverted): `uint -> short GetIndex()` (adds an `extsh` before the `>=`
compare and costs 2 functions); `CSfxChannel& channel` hoisted in `AddEmitter` before
`LocateHandle` (unchanged); `int maxVol` local in `AddEmitter` (unchanged, but the unconditional
`emitter.mMaxVol = maxVol` I first wrote was **not** faithful — retail branches past the store when
`areaVolume == 127`, so it is now inside the `if`); `CVector3f upVector = up;` in
`UpdateListener` (worse: 3 extra loads).

## Not done, and why

- **`SetActiveAreas` (0.38%, 1060 B)** — Echoes' is not Prime 1's. It walks a 500-byte-per-record
  table at `lbl_80413EFC` and calls six unwritten externs (`fn_80334C50`, `fn_80334CAC`,
  `fn_80334CB4`, `fn_80334C20`, `fn_80334C30`, `fn_8034066C`) whose signatures are unknown. Writing
  it means guessing six function contracts, which is the kind of thing the reviewer rejects as a
  bypassed wall. Left as the existing `TODO:` body.
- **`Shutdown` (30.05%)** — same table, same three externs (`fn_80334C50`/`fn_80334CB4`/
  `fn_80335408`) plus `fn_80340E08`. Blocked by the same thing.
- **`__ct__Q211CSfxManager15CBaseSfxWrapperFbs10CSfxHandlebi` (98.61%)** and
  **`__ct__Q211CSfxManager11CSfxWrapperFbsUsss10CSfxHandlebi` (99.19%)** — the *only* difference is
  which scratch register the compiler picks for three constants (`r11/r10/r9` vs `r10/r9/r6`).
  Every instruction is present and in the same order. This is register allocation, not source.
  See the WALL line below.
- **`__ct__Q211CSfxManager11CSfxChannelFv` (0.00%)** — retail constructs the listeners vector with
  an out-of-line `fn_802A0384` we do not have; the class is only reachable through the four static
  `mChannels` initialisers. Not writable without that function.
- **`UpdateListener` (86.58%) / `AddListener` (93.00%)** — pure float-register and frame-size
  allocation. Retail's `SListener` stores `mMaxVolume` at `+0x40` and `mActive` at `+0x44`; ours
  has `mFlags` at `+0x40`. Same `sizeof` (0x48), so the class layout check passes, but the
  *offsets inside* differ, which is what these two functions are sensitive to. Fixing it means
  moving `CSfxListener::mFlags`, which the brief forbids ("do not change class layouts").
- **`Play` (both wrappers, ~89%)**, **`Update` (83.66%)**, **`StopSound` (65.97%)**,
  **`AddEmitter` (84.96%)**, **`SfxStart` (58.86%)**, **`SetMuted` (13.67%)** — instructions all
  present, register assignment and block ordering differ.

WALL: __ct__Q211CSfxManager15CBaseSfxWrapperFbs10CSfxHandlebi 98.61% - same 36 instructions in the
same order, only the 3 constant-materialising registers differ (r11/r10/r9 vs r10/r9/r6); register
allocation, no source spelling will move it.
WALL: __ct__Q211CSfxManager11CSfxWrapperFbsUsss10CSfxHandlebi 99.19% - one 4-instruction epilogue
group is in a different order (r0 saved before vs after the r28-r31 restores); same cause.

## Codegen rules worth keeping

- `empty()` compiles to `beqlr`, `size() > 0` to `blelr`. Retail's `IsLowPass*FilterEnabled` uses
  `blelr`, so the source is a signed compare, not `empty()`.
- `volume > 20 ? volume : 21` and `rstl::max_val(v, 21)` are different code. Retail has the
  ternary.
- `for (auto it = v.begin(); …)` gives the pointer-walk retail emits; `for (int i = 0; i < v.size(); …)`
  gives an index loop and does not match. `GetAreaVolume`, `SetAreaVolume`, `RemoveLowPass*Filter`
  and `UpdateLowPass*Filters` all need the iterator.
- `a == b` and `b == a` are different instructions (`cmpw r3,r0` vs `cmpw r0,r3`). Retail's operand
  order is measurable in six functions here; match it.
- `x != y` vs `y != x` is **not** symmetric: the compiler loads the operands in source order, so
  `handle != sound->GetSfxHandle()` and `sound->GetSfxHandle() != handle` produce different `lwz`
  pairs. Seven sites in this unit needed the handle first.
- Assigning a `bool` to a `short` local and returning the local is what makes retail emit
  `srwi r0,r0,31; extsh r3,r0`; a direct `return (short)(a < b);` folds to one `srwi r3`.
- `CSfxHandle`'s index is signed in retail. Any new code that bounds-checks one needs
  `GetIndex() < 0 || GetIndex() >= size`, not just the upper test.
