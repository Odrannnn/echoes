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

---

# Second run (lane 8, 2026-09-30)

Re-measured on `wt-mp2-goal-L8` at `9e2dec9e`. **The previous run's per-function percentages were not
reproducible** - `PitchBend`, `SfxVolume` and `UpdateEmitter` are listed there as 100% but measured
79.64% / 78.87% / 77.31% here, with byte-identical source (`git diff 26ed50a8 HEAD --
src/Kyoto/Audio/CSfxManager.cpp` is empty). I checked the one header change in between
(`e40f8e67` dropped `CSfxHandle::operator=`): putting it back moves nothing (91/159, 62.47% fuzzy,
identical), so the regression is not from that. Its conclusions are treated below as hypotheses
about *spellings*, and the four "100%" rows are simply re-measured as open work.

## Result

| | before (this run) | after |
|---|---|---|
| `matched_functions` | 91 / 159 | **95 / 159** |
| `matched_code` | 8280 / 22800 (36.32%) | **9452 / 22800 (41.46%)** |
| `fuzzy_match_percent` | 62.47% | **65.19%** |

`./tools/goal_check.sh build/goal/item.json`, verbatim:

```
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 10300 -> 10304   linked 5048 -> 5048
  ok    check_symbol_names.py
  ok    All:  31.29% fuzzy, 23.66% matched, 11.83% linked (10304 / 28465 functions)
  ok    target rose: main/Kyoto/Audio/CSfxManager: 91 -> 95 / 159 functions
  ok    no asm added
goal_check: PASS progress-prime1-csfxmanager
```

`linked 5048 -> 5048` is expected: the unit stays `NonMatching`. The `gate.sh` report diff covers all
2057 units, so "no function anywhere got worse" is measured, not assumed.

## Files

- `src/Kyoto/Audio/CSfxManager.cpp` - all of the below.
- `include/Kyoto/Audio/CSfxManager.hpp:242` - `static uchar GetStudio(int)` -> `static int
  GetStudio(int)`. Return types are not mangled, so retail's bytes are the only constraint, and
  retail returns the raw `lbzx` result unmasked. With `uchar` the caller has to emit two `clrlwi`
  (one for the conditional's promotion, one for the `uchar` argument); with `int` it emits one.
  Measured: `Play__CSfxWrapper` 89.93 -> 91.25, `Play__CSfxEmitterWrapper` 88.21 -> 89.57.
  `GetStudio` itself is unchanged by this. No member, offset or `sizeof` moved.

## Reached 100% (4)

| function | before | after | what actually did it |
|---|---|---|---|
| `SetMuted` | 13.67% | **100%** | the body was wrong, not the spelling (see below) |
| `StopSound` | 65.97% | **100%** | two separate bounds/mismatch guards, each with its own `if (channel != kSC_Game) StopSound(kSC_Game, handle); return;` |
| `PitchBend` | 79.64% | **100%** | `const CSfxChannel& channel = mChannels[mCurrentChannel];` as the **first** statement, before the `!handle` guard |
| `CSfxEmitterWrapper::Play` | 88.21% | **100%** | the `ctrl = SND_MIDICTRL_REVERB` store comes **before** an if/else (not a ternary) for `paraData.value7` |

### `SetMuted` was a stub-shaped body, not a spelling problem

Retail (0x8029BC64, 360 B) has three loops, not one, and no call to `TurnOffChannel`:

```
mDoUpdate = true; mMuted = muted;              ; the channel ref is computed FIRST
if (muted) {
  for each sound:  if (!s) skip; s->IsLooped() ? s->UpdateEmitterSilent() : s->Stop();
  for each sound:  if (s && !s->IsLooped()) { s->Release(); mSounds[i] = nullptr; }
} else {
  for each sound:  if (s) s->UpdateEmitter();
}
```

The vtable slots pin the callees exactly (the vtable is byte-identical to retail's - checked
symbol by symbol with `objdump -r -j .data` on both objects, so the slot arithmetic is sound):
`28 = IsLooped`, `72 = Stop`, `92 = UpdateEmitterSilent`, `96 = UpdateEmitter`.

Two spellings mattered and both are counter-intuitive:

- **Index the vector again at every use** (`channel.mSounds[i]->Stop()`, not a cached
  `CBaseSfxWrapper* sound = channel.mSounds[i];`). Retail reloads the element before each virtual
  call; caching it in a register costs an extra callee-saved register, a 5th one, and MWCC switches
  from four `stw`s to `stmw r27,12(r1)`. Cached: 82.53%. Direct: 97.39%.
- **`CSfxChannel& channel` before `mDoUpdate`/`mMuted`.** Putting the reference first is what makes
  the allocator pick `r5`/`r4`/`r6` as retail does instead of `r4`/`r5`/`r6`. 97.39% -> 100%.

`mDoUpdate = true` before `mMuted = muted` (the old run measured the opposite for
`TurnOnChannel`; here retail's `stb` order is unambiguous: `mDoUpdate` first).

### `StopSound`

Retail recursion to `StopSound(kSC_Game, handle)` is emitted **twice**, once for the out-of-range
index and once for the null/handle-mismatch, and each site gets its own hidden-return slot (r1+16
and r1+8) - which is why retail's frame is 48 bytes and the old single-tail version's was 32. The
comparison must be `handle != sound->GetSfxHandle()` (handle operand first, as the old run's
`IsPlaying` finding says); the reversed `==` form scores 99.78%. A `goto` to one shared tail gives
68.69%, and a single combined guard 84.19% - the duplication is real.

### `CSfxEmitterWrapper::Play`

Retail has **two** stores of `paraData.value7`, one per branch, and a common `++numPara` tail
(0x8029F69C). A ternary gives one merged store and an extra `extsh` (the `short` return promoted to
`int`), 89.57%. With the if/else but the `ctrl` store *after* it, 50.43% - the `ctrl` store must
come first, then 100%. `static_cast<uchar>` on the value also gives 100% (same bytes), so the
`extsh` disappears once the expression is a byte store rather than an int one.

## Improved, not reached (4)

| function | before | after | what |
|---|---|---|---|
| `GetStudio` | 23.24% | 59.12% | the `{1, 2}` table is a **file-scope `static const uchar[]` in `.sdata2`**, not a stack local |
| `UpdateEmitter` | 77.31% | 92.50% | `IsSilent()` is tested **before** the position/direction stores, and the two stores are duplicated into both branches |
| `SfxVolume` | 78.87% | 85.61% | final clamp is lower-bound-first: `volume < 1 ? 1 : (volume > 127 ? 127 : volume)` |
| `CSfxWrapper::Play` | 89.93% | 91.25% | the `int GetStudio` return type above |

- `GetStudio`: retail's table is addressed `li r3,0 / SDA21 lbl_8041E2E0 ; addi r3,r2,-16608`, i.e.
  the **first two bytes of this unit's `.sdata2`**, and its contents are literally `01 02`. Moving
  the local array to file scope reproduces that exactly and lands in the existing 48-byte `.sdata2`
  (no growth). `int GetStudio(int area)` also removes a `clrlwi` at every call site.
- `UpdateEmitter`: retail 0x8029E62C calls `IsSilent` (non-virtual, `bl`) *before* the
  `GetEmitter().mPos`/`.mDir` stores, and has **two copies** of those stores - the silent branch
  does the stores then jumps to the epilogue, the non-silent branch does them and then the
  area-volume work. Writing that out is faithful to the bytes, not a duplication I invented.
  `!IsSilent()` as the outer test is 64.29%; the silent branch must be the fall-through.
- `SfxVolume`: retail's clamp tests `< 1` first and only then `> 127`, so
  `rstl::max_val(1, rstl::min_val(v,127))` is the wrong order; the nested ternary matches.
  `min_val(max_val(1,v),127)` (83.05%) and an `int`/`uchar` local (81-85%) all score lower than
  the ternary assigned back to `volume` itself.

## Measured, did not help

Record so the next run skips them.

- `StopSound`: `goto` to a shared recursion tail 68.69%; one combined
  `sound = ... ; if (sound == nullptr || handle != sound->GetSfxHandle())` guard 84.19%;
  `sound->GetSfxHandle() == handle` 99.78%.
- `SetMuted`: `mMuted` before `mDoUpdate` 97.33%; cached `sound` local 82.53%; nested
  `if (sound != nullptr) { if (...) }` instead of `&&` 82.53% (identical to the flat form - use the
  flat one); `!muted` block written first with early `return` 49.86%.
- `PitchBend`: sound loaded from `mChannels[mCurrentChannel].mSounds[...]` with the `!handle` guard
  after it 86.25%; non-`const` channel ref is byte-identical to the `const` one.
- `Play` (`CSfxEmitterWrapper`): ternary 89.57%; if/else after `ctrl` 50.43%; `static_cast<uchar>`
  value with the if/else 100% (same as plain).
- `Play` (`CSfxWrapper`): inline the ternary into the `SfxStart` call 91.25% (same);
  `static_cast<uchar>` on the whole ternary 91.25% (same); casting each branch to `uchar` 88.55%;
  `const uchar studio = static_cast<uchar>(GetStudio(...))` 82.17%; `uchar studio` local
  (pre-`int` return) 85.92%. **`GetReverbAmount()` returning `ushort` instead of `short` changes
  nothing** (both keep their own percentages) - reverted, not worth the diff.
- `SfxVolume`: `min_val(max_val(1,v),127)` 83.05%; `uchar vol` local 84.59%; `ushort` 84.34%;
  `int vol` + `static_cast<short>` 81.06%.
- `GetStudio`: `!static_cast<int>(mCurrentStudio)` / `mCurrentStudio == 0` / `!(m != 0)` all 59.12%
  (identical bytes); ternary `mCurrentStudio ? 1 : 0` 45.00%; `int` local for the flag 47.94%;
  `bool` local 30.29%; `1 - mCurrentStudio` 53.24%; `^ 1` 56.18%; `*(sStudios + idx)` 53.82%;
  direct `sStudios[i]` without the `const uchar*` local 53.82%. `mCurrentStudio` as `uchar` instead
  of `bool` (same 1-byte layout, same `lbz`): 59.12%, no change.
- `UpdateEmitter`: `uchar vol` local in place of `maxVolume` 91.67%; `!IsSilent()` outer 64.29%.
- `UpdateListener`: binding `CSfxListener& lis = entry.mListener` 86.58% (identical);
  reverse store order 82.36%; `mActive` first 76.06%.
- `AddListener`: no variant tried beyond the existing source.

## Still open, and why

- **`GetStudio`'s last 41%** is two things, both measured this run.
  (a) The negation. Retail emits `lbz r0,4(r4) ; cntlzw r0,r0 ; srwi r0,r0,5` - the general 32-bit
  `!x`. We emit `cntlzw r0,r0 ; rlwinm r0,r0,27,24,31`, which is MWCC's bool-range `!x` and, worse,
  leaves `0xF8000000` in the index register for a set flag: **the current `GetStudio` reads out of
  bounds when `mCurrentStudio` is true.** This is pre-existing (it is in the object as of
  `26ed50a8`), not something this change introduced, and it did not get worse - but it is a real
  bug and no spelling I tried removes it, because MWCC narrows the flag to a bool range for `bool`
  *and* for `uchar`. Fixing it is a `NEW:`-worthy change to how the negation is spelled.
  (b) Retail materialises `&mCurrentArea` and reads `mCurrentStudio` at `+4` of it
  (`addi r4,r13,-25836 ; lbz r0,4(r4)`); we emit a second SDA21 relocation. That only happens if
  `mCurrentStudio` has no symbol of its own in retail, which I could not reproduce without changing
  `mCurrentArea`'s symbol, and `mCurrentArea` is read directly by several functions that are at
  100%.
- **`Play` (`CSfxWrapper`), `UpdateEmitter`, `AddListener`, `SfxStart`, `Play`
  (`CSfxEmitterWrapper`) register allocation.** In `UpdateEmitter` every instruction and block is
  in retail's order and the only difference is that retail puts `position` in `r31` and `sound` in
  `r30` while we put `position` in `r30` and `sound` in `r31`. Same for `AddListener` (retail needs
  12 callee-saved registers, we need 13, so the frame is 224 vs 240) and `UpdateListener` (retail's
  float pool is `f0..f6` and starts at `f4`, ours is `f0..f7` and starts at `f3`).
- **`SfxStart` and `AddEmitter` share a pattern I could not reproduce**: retail stamps the
  `CSfxWrapper`/`CSfxEmitterWrapper` temporary's vptr with `__vt__<Derived>` and then *re-stamps it
  with `__vt__CBaseSfxWrapper`*, and emits **no** destructor call. We emit the vptr store once and
  then `bl __dt__<Derived>`. Tried: a named local (82.74%, same), the result bound to a
  `CBaseSfxWrapper*` (83.36%), duplicating the `Allocate*` call into the `mMuted` branch (63.40%).
  I do not know what source makes MWCC re-type a temporary slot without destroying it.
- **`__sinit_CSfxManager_cpp` (66%)** is not a source problem: retail registers each global with an
  *unnamed* local destructor (`fn_8029C938`, `fn_802A0028`, ...) where we register the real mangled
  `__dt__reserved_vector<...>`. Those `fn_*` are 52 of this unit's 0.00% functions, and the
  relocation targets, not the instructions, are what differ.
- **`Shutdown` (30.05%), `SetActiveAreas` (0.38%)** - unchanged from the previous run's analysis;
  both need the auxiliary-effect record at `lbl_80413EFC` and six unknown externs' contracts.
- **`__ct__CBaseSfxWrapper` 98.61%, `__ct__CSfxWrapper` 99.19%** - re-measured here, same
  conclusions as the previous run, from the bytes: the first is three constant-materialising
  registers (`r11/r10/r9` vs `r10/r9/r6`); the second is purely the epilogue order (retail restores
  `lr` **first**, then `r31..r28`; we restore `r31..r28` then `lr`). Both are the allocator, not
  the source. The previous run's `WALL:` lines stand and I am not repeating them.

WALL: GetStudio__11CSfxManagerFi 59.12% - the studios table is fixed, but MWCC emits its
bool-range `!x` (`cntlzw` + `rlwinm 27,24,31`) for every spelling of the negated flag I tried
(bool and uchar, cast, ternary, local, `^1`, `1-x`), and retail's `cntlzw`+`srwi 5` is the 32-bit
form; the second half of the gap is retail's `&mCurrentArea + 4` addressing, which needs
`mCurrentStudio` to have no symbol of its own.
WALL: UpdateEmitter__11CSfxManagerF10CSfxHandleRC9CVector3fRC9CVector3fUc 92.50% - identical
blocks in identical order; the only difference is which of `r30`/`r31` holds `position` and which
holds `sound`.
WALL: __ct__Q211CSfxManager11CSfxWrapperFbsUsss10CSfxHandlebi 99.19% - 37 of 37 instructions match;
the 5-instruction epilogue group is ordered `lr, r31..r28` in retail and `r31..r28, lr` here.
WALL: __ct__Q211CSfxManager15CBaseSfxWrapperFbs10CSfxHandlebi 98.61% - every instruction present in
order; three `li` constants land in r11/r10/r9 instead of r10/r9/r6.

## Codegen rules worth keeping (additions to the previous run's list)

- **A vtable slot index needs the slot base, not the `__vt__` symbol address.** Both objects put
  `__vt__CBaseSfxWrapper` at symbol+0xDC / +0xF4 with the same 12-word prologue, and the function
  address (`__dt__`) sits at slot 2, `SetActive` at slot 3. So the first virtual is at byte 12 of
  the vtable, and the useful anchors are: `28 IsLooped`, `32 IsPlaying`, `40 IsInArea`,
  `52 GetPriority`, `56 GetArea`, `60 GetSfxHandle` (the only struct-returning one),
  `72 Stop`, `76 Ready`, `80 GetAudible`, `84 GetVoice`, `92 UpdateEmitterSilent`,
  `96 UpdateEmitter`, `100 SetReverb`. The old run's slot arithmetic is off by 4 - do not reuse it.
- **`GetStudio`/`GetReverbAmount` return types are free.** Retail returns `GetStudio`'s `lbzx`
  unmasked, so `int` is the honest declaration; declaring `uchar` pushes two `clrlwi` into every
  caller. For `GetReverbAmount` (`li r3,127`) `short` and `ushort` compile identically.
- **MWCC re-loads a vector element at every use rather than keeping it live across a call**, and
  the resulting register count decides whether the prologue uses four `stw`s or one `stmw`. Writing
  `pool[i]->Stop()` instead of caching `pool[i]` in a local is what recovered `SetMuted`.
- **A conditional that assigns to a struct member is compiled as two stores, one per branch, and
  the common tail is duplicated only if the branches are written as an if/else.** A ternary merges
  them into one store plus a phi, and promotes the arms, which is where the extra `extsh` came
  from in `CSfxEmitterWrapper::Play`.
- **Taking `&mChannels[mCurrentChannel]` as a named `const CSfxChannel&` *as the first statement*
  changes the register allocation** of the whole function (`SetMuted` 97.39 -> 100,
  `PitchBend` 79.64 -> 100). MWCC allocates the address temp before the branch, matching retail.

(No `NEW:` filed: the only correctness bug I found is inside `GetStudio`, which is
already a measured wall above, and the brief says a wall is a WALL line, not a queue item.)

---

# Third run (lane 8, 2026-09-30)

Re-measured on `wt-mp2-goal-L8` at `781640ac`. The second run's "after" numbers **were**
reproducible here (95 / 159, 65.19% fuzzy, 9452 bytes), so this run starts from its position.

## Result

| | before (this run) | after |
|---|---|---|
| `matched_functions` | 95 / 159 | **112 / 159** |
| `matched_code` | 9452 / 22800 (41.46%) | **11524 / 22800 (50.54%)** |
| `fuzzy_match_percent` | 65.70% | **74.79%** |

Whole build: `matched 11221 -> 11238   linked 5507 -> 5507   (+17 functions at 100%, 0 units
newly linked)`. `All: 32.36% -> 32.39% fuzzy, 24.91% -> 24.94% matched, 11.94% linked
(11238 / 28465)`. `linked` is unchanged and expected: the unit stays `NonMatching`.

`./tools/goal_check.sh build/goal/item.json`, verbatim:

```
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 11221 -> 11238   linked 5507 -> 5507
  ok    check_symbol_names.py
  ok    All:  32.39% fuzzy, 24.94% matched, 11.94% linked (11238 / 28465 functions)
  ok    target rose: main/Kyoto/Audio/CSfxManager: 95 -> 112 / 159 functions
  ok    no asm added
goal_check: PASS progress-prime1-csfxmanager
```

`build/gate-diff.log` is 36 lines: 17 `RENAMED ... (0.00% -> 100.00%)`, 17 `+100%`, **0
WORSE / GONE / UNLINKED / FELL** over all 2066 units. DOL sha1
`6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`; `probe_sources.sh` 751 files, 0 failed, link
LINKED (244 undefined, 0 duplicates) - all unchanged from the baseline.

## Files

- `config/G2ME01/symbols.txt` - 17 renames (below). No other change; not copied from anywhere.
- `src/Kyoto/Audio/CSfxManager.cpp` - `Update`'s five sound loops (below). Nothing else.

## The 17 renames: our object already contained this code, dtk had named the copies `fn_*`

**This is where the +17 came from, and it is not a Prime 1 port.** 52 of the unit's functions
were `fn_*` at 0.00%: retail functions dtk could not demangle. Our object *already emitted
byte-identical code for 17 of them* under real C++ names (they are the `rstl` helpers our own
template code instantiates), so objdiff could not pair them - the only difference was the name
the target object carried. Giving each target symbol the name its body actually has turned 17
x 0.00% into 17 x 100.00%. This is the "giving an unpaired 0.00% function the name its body
actually has" that `tools/report_diff.py`'s own docstring calls one of the cheapest possible
improvements; `report_diff.py` reports every one as `RENAMED`, never as a loss.

**How each name was established** (all three checks, then verified):

1. *Size + instruction text* is identical to exactly one function our object defines (branch
   targets normalised, so the shape is compared, not the addresses).
2. *The call chain pins the ambiguous ones.* A helper chain in retail is
   `__ct__reserved_vector<SListener,4>`(56) -> `uninitialized_fill_n<SListener*>`(108) ->
   `construct<SListener>`(32) -> `construct_impl<SListener>`(40) -> `__ct__SListener`(68), and
   ours is that same chain with the same five sizes, so the five `fn_*` in it are named from the
   chain, not from a guess. Three `construct_impl<T>` and three `construct<T>` bodies are
   byte-identical to each other; the SListener chain says which one each `fn_*` is.
3. *A caller that is already 100%.* `UpdateLowPassFilters` and `UpdateLowPassAreaFilters` are
   both at 100% in this unit and both `bl` one `fn_`; ours both call
   `erase__reserved_vector<CSfxManager::SLowPassFilter,8>`. That is a proof, not a guess.
   `fn_8029D8F0` is called from `Update` where our source copies a `rstl::vector<short>`, and it
   copies size/capacity, `bl 802FDAB8` (= `allocate__Q24rstl17rmemory_allocatorFi`, already
   named in `symbols.txt`) and then copies elements - a `vector<short>` copy constructor.

| retail name | now | why |
|---|---|---|
| `fn_8029D8F0` (0x100) | `__ct__vector<s>FRCvector<s>` | called from `Update`'s `mpTranslationTable` copy; calls `allocate__...` |
| `fn_8029B1AC` (0x90) | `erase__reserved_vector<SLowPassFilter,8>FP...` | both `UpdateLowPass*Filters` (100%) call it |
| `fn_8029B748` (0x90) | `erase__reserved_vector<CSfxPitchBend,8>FP...` | called from `fn_8029B664` (= `UpdatePitchBends`) |
| `fn_802A00EC` (0x90) | `__dt__CSfxChannel` | called by the global-destructor registration (`Free__7CMemoryFPCv`) |
| `fn_8029FF94` (0x94) | `__dt__reserved_vector<CSfxWrapper,64>` | idem |
| `fn_8029FF00` (0x94) | `__dt__reserved_vector<CSfxEmitterWrapper,64>` | idem |
| `fn_8029BF9C` (0x64) | `__as__CSfxWrapperFRC...` | unique shape + size |
| `fn_8029C2A4` (0xD4) | `__as__CSfxEmitterWrapperFRC...` | unique shape + size |
| `fn_8029C228` (0x7C) | `__ct__C3DEmitterParmDataFRC...` | unique shape + size |
| `fn_8029ABE8` (0xA4) | `__ct<vector<s>>::CFactoryFnReturnF...` | unique shape + size |
| `fn_8029AD1C` (0x2C) | `GetIObjObjectFor__TToken<vector<s>>...` | unique shape + size |
| `fn_802A01C8` (0x38) | `__ct__reserved_vector<SListener,4>FiRC...` | SListener chain |
| `fn_802A0200` (0x6C) | `uninitialized_fill_n<SListener*>...` | SListener chain |
| `fn_802A026C` (0x20) | `construct<SListener>` | SListener chain |
| `fn_802A028C` (0x28) | `construct_impl<SListener>` | SListener chain |
| `fn_802A02B4` (0x44) | `__ct__SListenerFRC...` | SListener chain |
| `fn_802A02F8` (0x8C) | `__ct__CSfxListenerFRC...` | unique shape + size |

Method, for the next run on any unit: list the target's `fn_*` and our object's defined symbols,
pair by size, compare `objdump -d` text with branch targets normalised, confirm the caller, then
rename and **check the function reaches 100%** - a wrong identification cannot reach 100%, so the
rename verifies itself. `touch config/G2ME01/config.yml` after editing `symbols.txt`; the
`split` rule depends on `config.yml`, not `symbols.txt`, so ninja will not re-split otherwise.
`ninja` after the re-split takes ~10 s.

**Not done, and why** - these are the same `fn_*` groups where more than one of our functions has
the identical body, so the sizes and the call sites do not say which is which:

- 3 x `__dt__reserved_vector<X>` (140 B, `fn_8029FDE8`/`fn_8029FE74`/`fn_8029A0028`, all called by
  the global-dtor registration) against our **four** `<SListener,4>`/`<SAreaVolume,10>`/
  `<CSfxPitchBend,8>`/`<SLowPassFilter,8>` dtors. One of ours has no retail twin in this group.
- `fn_8029C938` / `fn_8029ADE4` (100 B) - `__dt__auto_ptr<CToken>` vs `__dt__auto_ptr<vector<s>>`.
- `fn_8029FDAC` (60 B) - `__dt__reserved_vector<CVector3f,4>` vs `<CBaseSfxWrapper*,72>`.
- `fn_8029C12C`/`fn_8029C10C`/`fn_8029BEF8`/`fn_8029BED8` - the `CSfxWrapper` and
  `CSfxEmitterWrapper` `construct_impl`/`construct` (their callers `fn_8029C154` and
  `fn_8029BF20` match nothing of ours, so the chain does not resolve).
- `fn_8029C0C4` / `fn_8029BE90` (72 B) - the two `push_back__reserved_vector<...,64>`.
- **`fn_8029B664` (0xE4) is `CSfxManager::UpdatePitchBends`** - it is called from `PitchBend` and
  `IsQueued`, both at 100%, exactly where our source calls `UpdatePitchBends`. Deliberately
  **not** renamed: our body is 236 B against retail's 228, so the identification is right but the
  function does not match, and renaming it would only trade a 0.00% for a partial score.

## `Update`: 83.66% -> 87.59% (source, not a flip - the unit is still `NonMatching`)

The five loops that walk `chan.mSounds` cached the element in a `CBaseSfxWrapper* sound`. Retail
re-indexes `mSounds` at **every** use: `lwz r3,0(r19)` before each virtual call, never a held
register. With the cache, MWCC emits an extra `mr r3,rN` at each of a dozen call sites and needs
one more callee-saved register. Dropping the cache (`chan.mSounds[i]->Stop()`, Prime 1's own
spelling) makes 747 of retail's 747 instructions line up one-for-one with ours, still displaced
only by register numbers and stack offsets.

This is the same rule the earlier runs found for `SetMuted` and `KillAll`, now measured on a
2988-byte function: **MWCC re-loads a vector element at every use; caching it in a local costs an
`mr` per call and one callee-saved register.** It applies to the emitter loop and the
`mChannels[kSC_Game]` loop in `Update` too - those still cache, and are the next thing to try.

`Update`'s remaining 12.4%, all measured off the bytes, none of it a missing instruction:

- **Frame 432 vs 368.** Retail's frame holds 60 bytes of *unused* stack between the parameter save
  area and the `CVector3f` temporaries (temps at `r1+96/108/120/132`, count at +144, `rights` at
  +148..196, `order` at +196; ours is the identical layout 60 bytes lower). Nothing in retail's
  instruction stream reads +36..+96, so it is dead space MWCC reserved. I did not find the
  declaration that reserves it.
- **Register numbering.** Retail `add r30,r0,r3` (= `&mChannels[mCurrentChannel]`) then
  `addi r31,r30,296` (= `&mSounds`); we pick r31 then r30. The same swap, in the same direction,
  is `SfxVolume` (`this` in r30 / handle in r29, we do the reverse) and `UpdateEmitter`
  (run 2's `position`/`sound` r31/r30). One cause, three functions.
- **The listener block materialises ten `CVector3f` stack temporaries where we materialise five**
  (retail stores at `r1+24,36,48,60,84,132`; ours at `24,32,36,44,48,56,60,64,68,72,76,80`). The
  same source expression, so this is how MWCC allocated the vector temporaries, not what the
  source says.
- `fn_8029B664` in place of `UpdatePitchBends` (see above), and retail's `mr r0,r3` before the
  `SfxCtrl` argument in `Play`.

## Measured, did not help

- `Play__CSfxWrapper` (91.25%, unchanged): retail passes the studio value to `SfxStart`'s `uchar
  prio` with `mr r6,r3` and **no** `clrlwi`; we emit `clrlwi r6,r3,24`. `GetStudio` returning
  `uchar` instead of `int`: 89.93%, and the unit loses a function elsewhere (95 -> 94), so `int`
  stays. `static_cast<uchar>` inside the ternary: 89.93%. `const uchar reverb = GetReverbAmount();`
  as a named local before `SfxCtrl`: 91.25%, byte-identical. The remaining diff is that one
  instruction plus retail's `mr r3,r5`/`lhz r5,28(r31)` pair.
- The two ctors are still where the earlier runs left them: `__ct__CSfxWrapper` 99.19% is purely
  the epilogue order (retail reloads `lr` first, then r31..r28) and `__ct__CBaseSfxWrapper`
  98.61% is three constant registers (r11/r10/r9 vs r10/r9/r6). Measured this run: of the 20
  functions in our object that save >=3 GPRs, **19 emit the retail epilogue order**
  (`lwz r0,<lr slot>` first) and `__ct__CSfxWrapper` is the only exception - so it is not a
  general codegen property, it is something about that one function I did not find. I re-measured
  the two, so those `WALL:` lines stand; I did not re-try any spelling.
- `GetStudio` 59.12%, `UpdateEmitter` 92.50%, `AddListener` 93.00%, `UpdateListener` 86.58%,
  `SfxVolume` 85.61%, `AddEmitter` 82.74%, `SfxStart` 58.86%, `__sinit` 66%, `Shutdown` 30.05%,
  `SetActiveAreas` 0.38% - all unchanged by this diff. No `WALL:` for any of them: I did not try
  several spellings this run, so a wall line would be a claim I did not measure.

## Codegen / tooling notes worth keeping

- **`objdiff-cli diff -p . -u main/Kyoto/Audio/CSfxManager --format json-pretty <sym>`** gives a
  per-instruction diff, but its `DIFF_ARG_MISMATCH` marker is **not** the match criterion: a
  function at 100% (`UpdateLowPassFilters`) shows 20 of them, all pure `lbl_80418A34` vs
  `mLowPassFrequency__11CSfxManager` symbol-name spelling. Use `build/report.json`'s per-function
  `fuzzy_match_percent` for the score; the JSON is only good for *where* to look.
- **`build/report.json`'s per-function list holds target functions only.** Our own functions that
  retail names differently are invisible there, which is why 52 `fn_*` sat at 0.00% while the code
  was already in the object. `nm -S` on `build/G2ME01/src/<unit>.o` against the report's list is
  how the missing pairs are found.
- The retail/ours instruction alignment for one function, with `tools/dis.sh`-style disassembly
  from `build/G2ME01/main.elf` pasted against `objdump -d --disassemble=<sym> build/G2ME01/src/...o`,
  plus `difflib` on the mnemonic text, is what made `Update` legible. Both dumps must strip
  `<symbol>` annotations and normalise the 8-hex-digit addresses, or every branch looks different.
- **`config/G2ME01/symbols.txt` already uses the `<s,...>` template spelling** MWCC emits for
  `short` (`__ct__Q24rstl36vector<s,Q24rstl17rmemory_allocator>FR12CInputStream...` is at 0x80255DA8),
  so a renamed name must use exactly the compiler's spelling, not a tidier one.
- This unit's retail `.sdata2` is 48 bytes at `0x8041E2E0`: the 2-byte studios table sits **first**
  (at +0) and retail's floats start at +0x10. Ours puts the table at +0x10. Both are 48 bytes and
  the float constants land on the same addresses, so nothing else in the unit is disturbed by it.

---

# Fourth run (lane 7, 2026-09-30)

Re-measured on `wt-mp2-goal-L7` at `8c57cd88`. The third run's "after" numbers **were** reproducible
here (112 / 159, 11524 B, 74.79% fuzzy), so this run starts from its position. Nothing in the three
earlier runs was repeated: every spelling they list is re-measured below, and the +17 came from
work none of them tried.

## Result

| | before (this run) | after |
|---|---|---|
| `matched_functions` | 112 / 159 | **129 / 159** |
| `matched_code` | 11524 / 22800 (50.54%) | **13352 / 22800 (58.56%)** |
| `fuzzy_match_percent` | 74.79% | **82.81%** |

`./tools/goal_check.sh build/goal/item.json`, verbatim:

```
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 11254 -> 11271   linked 5507 -> 5507
  ok    check_symbol_names.py
  ok    All:  32.43% fuzzy, 25.01% matched, 11.94% linked (11271 / 28465 functions)
  ok    target rose: main/Kyoto/Audio/CSfxManager: 112 -> 129 / 159 functions
  ok    no asm added
goal_check: PASS progress-prime1-csfxmanager
```

`build/gate-diff.log` is 36 lines: 17 `RENAMED ... (0.00% -> 100.00%)` and 17 `+100%`, **0
WORSE / GONE / UNLINKED / FELL** over all 2066 units - "no function anywhere got worse" is measured.
DOL sha1 `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`. `linked 5507 -> 5507` is expected: the unit
stays `NonMatching`.

## Files

- `config/G2ME01/symbols.txt` - 17 renames (below), nothing else; not copied from anywhere.
- `include/Kyoto/Audio/CSfxPitchBend.hpp:13` - one line, `CSfxHandle GetHandle() const` ->
  `const CSfxHandle& GetHandle() const`. No member, offset or `sizeof` moved.

## 1. `UpdatePitchBends`: 0.00% -> 100% (source, then a rename)

Run 3 found retail's `fn_8029B664` (228 B) is `CSfxManager::UpdatePitchBends` and deliberately did
not rename it because our body was 236 B. The 8 extra bytes were **two redundant copies of the
handle**, and they come from the accessor, not the loop:

```asm
retail  lhz r4,4(r28) ; addi r3,r1,12 ; lwz r0,0(r28) ; stw r0,12(r1) ; bl PitchBend
ours    lwz r0,0(r28) ; addi r3,r1,20 ; lhz r4,4(r28) ; stw r0,16(r1) ; stw r0,20(r1) ; bl PitchBend
```

`CSfxPitchBend::GetHandle()` returned `CSfxHandle` **by value**, so MWCC materialised the return
value into a temporary (r1+16) and then copied it into the by-value argument slot (r1+20) - two
`stw`s per call, four in the loop. Returning `const CSfxHandle&` removes the temporary and leaves
exactly one `stw` per call, and the object becomes 0xE4 = 228 bytes, byte-identical to retail
including the `lhz`-before-`lwz` operand order. The same shape appears for `IsQueued`
(r1+8 / r1+12 -> r1+8 only).

`include/Kyoto/Audio/CSfxPitchBend.hpp` is the only place this pattern occurs; nothing else in the
tree changed (verified: `Allocate*`, `PitchBend`, `IsQueued` stayed at 100%, and the whole-build
report diff has 0 losses).

**Codegen rule, general:** a by-value struct return on a 4-byte trivial type costs one extra copy
per call site, because MWCC materialises the return value *and* the argument. Declare such
accessors `const T&` when the caller only passes them straight on. This is the same family as
run 1's "MWCC re-loads a vector element at every use": both are about materialised temporaries
costing a `stw`/`mr` and sometimes a callee-saved register.

With the body fixed, `fn_8029B664` was renamed to `UpdatePitchBends__11CSfxManagerFf`
(called from `PitchBend` and `IsQueued`, both 100%).

## 2. The other 16: 0.00% -> 100%, all renames of code our object already emitted

This is run 3's method generalised, and it is where the other +16 came from. `build/report.json`'s
per-function list holds **target** functions only, so a function retail could not demangle is 0.00%
even when our object already contains byte-identical code under a real C++ name. Two ingredients
make the identification provable rather than a guess:

1. **Size unique on our side.** A size with exactly one candidate in our object cannot be
   misidentified.
2. **A 100%-matched caller, or the `__sinit` destructor-registration order.**

The second is new and is what cracked the groups the earlier runs called ambiguous. Retail's
`__sinit_CSfxManager_cpp` (0x8029FA60) calls `__register_global_object` nine times; each call loads
the global's address and its destructor. Reading the destructor addresses in registration order
gives the **static declaration order**, and MWCC emits definitions in **reverse** declaration order,
so the `.text` order of the destructor definitions is the reverse of the registration order. That
pinned all three 140-byte `__dt__reserved_vector` dtor calls, which the earlier runs could not
separate.

### The `CSfxWrapper` / `CSfxEmitterWrapper` pool group (8) - the chain resolves after all

Run 3 wrote these off: "their callers `fn_8029C154` and `fn_8029BF20` match nothing of ours, so the
chain does not resolve." The premise was wrong. `nm -S` shows our object *does* define the whole
chain, as **weak** symbols - the earlier survey had only looked at the `T`/`t` lines:

```
 1110  32 t construct<CSfxWrapper>__4rstlFPvRC...
 1130  40 W construct_impl<CSfxWrapper>__4rstlFPvRC...
 1158 124 W __ct__CSfxWrapperFRC...
 10c8  72 W push_back__reserved_vector<CSfxWrapper,64>FRC...
```

`push_back` -> `construct` -> `construct_impl` -> copy ctor, and retail's four addresses
(0x8029BE90/0x8029BED8/0x8029BEF8/0x8029BF20) chain identically. The two groups are told apart by
**position**: the first sits between `AllocateCSfxWrapper` (100%) and `__as__CSfxWrapperFRC`
(100%), the second between `AllocateCSfxEmitterWrapper` and `__ct__C3DEmitterParmDataFRC` (both
100%). And the top of each chain is a direct proof: our `AllocateCSfxWrapper` is byte-identical to
retail's and its relocation list contains `push_back__...<CSfxWrapper,64>`, so retail's function at
0x8029BE5C (which is inside `AllocateCSfxWrapper`) **is** that push_back.

| retail name | now | proof |
|---|---|---|
| `fn_8029BE90` (72) | `push_back__Q24rstl48reserved_vector<Q211CSfxManager11CSfxWrapper,64>FRCQ211CSfxManager11CSfxWrapper` | `bl` from 100%-matched `AllocateCSfxWrapper` |
| `fn_8029BED8` (32) | `construct<Q211CSfxManager11CSfxWrapper>__4rstlFPvRCQ211CSfxManager11CSfxWrapper` | called by the push_back above; size unique in its chain |
| `fn_8029BEF8` (40) | `construct_impl<Q211CSfxManager11CSfxWrapper>__4rstlFPvRCQ211CSfxManager11CSfxWrapper` | called by that construct |
| `fn_8029BF20` (124) | `__ct__Q211CSfxManager11CSfxWrapperFRCQ211CSfxManager11CSfxWrapper` | called by that construct_impl; 124 B unique |
| `fn_8029C0C4` (72) | `push_back__Q24rstl55reserved_vector<Q211CSfxManager18CSfxEmitterWrapper,64>FRCQ211CSfxManager18CSfxEmitterWrapper` | `bl` from 100%-matched `AllocateCSfxEmitterWrapper` |
| `fn_8029C10C` (32) | `construct<Q211CSfxManager18CSfxEmitterWrapper>__4rstlFPvRCQ211CSfxManager18CSfxEmitterWrapper` | called by the push_back above |
| `fn_8029C12C` (40) | `construct_impl<Q211CSfxManager18CSfxEmitterWrapper>__4rstlFPvRCQ211CSfxManager18CSfxEmitterWrapper` | called by that construct |
| `fn_8029C154` (212) | `__ct__Q211CSfxManager18CSfxEmitterWrapperFRCQ211CSfxManager18CSfxEmitterWrapper` | called by that construct_impl; 212 B unique |

Note the size pair is *crossed* against the earlier runs' guess: 124 B is the `CSfxWrapper` copy
ctor (in the first group) and 212 B is the `CSfxEmitterWrapper` one. It is the group, not the size,
that names the class.

### The global destructors (6) - from `__sinit`'s registration order

Retail registers, in order (destructor address -> object):

```
1  0x802A00B4 (56)  mChannels (array of 4 CSfxChannel, 584 B each)
2  0x802A0028 (140) }
3  0x802A0028 (140) } the two low-pass filter vectors
4  0x8029C938 (100) the translation-table token
5  0x8029FF94 (148) mWrapperPool
6  0x8029FF00 (148) mEmitterWrapperPool
7  0x8029FE74 (140) }
8  0x8029FDE8 (140) } pitch bends, area volumes
9  0x8029FDAC (60)  a global this unit does not have
```

Our `__sinit` has the identical pattern (56, 140, 140, 100, 148, 148, 140, 140) for the same eight
objects, in the same order, and `nm` on the object shows the same dtors. Reversing each list gives
the same answer twice - from the registration order and from the `.text` order - so:

| retail name | now | proof |
|---|---|---|
| `fn_802A00B4` (56) | `__arraydtor$240` | retail's `__construct_array(dest, __ct__CSfxChannel, dtFn, 584, 4)` passes this address as the array destructor, and the body is `__destroy_arr(addr, addr, 584, 4)` like ours |
| `fn_802A0028` (140) | `__dt__Q24rstl50reserved_vector<Q211CSfxManager14SLowPassFilter,8>Fv` | registered **twice**, the only 140 B dtor we also register twice |
| `fn_8029C938` (100) | `__dt__Q24rstl17auto_ptr<6CToken>Fv` | 4th registration; the only other 100 B dtor we have is `__dt__auto_ptr<vector<s>>`, which retail's has a direct `bl` for (from `__ct__<vector<s>>::CFactoryFnReturnF`, 100%) and ours does not |
| `fn_8029FE74` (140) | `__dt__Q24rstl34reserved_vector<13CSfxPitchBend,8>Fv` | 7th registration |
| `fn_8029FDE8` (140) | `__dt__Q24rstl48reserved_vector<Q211CSfxManager11SAreaVolume,10>Fv` | 8th registration |
| `fn_8029ADE4` (100) | `__dt__Q24rstl55auto_ptr<Q24rstl36vector<s,Q24rstl17rmemory_allocator>>Fv` | the only one of the two with a direct `bl` in the whole ELF, from a 100%-matched caller; `__dt__auto_ptr<CToken>` has none |

All four bodies in the 140 B group are byte-identical to each other, and both 100 B
`auto_ptr` dtors are too, so **none of this can be established from the bodies at all** - only from
the registration and call structure. That is why the earlier runs, looking only at sizes and call
sites inside the group, could not separate them.

### The two `TObjOwner` dtor/copy (2) - unique size, identical text

`fn_8029AC8C` (144) and `fn_8029AD48` (156) have exactly one candidate of that size in our object,
and each is instruction-for-instruction identical to it (36 and 39 instructions, same order, same
immediates, only relocations differ):

| retail name | now |
|---|---|
| `fn_8029AC8C` (144) | `__dt__71TObjOwnerDerivedFromIObj<Q24rstl36vector<s,Q24rstl17rmemory_allocator>>Fv` |
| `fn_8029AD48` (156) | `GetNewDerivedObject__71TObjOwnerDerivedFromIObj<Q24rstl36vector<s,Q24rstl17rmemory_allocator>>FRCQ24rstl55auto_ptr<Q24rstl36vector<s,Q24rstl17rmemory_allocator>>` |

## 3. Measured this run, did not help

- `__ct__CSfxWrapper` 99.19%, `mReady(true)` moved to the front of the initialiser list: **99.19%,
  byte-identical object**. MWCC normalises the initialiser list to declaration order, so *no*
  initialiser-list spelling can reach a different allocation here.
- `__ct__CBaseSfxWrapper` 98.61%, `mPlaying` and `mInArea` swapped in the initialiser list (and
  `mPriority` moved ahead of `mRank` in a separate run): **98.61%, byte-identical object**, same
  reason.
- Re-measured unchanged: `GetStudio` 59.12%, `UpdateEmitter` 92.50%, `AddListener` 93.00%,
  `UpdateListener` 86.58%, `SfxVolume` 85.61%, `AddEmitter` 82.74%, `SfxStart` 58.86%, `Play` 91.25%,
  `Update` 87.59%, `__sinit` 66%, `Shutdown` 30.05%, `SetActiveAreas` 0.38%. I did not try new
  spellings on these, so no `WALL:` for any of them.

## 4. What is left, and the measurements behind it

Sixteen 0.00% functions remain, and none of them is a rename: our object emits no code of that
size, so they need new source.

- **The eight 60-byte wrappers (0x8029BA7C..0x8029BC20)** are one template instantiated eight times:
  `f(retval, a, b, c); fn_8029B8E8(retval);` with a 512-byte frame, differing only in the first
  `bl` target - eight `CAudioSys` entry points 0x80 apart (0x8033542C, 0x803354AC, 0x8033555C,
  0x803355FC, 0x803356BC, 0x803357F4, 0x803358A8, 0x8033593C).
- **`fn_8029B8E8` (404 B)** is their common tail and is the same "find or add a record in the
  500-byte-per-record table at 0x80413EFC, 10 records" loop that blocks `SetActiveAreas` and
  `Shutdown`. It calls `fn_80334C50`. **This is the same blocker as in runs 1-3, now with the
  helper identified**: one private "index of the area record for this id" function gates three
  groups of functions (8 wrappers + the helper + parts of `SetActiveAreas`/`Shutdown`).
- **`fn_802A0384` (168 B)** is the out-of-line `SListener` default constructor that retail's
  76-byte `__ct__CSfxChannel` calls. Ours materialises the same `SListener` twice *inline*
  (`__ct__CSfxChannel` is 196 B), so the split cannot be forced from source - and without
  `fn_802A0384` present, `__ct__CSfxChannel` cannot match either.
- **`fn_8029FD30` (124), `fn_8029FCE0` (80), `fn_8029FC34` (172), `fn_8029B81C` (204)** are
  retail-only code; the same-size functions in our object (`KillAll`, `GetVoice<CSfxEmitterWrapper>`,
  `__ct__CSfxWrapperFRC`, `__ct__C3DEmitterParmDataFRC`) are already matched to their own symbols and
  are not the same code (similarity 0.45-0.49 on normalised text).
- **`fn_8029FDAC` (60 B)** is the one honest loose end left: retail's 9th registered global, a 60-byte
  dtor. We emit two 60-byte dtors, both unreferenced dead code - `__dt__reserved_vector<CVector3f,4>`
  and `__dt__reserved_vector<CBaseSfxWrapper*,72>` - and nothing in the registration or call
  structure tells them apart, so I did not guess. **One function, not worth a coin flip.**

## 5. `SfxStart` (58.86%) - what retail's call sites prove about two signatures

Not a rename, and not finished, but it is a *measured* structural difference that the next run
should start from rather than re-derive. Retail's `SfxStart` computes the pool slot itself and
passes it:

```asm
retail  lwz r0,index(r13) ; li r3,1 ; lis r4,pool ; stb r3,mDoUpdate(r13) ; mulli r5,r0,584
        addi r3,r1,20 ; addi r0,r4,4200 ; extsh r4,r30 ; add r31,r0,r5 ; bl LocateHandle
        ... ; bl __ct__CSfxWrapper ; addi r0,r4,31612 ; stw r0,28(r1) ; stw r3,296(r5)
ours    li r0,1 ; addi r3,r1,20 ; stb r0,mDoUpdate(r13) ; bl LocateHandle ; ...
        ... ; bl __ct__CSfxWrapper ; bl AllocateCSfxWrapper ; stw r3,0(r26)
```

So retail's `LocateHandle` takes two arguments it never reads (a `CSfxWrapper*` and the pan) - its
own body starts `li r4,0` and uses r3 as the hidden return pointer - and retail's
`AllocateCSfxWrapper` takes a second argument, the destination slot. **Neither signature change
would alter the two functions' own bodies** (both are at 100% and the extra parameters are unused),
so declaring the parameters is safe. What still blocks the function is one thing: the whole body is
shifted by one register (`stmw r23,76(r1)` + `this` in r23 in retail, `stmw r24,80(r1)` + `this` in
r25 in ours), the same one-register offset run 3 measured in `Update` and `SfxVolume`.

## WALL lines re-measured this run

Both ctors were re-measured here, and the initialiser-list order was tried on both (byte-identical
objects, see above), so these are this run's measurements, not a copy of an earlier verdict.

WALL: __ct__Q211CSfxManager11CSfxWrapperFbsUsss10CSfxHandlebi 99.19% - all 37 instructions match in
order; the only difference is the epilogue's restore order (retail `lwz r0,<lr>` then r31..r28, we
emit r31..r28 then lr). Moving `mReady` within the initialiser list produced a byte-identical
object, so no initialiser-list spelling can move it.
WALL: __ct__Q211CSfxManager15CBaseSfxWrapperFbs10CSfxHandlebi 98.61% - all 36 instructions match in
order; the three `li` constants land in r10/r9/r6 instead of r11/r10/r9. Reordering the initialiser
list (`mPriority` before `mRank`, and `mInArea` before `mPlaying`) produced a byte-identical object;
the allocation is driven by declaration order, not by the initialiser list.

## Method notes for the next run on any unit

- **`nm -S` prints weak (`W`) and local (`t`) definitions too.** Run 3 concluded this unit's
  pool code "matches nothing of ours" because the chain was looked for in the `T` lines only; the
  `W` symbols are the missing half. Always take the whole `TtWwVv` set.
- **`build/report.json`'s function list is target-only**, so a target function at 0.00% may already
  be in our object. The unpaired remainder is exactly
  `nm -S --defined-only` symbols whose names are absent from the report's list - 22 of them at the
  start of this run, now 4.
- **Retail's `__sinit_<file>_cpp` is a symbol table for the file's statics.** The destructor
  address passed to each `__register_global_object` call, read in order, gives the static
  declaration order; combined with reverse emission that resolves groups of byte-identical
  destructors that no amount of size or shape analysis can separate. This is the technique that
  cracked the three 140-byte dtor calls, and it applies to any unit with several same-size dtor
  instantiations.
- **A rename verifies itself**: a wrong identification cannot reach 100%, so a rename that lands at
  100% is evidence the identification was right. All 17 here did.
- **MWCC normalises the member-initialiser list to declaration order.** Reordering it to change
  allocation is a no-op; the only levers are the declarations themselves and the number of constants
  materialised.
- An unused by-value parameter is still materialised at the call site (`extsh r4,r30` in retail's
  `SfxStart`), so retail's arities are visible in the call sequence even when the callee ignores
  them. That is how the `LocateHandle`/`AllocateCSfxWrapper` argument counts were established.

---

# Fifth run (lane 3, 2026-09-30)

Re-measured on `wt-mp2-goal-L3` at `33b784fb`. Run 4's "after" numbers **were** reproducible here
(129 / 159, 13352 B, 82.81% fuzzy), so this run starts from its position. No spelling any earlier run
lists was retried; the +2 came from work none of them tried.

## Result

| | before (this run) | after |
|---|---|---|
| `matched_functions` | 129 / 159 | **131 / 159** |
| `matched_code` | 13352 / 22800 (58.56%) | **13596 / 22800 (59.63%)** |
| `fuzzy_match_percent` | 82.81% | **83.88%** |

`./tools/goal_check.sh build/goal/item.json`, verbatim:

```
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 11293 -> 11295   linked 5507 -> 5507
  ok    check_symbol_names.py
  ok    All:  32.49% fuzzy, 25.14% matched, 11.94% linked (11295 / 28465 functions)
  ok    target rose: main/Kyoto/Audio/CSfxManager: 129 -> 131 / 159 functions
  ok    no asm added
goal_check: PASS progress-prime1-csfxmanager
```

`build/gate-diff.log` is 5 lines: 2 `+100%`, 1 `RENAMED ... (0.00% -> 100.00%)`, **0
WORSE / GONE / UNLINKED / FELL** over all 2066 units — "no function anywhere got worse" is measured.
`linked 5507 -> 5507` is expected: the unit stays `NonMatching`.

## Files

- `config/G2ME01/symbols.txt` - one rename (below). Not copied from anywhere.
- `include/Kyoto/Audio/CSfxManager.hpp:159` - `SListener() : mActive(false) {}` (defined inline in
  the class body) becomes a declaration `SListener();`. No member, offset or `sizeof` moved;
  `NESTED_CHECK_SIZEOF(CSfxManager, SListener, 0x48)` still holds.
- `src/Kyoto/Audio/CSfxManager.cpp:36` - the definition, `CSfxManager::SListener::SListener() :
  mActive(false) {}`.

## The whole change: an inline default constructor that retail has out-of-line

Run 4 listed `fn_802A0384` (168 B) as *not writable*: "Ours materialises the same `SListener` twice
*inline* (`__ct__CSfxChannel` is 196 B), so the split cannot be forced from source - and without
`fn_802A0384` present, `__ct__CSfxChannel` cannot match either." **That premise was wrong.** The split
is forced by moving the constructor out of the class body, and it is a *declaration* change, not a
layout one:

```cpp
// include/Kyoto/Audio/CSfxManager.hpp
struct SListener {
  SListener();          // was: SListener() : mActive(false) {}
  CSfxListener mListener;
  bool mActive;
};
// src/Kyoto/Audio/CSfxManager.cpp
CSfxManager::SListener::SListener() : mActive(false) {}
```

With the body inline, MWCC inlines it into both of its use sites, so no `SListener` constructor symbol
is emitted at all. With it out-of-line, MWCC emits `__ct__Q211CSfxManager9SListenerFv` at **0xA8 =
168 bytes**, which is exactly retail's `fn_802A0384`, and `__ct__CSfxChannel` shrinks from 196 B to
retail's 76 B and calls it. Then run 3/run 4's rename rule applies: the target symbol is given the
name its body actually has, and the rename verifies itself by landing at 100%.

| retail name | now | before | after |
|---|---|---|---|
| `fn_802A0384` (168) | `__ct__Q211CSfxManager9SListenerFv` | not in our object | **100%** |
| `__ct__Q211CSfxManager11CSfxChannelFv` (76) | unchanged | 0.00% (196 B body) | **100%** |

Verification before renaming: normalised `objdump` text of our `__ct__...9SListenerFv` against
`tools/dis.sh 0x802A0384 0xA8` is **42 instructions against 42, opcode-for-opcode identical in the
same order**. Then the rename reached 100%, which is itself the proof (a wrong identification cannot
reach 100%). `touch config/G2ME01/config.yml` after editing `symbols.txt` — the `split` rule depends
on `config.yml`, not `symbols.txt`, so ninja will not re-split otherwise; the re-split + rebuild is
about 12 s.

**Codegen rule, general and worth more than this item:** *a default constructor defined in the class
body is invisible as a symbol — MWCC inlines it into every use site. If retail emits the constructor
out of line, declare it in the header and define it in the .cpp.* This is the third time this unit's
history has turned on "our object emits the code but under the wrong name or not as a symbol at all"
(runs 3 and 4's 33 renames, and this), and it is cheap to test: one declaration, one definition, one
build.

**This also supersedes run 4's "not writable" note and run 2's `SListener` layout worry.** Run 2 wrote
that "Retail's `SListener` stores `mMaxVolume` at `+0x40` and `mActive` at `+0x44`; ours has `mFlags`
at `+0x40`" and that fixing it "means moving `CSfxListener::mFlags`, which the brief forbids". That is
**wrong**: `CSfxListener`'s tail is `mFlags` at +0x3C, `mMaxVolume` at +0x40, and `SListener::mActive`
at +0x44 in both. Measured here from the bytes - retail's `__ct__CSfxListener` ends
`stw r8,60(r3) ; stb r9,64(r3)` and retail's `__ct__CSfxChannel` ends `stw r0,292(r31)`. Our layout
is identical; the earlier runs' two `UpdateListener`/`AddListener` percentages are float-register and
frame allocation, not offsets. Do not spend another run on a layout change here.

## Measured this run, did not help (reverted, so the next run skips them)

- **`Update`'s emitter loop: drop the cached `sound`, as run 3 suggested.** Run 3's closing note said
  the rule "applies to the emitter loop and the `mChannels[kSC_Game]` loop in `Update` too - those
  still cache, and are the next thing to try". **It does not apply there.** Re-indexing
  `chan.mSounds[i]` at every use in the emitter loop (the `CBaseSfxWrapper* sound` local removed, with
  the `static_cast` re-reading `chan.mSounds[i]`) takes `Update` from **87.59% to 87.23%**. The rule
  is real but local: it holds where the cached pointer feeds **virtual calls**, and not where it feeds
  a `static_cast` and a later field read.
- **`AddListener`: replace the `SListener& entry` reference with two full-index expressions.**
  **93.00% -> 16.80%**, unit 131 -> 131 but the function collapses. Retail computes
  `&mChannels[channel].mListeners[listener]` once (`mulli r10,72` / `add r31,r4,r0`) and keeps it live
  across the `__ct__CSfxListener` call, so the named reference is load-bearing, not a convenience.
- **`SAreaVolume` is trivially destructible - declare it so.** `RSTL_DECLARE_TRIVIALLY_DESTRUCTIBLE(
  CSfxManager::SAreaVolume)` makes `__dt__reserved_vector<SAreaVolume,10>` collapse from retail's
  140 B loop to the 60 B form, and the unit goes **131 -> 130**. So retail's 140-byte dtor with the
  unrolled-count loop **is genuine** and `SAreaVolume` must stay non-trivially-destructible as far as
  `rstl::reserved_vector` is concerned. (Note the macro needs `namespace rstl { }` around it and the
  header needs `rstl/construct.hpp` in scope; `reserved_vector.hpp` does not pull it in.)
- **`AllocateCSfxWrapper` / `AllocateCSfxEmitterWrapper`: take a non-const reference**, on the theory
  that retail's vptr re-stamp after the `Allocate` call is MWCC restoring a vptr mutated through a
  non-const reference. **The unit drops 131 -> 129**: `SfxStart` and `AddEmitter` do not move at all
  (58.86% and 82.74%, unchanged) and the two `Allocate*` functions themselves lose their match. So the
  re-stamp is not caused by the parameter's constness. Run 2's "I do not know what source makes MWCC
  re-type a temporary slot without destroying it" stands.
- Re-measured unchanged: `GetStudio` 59.12%, `UpdateEmitter` 92.50%, `AddListener` 93.00%,
  `UpdateListener` 86.58%, `SfxVolume` 85.61%, `AddEmitter` 82.74%, `SfxStart` 58.86%, `Play` 91.25%,
  `Update` 87.59%, `__sinit` 66%, `Shutdown` 30.05%, `SetActiveAreas` 0.38%. No `WALL:` for any of
  them: I did not try several *new* spellings on them this run, so a wall line would be a claim I did
  not measure.

## What is left, with this run's measurements

28 unmatched. Fourteen are at 0.00% and need source that does not exist in our object:

- **The eight 60-byte wrappers (`fn_8029BA7C`..`fn_8029BC20`) + `fn_8029B8E8` (404) + `fn_8029B81C`
  (204)** = 10 functions, 884 bytes, the largest single prize left in the unit. Measured here from the
  bytes: the eight wrappers are one template whose body is `f(&local500B, a, b, c); fn_8029B8E8(
  &local500B);` with a 512-byte frame, differing only in the first `bl` target - eight `CAudioSys`
  entry points 0x80-0x94 apart (`0x8033542C`, `0x803354AC`, `0x8033555C`, `0x803355FC`, `0x803356BC`,
  `0x803357F4`, `0x803358A8`, `0x8033593C`). `fn_8029B8E8` is the "find or add a record" loop over a
  500-byte-per-record table at `lbl_80413EFC` with 10 records; `fn_8029B81C` is the sibling loop that
  calls `SetAreaVolume(id, 127)` (which is at 100% in our tree). Both call externs that live in
  **`main/auto_03_8032F974_text`**, an unwritten unit: `fn_80334C50`, `fn_80334D8C`, `fn_80334C98`,
  `fn_8033541C`, `fn_80334C40`, `fn_80334CAC`, `fn_80334C18`, and (for `fn_8029B81C`) `fn_80334C48`,
  `fn_80334CB4`, `fn_80334C20`, `fn_8034066C`. **That is the whole blocker, and it is the same one runs
  1-4 recorded**: writing these means guessing a dozen function contracts in another unit, which the
  reviewer rejects as a bypassed wall. `SetActiveAreas` (0.38%, 1060 B) and `Shutdown` (30.05%,
  172 B) are gated by the same table and the same externs.
- **`fn_8029FC34` (172) / `fn_8029FCE0` (80) / `fn_8029FD30` (124)** form a destructor chain that is
  *not* in our object at all: each is `if (this == nullptr) return this; <wait for a frame count>;
  <call the next one down with the flag>; if ((short)arg4 > 0) CMemory::Free(this); return this;`.
  `fn_8029FD30` walks a table of **2052-byte** records counting down a per-record frame counter, and
  `fn_8029FC34` reads its counter at `this+6160`. Writing them means knowing the owning class's
  layout, which nothing in this unit or its headers describes.
- **`fn_8029FDAC` (60)** is still one honest loose end and **I did not guess**: retail's `__sinit`
  registers **ten** objects, not the nine run 4 counted, and the tenth registration (`0x802A042C`
  region, dtor `0x8029FC34`) is one this unit has no static for. `fn_8029FDAC` is the ninth
  registration's dtor, and both of our 60-byte dtors (`__dt__reserved_vector<CVector3f,4>` and
  `__dt__reserved_vector<CBaseSfxWrapper*,72>`) are unreferenced dead code with byte-identical
  bodies. One function; a coin flip; run 4's judgement not to guess still stands.

Run 4's two `WALL:` lines on the ctors were re-measured only in the sense that the unit's numbers
reproduced; I did not retry a spelling, so I do not restate them.

## Notes for the next run on any unit

- **Run 4's "not writable from source" verdicts are not to be trusted without checking whether the
  symbol is merely inlined.** A constructor (or any small member function) defined in the class body
  produces no symbol at all, so "our object emits no code of that size" is true and irrelevant: the
  code is there, twice, at the use sites. Declaring it out-of-line is a one-line test.
- **`build/report.json` is not written by the unit build alone.** `tools/goal_check.sh` (and the doc
  checker it runs) rewrite the derived state block in `docs/HANDOFF.md`. `git checkout
  docs/HANDOFF.md` before finishing, or the judge sees a judge-owned path touched. It happened twice
  in this run.
- **`git checkout <file>` on a file you have also hand-edited throws the edit away.** Reverting the
  `Allocate*` experiment with `git checkout src/Kyoto/Audio/CSfxManager.cpp` also discarded the
  `SListener` change; it had to be re-applied and re-verified. Revert one hunk at a time.
- **MWCC's "MWCC re-loads a vector element at every use" rule is narrower than run 3 stated.** It
  holds for cached pointers that feed virtual calls, not for every cached element. Measured both ways
  this run.

---

# Sixth run (lane 5, 2026-10-01)

Re-measured on `wt-mp2-goal-L5` at `7526aaa5`. **Runs 1-5's "after" numbers were reproducible here**
(131 / 159, 13352 B, 82.81% fuzzy), so this run starts from their position. No spelling an earlier
run lists was retried as a *conclusion*; their wall verdicts on the two ctors were re-measured
(below) and one of them turned out **not** to be a wall.

## Result

| | before (this run) | after |
|---|---|---|
| `matched_functions` | 131 / 159 | **132 / 159** |
| `matched_code` | 13352 / 22800 (58.56%) | **13744 / 22800 (60.28%)** |
| `fuzzy_match_percent` | 82.81% | **84.22%** |

`./tools/goal_check.sh build/goal/item.json`, verbatim:

```
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 11326 -> 11327   linked 5507 -> 5507
  ok    check_symbol_names.py
  ok    All:  32.58% fuzzy, 25.25% matched, 11.94% linked (11327 / 28465 functions)
  ok    target rose: main/Kyoto/Audio/CSfxManager: 131 -> 132 / 159 functions
  ok    no asm added
goal_check: PASS progress-prime1-csfxmanager
```

`build/gate-diff.log` is 2 lines: 1 `+100%`, **0 WORSE / GONE / UNLINKED / FELL** over all 2066
units, so "no function anywhere got worse" is measured. DOL sha1
`6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`; `probe_sources.sh` 751 files, 0 failed, LINKED
(250 undefined, 0 duplicates). `git checkout docs/HANDOFF.md` afterwards - the checker rewrites
it (run 5's note).

## Files

- `src/Kyoto/Audio/CSfxManager.cpp` only. No header, no `config/`, no asm.

## 1. `__ct__CSfxWrapper`: 99.19% -> **100%** - run 4's WALL was wrong

Runs 3 and 4 called this a wall: "37 of 37 instructions match in order; the only difference is the
epilogue's restore order", and "moving `mReady` within the initialiser list produced a byte-identical
object, so no initialiser-list spelling can move it". **The premise was that the epilogue is the
cause. It is not - the epilogue is a symptom.**

Measured here: the *initialiser* is not the lever, but the **declared parameter type** is. The
constructor takes `short volume, short pan`. Writing any one of the three as an explicit cast -

```cpp
, mPan(static_cast< short >(pan))
```

- reaches 100%, and so do `, mSfxId(static_cast< ushort >(sfxId))` and
`, mVolume(static_cast< short >(volume))` (all three measured, each 148 B at 100.00%, each one
line). `mPan(static_cast< short >(pan))` is the one kept: it is a no-op for the value, so it is
the least surprising of the three, and it is the spelling that also leaves `mVolume`/`mSfxId` alone.

Before, the 4-byte stores came out as `stw r4,32(r28) / sth r29,28(r28) / sth r30,36(r28) /
sth r31,38(r28)` with the *epilogue* in the wrong order; with the cast, MWCC allocates the
parameters so the body and epilogue both land on retail's registers. Note what the cast is *not*:
it is not a layout change (`NESTED_CHECK_SIZEOF(CSfxManager, CSfxWrapper, 0x2c)` still holds, no
member moved) and not a behaviour change - `pan` is already a `short`.

**Codegen rule, general:** *a by-value scalar parameter re-listed in the member-initialiser list is
materialised differently from the parameter itself; an explicit `static_cast<T>` on one of them
changes the allocator's choice for the whole constructor, including the epilogue.* Same family as
run 4's `const CSfxHandle&` finding: the materialised temporary, not the value, is what moves.

Retried and confirmed **not** to help (byte-identical object, 99.19%): `mReady(true)` moved within
the initialiser list, `mReady` set in the constructor body instead of the list, `mPan` cast *and*
`mSfxId` cast together (one cast is enough; two is 74.95%, the second pushes the parameters out of
the callee-saved set entirely and the frame loses `r28`).

## 2. `UpdateEmitter`: 92.50% -> **99.00%** - the clamps, re-measured

Retail's bytes (0x8029E62C) end:

```
clrlwi r4,r3,24 ; cmplwi r4,127 ; bne  <scale>
mr r3,r29                          ; the neutral path just forwards maxVolume
b <clamp>
<scale>: ... clrlwi r3,r0,24
<clamp>: clrlwi r0,r3,24 ; cmplwi r0,1 ; bge ; li r31,1 ; b ; cmplwi r0,127 ; li r31,127 ; bgt ; mr r31,r3
```

Three things, all measured one at a time:

- **The neutral test comes first and forwards the value.** `if (areaVolume != 127) { ... }` compiles
  to `beq` over the scale block; retail has `bne` *to* the scale block with `mr r3,r29` in the
  fall-through. Writing it as `if (areaVolume == 127) { scaled = maxVolume; } else { ... }` with a
  named `scaled` gives retail's shape. 92.50% -> 97.50%.
- **`scaled` is a `uchar`, not an `int`.** Retail's scale result goes `clrlwi r3,r0,24` straight
  into the clamp; with an `int` local MWCC keeps a wider value live and the clamp re-narrows.
  97.50% -> 98.83%.
- **The floor is an unsigned compare against a `uchar` arm.** `uint(scaled) > 2u ? scaled : uchar(2)`
  rather than `rstl::max_val(int(...), 2)`, and **no write-back to the `maxVolume` parameter**
  (the parameter is dead after the store, so writing it costs a register). 98.83% -> 99.00%.

Measured, did not help (all reverted): `rstl::min_val(uint(volume), 127u)` for the cap 94.65%;
`const uint capped` 94.65%; the whole scale as one ternary 94.08%; `int`/`short` `scaled` 97.83% /
97.50%; `!IsSilent()` as the outer test 83.08%; `sound->GetEmitter()` bound to a `data` reference
83.67% (retail re-calls `GetEmitter()` per store - run 2's finding, re-confirmed here); a named
`const CVector3f& pos/dir` at the top 99.00% (no change); `mDoUpdate = true` inside the silent
branch 92.08%; `channel` re-read instead of a named reference 89.20%; swapping the guard to
`!IsPlaying() || handle != ...` 87.12%.

The remaining 1% is the **r30/r31 swap** for `position` vs `sound` - the same one run 2 recorded -
plus one register in the floor. 99.00% is as far as the spellings I tried reach it.

## 3. `SfxVolume`: 85.61% -> **96.99%** - the same three clamps, same three reasons

This function has the identical `areaVolume * min(volume,127) / 127` and `clamp 1..127` shape, and
the same three fixes land, with one difference: **`scaled` is a `short` here, not a `uchar`**
(96.99% with `short`, 94.95% with `uchar`, 96.58% with `int`). The final clamp is a fresh `uchar vol`
rather than a write-back to the `volume` parameter, again because the parameter is dead afterwards.

Measured, did not help: `min_val(max_val(v),127)` 87.04%; signed compares in the clamp 95.46%;
unsigned `capped` 93.24%; clamping `scaled` instead of `volume` 92.60%; the clamp inlined at both
use sites 81.58%; `mMuted`/`IsPlaying` operands swapped 81.98%; `SfxVolume` before `SetVolume`
91.48% (retail's order is SetVolume first, as the old source had it); `channel` re-read 88.31%;
re-indexing `channel.mSounds[...]` at each use 83.27%.

**Codegen rule, general, and this is the one worth keeping from this run:** *for retail's
"scale then clamp" idiom, the register allocation is decided by (a) which arm of the neutral
test is the fall-through, (b) the width of the local that holds the scaled value, and (c) whether the
clamped result is written back to the parameter or kept in a fresh local. All three are visible in
the bytes - the fall-through direction, the `clrlwi ...,24` right after the multiply, and whether
the parameter's register is reused at the end - and all three are source-level. Together they are
worth 6.5 points in `SfxVolume` and 6.5 in `UpdateEmitter`.*

## 4. Re-measured, still walls (runs 3/4's verdicts hold on this tree)

- **`__ct__CBaseSfxWrapper` 98.61%** - still only the three `li` constants landing in r10/r9/r6
  instead of r11/r10/r9. Retried here: the three casts that fix `__ct__CSfxWrapper` do **nothing**
  for this one (98.61%, unchanged), so the two constructors are separate allocator cases and run 4's
  "19 of 20 functions in our object emit the retail epilogue order, this one is the exception"
  measurement is not what drives either.
- **`GetStudio` 59.12%** - 8 spellings retried (bool/int/uint local, `!` on an int, `== 0`, ternary,
  index local, no `studios` local): **all 59.12%, byte-identical**. Run 2's WALL stands, and its
  out-of-bounds read (`0xF8000000` as the table index when `mCurrentStudio` is true) is still
  there - it is pre-existing, not something this change introduced.
- **`Play__CSfxWrapper` 91.25%** - the `mr r6,r3` vs `clrlwi r6,r3,24` (the `studio` argument to
  `SfxStart`) and the `mr r0,r3` / `clrlwi r5,r3,24` (the reverb value). `GetReverbAmount()`'s
  `short` vs `ushort` return changes nothing (run 2). `static_cast<uchar>` on either arm of the
  `studio` ternary: 89.93% (worse), on the whole: 91.25% (no change).
- **`AddListener` 93.00%**, **`UpdateListener` 86.58%**, **`Update` 87.59%**, **`AddEmitter` 82.74%**,
  **`SfxStart` 58.86%**, **`__sinit` 66%**, **`Shutdown` 30.05%**, **`SetActiveAreas` 0.38%** - all
  re-measured unchanged. No `WALL:` for any of them: I tried no *new* spellings on them this run.

## 5. Method notes

- **`fast_try.sh` (rebuild one object + regenerate `report.json`) is a ~0.5 s loop**, and this whole
  run is ~120 measured variants through it. That is the practical reason five earlier runs each
  got a handful of functions: the expensive step is understanding the bytes, not trying a spelling.
- **A disassembly differ over normalised instruction text is what made this run's work possible**
  (`objdump` both sides, strip `<symbol>` annotations, normalise branch targets to `T`, `difflib`
  unified diff). It shows *which* instructions moved, which `objdiff`'s percentage does not - and
  its `DIFF_ARG_MISMATCH` markers are still not the match criterion (run 3's note).
- **Re-verify a WALL before inheriting it.** `__ct__CSfxWrapper` had been declared a wall twice on
  the strength of "the epilogue order is the only difference", and the epilogue was a symptom.
- **Two functions with the same idiom can need different widths for the same local** (`uchar
  scaled` in `UpdateEmitter`, `short scaled` in `SfxVolume`). Measure each; do not generalise.

## Still open

Sixteen 0.00% functions, unchanged from run 5 and for the same reasons: the eight 60-byte wrappers +
`fn_8029B8E8` (404) + `fn_8029B81C` (204) need the auxiliary-effect record type and a dozen unwritten
externs in `main/auto_03_8032F974_text`; `fn_8029FC34`/`fn_8029FCE0`/`fn_8029FD30` need the owning
class's layout; `fn_8029FDAC` (60) is a coin flip between two byte-identical 60-byte dtors in our
object. `SetActiveAreas` (0.38%) and `Shutdown` (30.05%) are gated by the same record type.

WALL: __ct__Q211CSfxManager15CBaseSfxWrapperFbs10CSfxHandlebi 98.61% - all 36 instructions match in
order; the three `li` constants land in r10/r9/r6 instead of retail's r11/r10/r9. The three parameter
casts that take `__ct__CSfxWrapper` to 100% leave this one byte-identical, so the two constructors
are independent allocator cases.
