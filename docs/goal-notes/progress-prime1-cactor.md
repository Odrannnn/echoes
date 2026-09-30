# progress-prime1-cactor

`kind: progress` · `target: MetroidPrime/CActor` · worktree `../wt-mp2-goal-L1` (lane 1) · 2026-09-30

**Result: `main/MetroidPrime/CActor` 56 -> 59 matched functions** (`All:` 9513 -> 9516, `linked`
unchanged at 4853). Three functions went to 100%; nothing else in the unit, or in any other
unit, changed by a single byte. The unit stays `NonMatching`; no `flip_test` was run and no
`configure.py`/`config/`/`asm` file was touched. Only `src/MetroidPrime/CActor.cpp` is modified.

```
tools/goal_check.sh build/goal/item.json
  ok    no judge-owned path touched
  ok    gate.sh (DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 9513 -> 9516   linked 4853 -> 4853
  ok    check_symbol_names.py
  ok    All:  29.20% fuzzy, 21.35% matched, 11.64% linked (9516 / 28465 functions)
  ok    target rose: main/MetroidPrime/CActor: 56 -> 59 / 98 functions
  ok    no asm added
goal_check: PASS progress-prime1-cactor
```

## The three that landed

All three are **Prime 1's `CActor.cpp` shape** adapted to this repo's accessors - the item's
suggestion was right, and each needed a *different* edit, not a transcription.

| function | before | after | edit |
|---|---|---|---|
| `CanDrawStatic__6CActorCFv` | 58.08% (152 B) | **100%** | Prime 1's three-statement `if` form, not one `return A && B && C && D;` |
| `InFluidId__6CActorCFv` | 44.50% (48 B) | **100%** | `if (empty) return kInvalidUniqueId; return back();` instead of a `?:` |
| `RemoveInvalidFluidIds__6CActorFR13CStateManager` | 89.49% (156 B) | **100%** | the `if`/`else` **arms swapped** (`!ObjectById` first) |

### `CanDrawStatic` - one boolean expression is not a chain of statements

Ours was a single expression. Retail's 38 instructions test `GetActive()`, then
`GetModelData()`, then `mDrawFlags`, and then test `IsNull()`/`HasAnimation()` **twice** -
which one `&&` chain cannot produce. Prime 1's shape does:

```cpp
if (!GetActive() || !HasModelData() || static_cast< char >(mDrawFlags.GetTrans()) > 4) {
  return false;
}
const CModelData* modelData = GetModelData();
if (modelData->IsNull() || modelData->HasAnimation()) {
  return false;
}
return true;
```

(Prime 1 writes `GetBlendMode()`; this repo's `CModelFlags` only has `GetTrans()`, and retail
Echoes reads the byte at `0x100` either way, so `GetTrans()` is the right accessor here.)

### `InFluidId` - a `?:` collapses where an early return does not

Ours emitted a **load from address 0**: the compiler merged both arms into one `lhz r0,0(r4)`
with `r4 = 0` on the empty path, because it saw `kInvalidUniqueId` and the vector's element
load as the same expression. Retail keeps two paths, one of them a relocated
`R_PPC_EMB_SDA21 kInvalidUniqueId` load. Writing the early return stops the merge. This is
worth knowing on its own: **the old code was a latent null dereference**, not just a byte
mismatch.

### `RemoveInvalidFluidIds` - branch polarity

Both bodies are the same two statements; only the order of the `if` arms differs, and that is
enough. MWCC makes the *first* arm the fall-through; retail falls through into
`mFluidIds.erase(it)` and branches over it. Prime 1's order is the opposite of Echoes' here.

```
ours   : if (obj) { ++it; } else { it = erase(it); }   -> branch TO the erase block
retail : if (!obj) { it = erase(it); } else { ++it; }  -> branch OVER the erase block
```

## What did not work (spellings tried, so the next run skips them)

**`GetOrbitPosition`, `GetAimPosition` (70.57%), `GetSortingBounds` (82.00%) - a measured
wall.** Retail copies the struct to the return slot **one float at a time, in order, with a
single FPR**:

```
lfs f0,0x54(r4) ; stfs f0,0(r3) ; lfs f0,0x58(r4) ; stfs f0,4(r3) ; ...
```

MWCC 2.7 always emits the **two-FPR interleaved** form instead
(`lfs f0; lfs f1; stfs f0; lfs f0; stfs f1; stfs f0`). I probed this directly with
`tools/probe_cc.sh` on a synthetic class - the two-FPR form comes out of *every* spelling:

- `return m;` / `return CVector3f(m.GetX(),m.GetY(),m.GetZ());` / `const CVector3f v = m; return v;`
  / `CVector3f v; v = m; return v;` / field-by-field `SetX/SetY/SetZ` / a user-declared copy ctor /
  a private `operator=` / a user-declared dtor / an in-class inline helper / a 6-float `CAABox`
  equivalent / the member at offset 84 / a base-class member / `const_cast` / `mutable` *is* the
  only lever - and it needs the **member** to be `mutable` or the **method** non-const.

The mechanism is aliasing: MWCC hoists the loads above the stores only when it can prove
`*sret` does not alias `this`, and a `mutable` member or a non-const `this` removes that proof.
Retail's symbol is `GetOrbitPosition__6CActorCFRC13CStateManager` - **const** - so the retail
source cannot be reaching it that way, and I could not find a const-method spelling that
reproduces it. Declaring `mPosition`/`mRenderBounds` `mutable` in `include/MetroidPrime/CActor.hpp`
*does* produce retail's bytes, but it is a shared header and would change codegen for every
unit that touches those members, so I did not do it. **The next thing to try is a member-level
`mutable`, measured against the whole report, not just CActor.**

**Prime 1's source verbatim, where Echoes forked (all measured worse or equal):**

| spelling tried | function | result |
|---|---|---|
| Prime 1's `GetYaw` (`sqrt`, `double ret = -atan2(...)`, no `CMath::SqrtF`) | `GetYaw` | **0.00%** (104 B) - completely different; keep `CMath::SqrtF` + `atan2f` |
| Prime 1's `IsModelOpaque` as an early-return chain instead of `else if` | `IsModelOpaque` | 95.24% -> 95.24%, no change |
| Prime 1's `SetModelData` (no `DeleteAllLights` call) | `SetModelData` | Echoes *does* have the call; keep ours |
| `if (!data.IsNull()) { new } else { free }` (polarity swap) | `SetModelData` | 90.93% -> **40.51%** |
| `if (!data.IsNull()) ... else if (...) ... else ...` | `SetModelData` | 90.93% -> **32.03%** |
| `const TUniqueId id = kInvalidUniqueId;` hoisted above the `switch` | `OnScanStateChange` | 99.79% -> **71.38%** (the load is hoisted out of all three arms) |
| `return GetTranslation();` / `return CVector3f(mPosition.GetX(),...)` | `GetOrbitPosition`, `GetAimPosition` | 70.57% -> 70.57% |
| `mValidTargetPlayers &= ~(1 << i) & 0xFu;` | `SetValidTarget` | 81.90% -> 76.19% |
| `mValidTargetPlayers = (mValidTargetPlayers & ~(1 << i)) & 0xFu;` | `SetValidTarget` | 81.90% -> 81.90% |
| `mTargetableVisorFlags &= ~flags & 0xFu;` | `SetVisorOrbitableFlags` | 83.75% -> 76.56% |
| `mTargetableVisorFlags = (mTargetableVisorFlags & ~flags) & 0xFu;` | `SetVisorOrbitableFlags` | 83.75% -> 83.75% |
| `... ? mEchoVolume : mNormalVolume` with a trailing `& 1` | `GetVisorSoundVolume` | 84.96% -> **81.21%** |

**A recurring, unexplained pattern: MWCC hoists the function's first load above the
callee-saved-register stores, retail does not.** This alone blocks `Render` (95.92% - 5 of 98
instructions differ, all of them the position of one `lwz r4,96(r3)`), `IsModelOpaque`
(95.24% - one `lbz`), `AddToRenderer` (98.97% - one `lwz` plus the order of the two
`GetShadow()` argument evaluations), `CanRenderUnsorted` (89.19% - one `lwz`/`li`), and
`GetLocatorTransform` / `GetScaledLocatorTransform` x2 (83.33% - one `lwz`). Every one of them
is instruction-for-instruction identical to retail apart from that hoist. I could not find a
source-level lever: the early-return reformulation, the Prime 1 spelling and the local-variable
spelling all leave the hoist in place.

**Two more, recorded so they are not re-derived:**

- `GetRenderAlphaBufferAlpha` (98.15%): retail materialises the `TUniqueId` argument **once** at
  `r1+8`; ours materialises it at `r1+8` *and* copies to `r1+12`, which is why ours is one
  instruction longer. `OnScanStateChange` (99.79%) is the same thing three times over - ours
  gives the three `SendScriptMsgs` calls three different outgoing slots (`r1+16/12/8`) and
  needs a 32-byte frame where retail needs 16. Both point at MWCC coalescing the by-value
  `TUniqueId` temp with the outgoing-argument slot in retail and not in this build. Changing
  `CPlayerTargeting::GetScanTargetIndex`'s second parameter to `const TUniqueId&` is the obvious
  experiment but it changes a shared header's symbol, so I did not try it blind.
- `SSound::SSound` (98.75%, 32 B) is a **register-allocation** difference only: ours
  `lwz r4,0(r4)` (destroys the incoming parameter register), retail `lwz r7,0(r4)`. Two of
  eight instructions. Same class of difference in `GetYaw` (11/26, identical instruction set) and
  `PlayCustomSound` (29/45, identical instruction set, only r27-r30 assignment differs).

`GetDistanceToCamera` (82.08%) is a genuine **register-pressure** difference, not scheduling:
retail's loop is 59 instructions in a 96-byte frame with no stack traffic, ours is 65 in 112
bytes and spills the three component deltas to `r1+8/12/16` and reloads `this->x`. The
instruction order of the three subtractions is already identical to retail's (y, x, z), so the
source would have to be restructured to hold one fewer value live, not respelled.

## Files touched

- `src/MetroidPrime/CActor.cpp` - `CanDrawStatic` (line ~848), `InFluidId` (line ~862),
  `RemoveInvalidFluidIds` (line ~868). 15 insertions, 6 deletions, one file.

## How it was verified

Per-function percentages come from `build/report.json` after `./tools/decomp_build.sh`, read
before and after each change and diffed, so "no other function moved" is a measurement rather
than an assumption. `tools/goal_check.sh build/goal/item.json` is the judge and it exits 0.
`docs/HANDOFF.md` is modified in the worktree only because `goal_check.sh` sets
`MP_GATE_DOCS_WRITE=1` and `check_docs_claims.py --write` re-derives the state block; I did not
edit it, and the driver discards it.

## Notes for the next run

- The `?:` -> early-return and if/else-arm-swap edits are cheap and general. Any function in
  this unit (or another) whose retail code loads a relocated `SDA21` constant on one arm and a
  struct element on the other is the `InFluidId` pattern; any loop whose retail body branches
  *over* the second arm is the `RemoveInvalidFluidIds` pattern.
- The cheap instrumentation is worth rebuilding: `objdiff-cli diff` only reports section-level
  percentages, so I wrote a small instruction-pair differ over
  `build/G2ME01/src/<unit>.o` vs `build/G2ME01/obj/<unit>.o` (dtk's retail object), plus a
  per-function percentage snapshot/diff. Both are throwaway; re-deriving them cost most of an
  hour. `tools/probe_cc.sh` + a synthetic class is the fast way to settle a codegen question
  without rebuilding the unit, and it is worth doing **before** touching the real source.

---

# progress-prime1-cactor - second run (lane 6, worktree `../wt-mp2-goal-L6`, 2026-09-30)

**Result: `main/MetroidPrime/CActor` 59 -> 60 matched functions** (`All: 31.22% fuzzy, 23.52%
matched, 11.82% linked`). Whole-DOL `matched` 10243 -> 10244, `linked` 5025 -> 5025, no function
anywhere regressed (`tools/report_diff.py` over all 2057 units). The unit stays `NonMatching`; no
`flip_test` was run, no `configure.py`/`config/`/`asm` file was touched, and nothing under `tools/`,
`docs/` or `build/goal/` was edited except this notes file. Two files changed:

```
include/Kyoto/Audio/CSfxHandle.hpp   -2 +12   (drop the user-declared operator=, add the note)
src/MetroidPrime/CActor.cpp          +6 -6     (RemoveLoopedSoundAt, StopLoopedSound)
```

```
tools/goal_check.sh build/goal/item.json
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 10243 -> 10244   linked 5025 -> 5025
  ok    check_symbol_names.py
  ok    All:  31.22% fuzzy, 23.52% matched, 11.82% linked (10244 / 28465 functions)
  ok    target rose: main/MetroidPrime/CActor: 59 -> 60 / 98 functions
  ok    no asm added
goal_check: PASS progress-prime1-cactor
```

**Nothing here came from Prime 1.** The item's suggestion to read Prime 1's `CActor.cpp` does not
apply to what was left: Prime 1 has no looping-sound system at all (`grep -rn StopLoopedSound` in
`prime-ref` finds only `CAtomicBeta::StopLoopedSound(CSfxHandle&)`, an unrelated method), so
`StopLoopedSound(s)`, `RemoveLoopedSoundAt`, `AddLoopedSound`, `PlayLoopedSound` and the four
`CFluidHeightCompare` sort instantiations have no Prime 1 counterpart. Of the 13 functions the
item listed as sharing a Prime 1 name, the previous run had already taken the three that were
reachable; the ones still open are the ones its notes recorded as register-allocation walls.

## The one that landed: `RemoveLoopedSoundAt` 31.58% -> 100%

It needed **two** independent changes, and neither is visible in the source diff alone.

### 1. `CSfxHandle`'s user-declared `operator=` is what stops a block copy

Retail copies the 8-byte `SSound` as **two words** - `lwz r5,4(r6)` / `lwz r0,8(r6)` / `stw r5,4(r7)`
/ `stw r0,8(r7)` - with the pair's `first` copied separately as `lhz`/`sth`. Ours copied it member by
member (`lwz` + `lbz` + `lbz`). Nothing in `CActor.cpp` reaches that: the difference is in
`include/Kyoto/Audio/CSfxHandle.hpp`, which had

```cpp
void operator=(const CSfxHandle& other) { mID = other.mID; }
```

A user-declared copy assignment on a member makes MWCC 2.7 expand the enclosing struct's assignment
member by member. Removing it (the implicit operator= does exactly the same thing) restores the
2-word move. Measured with a synthetic probe over the real headers
(`.tmp`-style throwaway, `reserved_vector<pair<ushort,SSound>,4>::operator=`):

| element type | assignment MWCC 2.7 emits |
|---|---|
| `SSound` alone (4+1+1) | `lwz/stw`, `lbz/stb`, `lbz/stb` - member-wise |
| `{ushort, uint}` | `lhz/sth`, `lwz/stw` - block |
| `{ushort, SSound}` with `CSfxHandle::operator=` | `lhz/sth`, `lwz/stw`, `lbz/stb`, `lbz/stb` - member-wise |
| `{ushort, SSound}` without it | `lhz/sth`, `lwz/lwz/stw/stw` - **retail's shape** |

The header is shared (17 `.cpp` files include it), so this was measured against the **whole report**,
not just CActor: `tools/report_diff.py .tmp/.../report_base.json build/report.json` prints
`matched 10243 -> 10244   linked 5025 -> 5025   (+1 functions at 100%)` and `no regression`. No
other unit's count moves and no function anywhere drops. **Construction is unaffected** - the copy
*constructor* still goes member by member, which is why `reserved_vector<SSound,2>`'s fill
constructor still matches at 100% and only the *assignment* changed shape.

### 2. The loop variable's signedness and where it starts

```cpp
// ours, 31.58%: 24 retail insns vs 26 ours, member-wise copy + wrong induction variables
for (int i = index; i + 1 < mLoopingSoundCount; ++i) {
  mLoopingSounds[i] = mLoopingSounds[i + 1];
}

// retail, 100%: 24 insns, byte for byte
for (uint i = index + 1; i < mLoopingSoundCount; ++i) {
  mLoopingSounds[i - 1] = mLoopingSounds[i];
}
```

Retail's preheader is `addi r9,r4,1` / `mulli r4,r9,12` - it starts the induction variable at
`index + 1` and strength-reduces **`i * 12`** (the *source* index), recomputing `(i - 1) * 12` with a
`mulli` for the destination. Our old form started at `index`, compared `i + 1 < count` (so the
`+1` was recomputed in the loop's test block) and strength-reduced the *destination pointer*
instead. **Both halves matter and they are not independent**: the same shifted form written with
`int i` compiles to 42 differing instruction lines - worse than the old form - because `int` makes
MWCC recompute the address with `mulli` from the index instead of strength-reducing anything.
`uint` + shifted start + `i - 1` / `i` is the only combination measured that reaches 0.

Spellings measured on this function (differing instruction lines against retail, lower is better):

| spelling | lines |
|---|---|
| `int i = index; i + 1 < n; a[i] = a[i+1]` (was 34) | 34 |
| `uint i = index + 1; ...; a[i-1] = a[i]` | **0** |
| `int i = index + 1; ...; a[i-1] = a[i]` | 42 |
| `uint i = index; i + 1 < n; ...` (with the header fix) | 20 |
| `uint i = index + 1` with `TLoopingSound& dst/src` locals | 42 |
| `uint i = index + 1` with a `TLoopingSound* p` walk | 39 |
| `member-wise` (`a[i-1].first = ...; a[i-1].second = ...`) | 26 |
| field-by-field through the `SSound` members | 28 |
| a `const SSound s = ...; a[i-1].second = s;` temporary | 41 |

## Also improved, and kept: `StopLoopedSound` 80.83% -> 94.66%

Retail compares the pair's `first` **through the index** - `lhzx r0,r5,r3` - and only materialises
the element pointer (`add r31,r5,r3`) on the matching path. Ours bound `TLoopingSound& sound =
mLoopingSounds[i]` at the top of the body, so the pointer was computed unconditionally and the
`.first` load could not use indexed addressing.

```cpp
if (mLoopingSounds[i].first == sfxId) {
  TLoopingSound& sound = mLoopingSounds[i];
  if (const CSfxHandle& handle = sound.second.mHandle) {
    CSfxManager::RemoveEmitter(handle);
  }
  ...
}
```

Both halves are needed and each is measurable on its own: the first fixes the `lhzx` and drops the
function from 60 to 58 instructions (80.83% -> 91.90%), and binding the handle to a
`const CSfxHandle&` removes the second `lwz r0,4(r31)` that MWCC was re-issuing for the argument to
`RemoveEmitter` (91.90% -> 94.66%).

WALL: StopLoopedSound 94.66% - 15 differing lines are all the loop induction variable, ours in r6
with a `mr r30,r6` copy per iteration where retail keeps it in r30; six loop shapes (`while`,
`for` with a cached bound, `uint`/`int`/`size_t` index, `static_cast<int>(i)` at the call, the
index declared outside the loop) all leave the copy in place.

WALL: StopLoopedSounds 99.17% - 6 differing lines, ours keeps the array base in r29 and the element
pointer in r30 where retail has them the other way round; `int` index, a vector reference, a
`&mLoopingSounds[0]` data pointer and a `TLoopingSound*` local all make it worse (14-46 lines).

## Measured and NOT changed (so the next run skips it)

- **The recurring "first load hoisted above the callee-saved-register stores" wall is real and I
  re-confirmed it on three more functions.** `GetLocatorTransform(const rstl::string&)` and both
  `GetScaledLocatorTransform`s (83.33%, 12 insns), `SetActorLights` (91.30%, 23 insns) and `GetYaw`
  (69.23%, 26 insns) are all instruction-for-instruction retail apart from one load moving across
  the prologue. New spellings tried this run, all still hoisted: a named local for the model data,
  a named local for the returned `CTransform4f`, `mModelData->`, `mModelData.get()->`, a
  `static_cast<CTransform4f>` around the return, `lights.release()` into a local, and
  `mCalculateLighting = !false`. There is no source-level lever I could find; treat this as MWCC
  2.7 scheduling, not as a missing expression.
- **`GetVisorSoundVolume` (84.96%): the member mapping is now known and ours is wrong.** Retail's
  three `lbz` are at **313** for the `fn_80036F10()`-true arm, **314** for the echo-visor arm and
  **312** for the fallback; ours returns `mMaxVol` (312) first and `mNormalVolume` (313) last. The
  `CActor` constructor pins 312 = `CAudioSys::kMaxVolume` and 313 = `params.GetMaxVolume()`, so
  retail's early return is `mNormalVolume` and its fallback is `mMaxVol` - i.e. swapping the two
  members in our source makes all three offsets match. It buys only 84.96% -> 85.00% (the branch
  layout still differs: retail branches *over* the fallback block with `bne`, and reloads 314 into
  r31 then masks it with `clrlwi r3,r31,24` where ours loads it straight into r3), so I reverted it
  rather than put a semantic change with no measurable gain into this item's diff.
- **`__ct__reserved_vector<pair<ushort,CActor::SSound>,4>(int, const pair&)` is still 0.00%** and is
  the one function in this unit where a *whole missing behaviour* is visible: retail inlines the
  element copy, ours calls an out-of-line `uninitialized_fill_n<pair*,pair>`. Probed with the real
  headers: MWCC inlines that helper for `SSound` (16 insns) and for `{ushort,uint}` (2) but not for
  `{ushort,SSound}` (17), and the outlined body is byte-identical to retail's inline one, so the
  difference is purely MWCC's inline decision. **Not reachable from `CActor.cpp`** - it needs a
  change in `rstl/construct.hpp` / `rstl/pair.hpp` and I found no lever. Per the brief this is a
  measured wall, not a `NEW:` item.
- **`SSound::SSound` (98.75%) is 2 of 8 instructions**: retail `lwz r7,0(r4)`, ours `lwz r4,0(r4)`.
  Init-list order (`mHandle, mLocator, mUseEchoVolume`), all three permutations, a body of
  assignments instead of a mem-init list, and a mem-init list plus a body assignment for the
  bitfield all leave `r4` in place.
- **`SetValidTarget` (81.90%) / `SetVisorOrbitableFlags` (83.75%)**: retail **reloads the byte**
  (`lbz r5,339(r3)` for the read and `lbz r0,339(r3)` for the write-back) and puts `1 << playerIndex`
  in the *argument* register r4; ours keeps the byte in r5 for both and shifts into r0. New
  spellings this run: a local for the whole 4-bit field, an explicit `x = x | y` instead of `|=`, a
  local for the bit, and an inverted early return - all 19-25 differing lines. (The previous run's
  `& 0xF` variants are in the run above.)
- **`AddToRenderer` (98.97%) is one instruction**: retail reloads `lwz r31,192(r30)` (= `mSimpleShadow`,
  0xC0) immediately before the `AddDrawable` call, ours reuses the earlier load. The intervening
  `GetShadow()->GetTransform().GetTranslation()` is an out-of-line call, so this is MWCC choosing not
  to reload, not a missing expression; our source already spells `GetShadow()` three times in that
  statement and MWCC CSE'd all three.
- **The `TUniqueId` by-value temp** (`GetRenderAlphaBufferAlpha` 98.15%, `OnScanStateChange` 99.79%):
  still one extra `sth`. Unchanged from the previous run's finding; I did not touch
  `CPlayerTargeting::GetScanTargetIndex`'s signature.

## Rebuilding the instrumentation - do this first, it is worth an hour

There is no per-instruction differ in `tools/`, and re-deriving one is most of the cost of a run like
this. What is needed, all of it throwaway (put it under `.tmp/`, which is gitignored):

1. **Per-function percentages already exist**: `build/report.json` has a `functions` array per unit
   with `fuzzy_match_percent` and `size`. No objdiff JSON, no separate tool.
2. **A single-unit recompile in ~0.9 s** - take the flags out of `build.ninja` (the `mwcc_sjis` block
   for the unit, `cflags =` and `basedir`/`basefile`) and run `mwcceppc.exe` under
   `build/tools/wibo` + `build/tools/sjiswrap.exe` with `-o` pointed at a scratch path. `tools/probe_cc.sh`
   is the same command but its include list is missing `-i extern/musyx/include` and the
   `inline_max_size(125)` pragma, so it fails on any file that pulls in `CAudioSys.hpp`.
3. **A differ** - `objdump -d --no-show-raw-insn` both objects, split on `<symbol>:`, and **rewrite
   every branch's displacement relative to the function's own start** (and keep the callee name of a
   `bl`). Without that normalisation a one-instruction shift turns the whole function red and you
   cannot see anything. Then count differing lines; it is a good enough proxy to iterate on and the
   real percentages come from the full build at the end.
4. **A variant runner** - hold the file's text, `str.replace` one function body per variant, recompile,
   diff, restore. Never leave the tree in a half-edited state between variants.

With that loop up, each spelling costs about a second and this run measured ~60 of them.
