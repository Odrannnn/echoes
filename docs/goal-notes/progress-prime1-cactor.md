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

---

# progress-prime1-cactor - third run (lane 1, worktree `../wt-mp2-goal-L1`, 2026-09-30)

**Result: `main/MetroidPrime/CActor` 60 -> 72 matched functions** (98 total, fuzzy 90.28% ->
91.11%). Whole-DOL `matched` 10485 -> **10536** (+51), `linked` unchanged at 5051. The change is
**one word** in one shared header. No `flip_test` was run, no `configure.py`/`config/`/`asm` file
was touched, nothing under `tools/` or `build/goal/` was edited except this notes file.

```
tools/goal_check.sh build/goal/item.json
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 10485 -> 10536   linked 5051 -> 5051
  ok    check_symbol_names.py
  ok    All:  31.79% fuzzy, 24.47% matched, 11.84% linked (10536 / 28465 functions)
  ok    target rose: main/MetroidPrime/CActor: 60 -> 72 / 98 functions
  ok    no asm added
goal_check: PASS progress-prime1-cactor
```

## The one that landed: `mutable` on `CActor::mPosition` -> 51 functions

The first run's notes ended with "**The next thing to try is a member-level `mutable`, measured
against the whole report, not just CActor**". That is the whole result. It is not a CActor-local
fix: it is worth 12 functions in `CActor` and **39 more across 20 other units**, because every
`CActor` subclass that forwards `GetOrbitPosition` / `GetAimPosition` / `GetSortingBounds` to the
base class had the same defect.

`include/MetroidPrime/CActor.hpp` line 278, one word plus a comment:

```cpp
  mutable CVector3f mPosition;               // x54
```

`mRenderBounds` does **not** need it - making it `mutable` as well changes nothing (measured;
`GetSortingBounds` is already exact with only `mPosition` mutable). So `mutable` is not acting
per-member; it changes one *class-level* fact.

| unit | functions newly at 100% |
|---|---|
| `main/MetroidPrime/CActor` | 12 |
| `main/MetroidPrime/CPhysicsActor` | 6 |
| `main/MetroidPrime/ScriptObjects/CScriptActor` | 3 |
| `main/MetroidPrime/ScriptObjects/CSScriptDoor` | 3 |
| `main/MetroidPrime/ScriptObjects/CScriptEffect` | 3 |
| `main/MetroidPrime/ScriptObjects/CScriptPlatform` | 3 |
| `main/MetroidPrime/ScriptObjects/CScriptDock` | 3 |
| `main/MetroidPrime/CCollisionActor` | 3 |
| `main/MetroidPrime/CStateManager` | 2 |
| `main/MetroidPrime/ScriptObjects/CScriptTrigger` | 2 |
| `main/MetroidPrime/ScriptObjects/CSScriptDebris` | 2 |
| `main/MetroidPrime/CSteeringBehaviors` | 2 |
| + 8 further units | 1 each |

Verified mechanically over the whole report, not by eye:

```
functions that LOST a 100% match: 0
vanished symbols:                  0
newly at 100%:                    51
```

In `CActor` the four that went to 100% are `GetOrbitPosition` 70.57%, `GetAimPosition` 70.57%,
`GetSortingBounds` 82.00% - the three the first run called a measured wall - and, unasked for,
**`GetYaw` 69.23% -> 100%**, which the second run had also written off.

### Why it works (measured, not assumed)

Retail copies a returned struct one float at a time, in order, through a **single FPR**:

```
lfs f0,0x54(r4) ; stfs f0,0(r3) ; lfs f0,0x58(r4) ; stfs f0,4(r3) ; ...
```

MWCC 2.7 emitted the **two-FPR interleaved** form and hoisted the loads above the stores. The
first run's notes named the mechanism correctly: MWCC hoists the loads only when it can prove
`*sret` does not alias `this`, and `mutable` (or a non-const `this`) removes the proof. The notes
were also right that this is a *class-level* decision, which is why one member is enough.

**`GetYaw` is the same wall seen from the other side.** It returns a `float`, not a struct, so
there is no `*sret` - but its 6-line diff was the *identical* "MWCC hoists the body's first loads
above the callee-saved-register stores" pattern (`lfs f1,56(r3); lfs f0,40(r3); fmuls` interleaved
into the prologue). It went to 100% with the same one-word change. **Any function in this repo
that returns a member by value from a `const` method, or whose first loads MWCC hoists into the
prologue, is a candidate for this.** That is the generalisable finding of this run.

### Regression check, honestly

Four functions' fuzzy percentages fell, all in units that were already `NonMatching`; **no
function lost a 100% match and no symbol vanished**:

```
CActor::PlayCustomSound                              64.98% -> 64.44%
CScriptTeamAiMgr::ChoosePlayer                       88.16% -> 60.93%
CScriptTeamAiMgr::FindBestIndividualAttackTarget     84.62% -> 82.59%
CScriptWater::GetSortingBounds                       32.29% -> 31.71%
```

`CScriptTeamAiMgr` is the one to look at: its two dropped functions are the two that call
`player.GetAimPosition(mgr, 0.f)` (`CScriptTeamAiMgr.cpp:306,358`), i.e. the `mutable` changed the
codegen of the **inlined** copy. Net for that unit is +2 up / -2 down. `ChoosePlayer` is a 624-byte
function with heavy register pressure; the single-FPR sequential copy is right for the standalone
accessor and wrong for that call site. Recorded, not fixed - it is not this item's business.

`docs/HANDOFF.md` is modified in the worktree only because `goal_check.sh` sets
`MP_GATE_DOCS_WRITE=1` and re-derives the state block (4 lines). I did not edit it.

## Measured and NOT changed (spellings and scores, so the next run skips them)

**No `WALL:` line is written on purpose.** The item passed; the driver parks a *failed* item whose
notes gained a `WALL:` line, and parking this one would be wrong. Everything below is measurement.

### 1. The prologue-hoist wall is real and is now *partly* solved - it is not a missing expression

MWCC 2.7 interleaves the body's first loads into the prologue; retail emits the prologue
atomically. Still open for: `GetLocatorTransform(const rstl::string&)` and both
`GetScaledLocatorTransform`s (83.33%, 2 of 12 instructions - `lwz r4,96(r4)` above `stw r0,20(r1)`
instead of after `mr r31,r3`), `SetActorLights` (91.30%, 2 of 23), and by the same shape
`Render`, `IsModelOpaque`, `AddToRenderer`, `CanRenderUnsorted`. **New this run**, on top of the
second run's list: a named local for the model data, a named local for the returned
`CTransform4f`, `mModelData->`, `mModelData.get()->`, a copy of the `CSegId` argument, and a
`!= nullptr` guard. All leave the hoist in place - a guard makes it *worse* (8 differing lines,
because it adds a branch). **`#pragma scheduling off` / `#pragma scheduling reset` around the
function is a no-op for this pass** (measured: still exactly 2 differing lines). `#pragma
dont_inline` and `#pragma optimization_level` are not the same knob and were not tried.

### 2. MWCC 2.7 elides redundant loads that retail keeps - a second, inverse wall

The same shape as the hoist, opposite direction, and it is what blocks the small functions:

- `SetVisorOrbitableFlags` (83.75%): retail `lbz r5,338(r3)` **twice** - once for the
  `clrlwi` mask, once as the `rlwimi` destination. Ours loads once and reuses. 2 instructions.
- `SetValidTarget` (81.90%): same double `lbz`, **plus** retail masks the shift with
  `clrlwi r4,r4,28` in the `&=` arm only. Ours masks in neither arm. 3 instructions.
- `AddToRenderer` (98.97%): retail reloads `lwz r31,192(r30)` immediately before the
  `AddDrawable` call across an intervening out-of-line call. 1 instruction.
- `PreRenderAllViewports` (92.73%): retail re-tests `mModelData` for null *inside* the
  `if (HasModelData())` block - a test MWCC 2.7 proves redundant and deletes - then reloads
  `mModelData->mAnimData` at offset 16. 10 instructions, 6 of them real.
- `GetRenderAlphaBufferAlpha` (98.15%) / `OnScanStateChange` (99.79%): the by-value `TUniqueId`
  argument. Binding it to a named local moves the outgoing slot to `r1+8` where retail has it
  (13 -> 12 differing lines) but MWCC still emits one dead `sth` into a second slot. Only a
  `const TUniqueId&` parameter would remove the copy, and that changes a shared header's mangled
  symbol, so it was not tried blind (the second run's judgement stands).
- `OnScanStateChange`'s three `SendScriptMsgs` arms: retail gives all three the **same** slot
  `r1+8` in a 16-byte frame; ours gives each arm its own (16/12/8) in a 32-byte frame. Tried: an
  if/else chain instead of the switch (25 differing lines, worse), reordering the cases (41,
  worse), an explicit `TUniqueId(...)` temporary per arm (21, worse), a `const` local per case
  (21, worse), and a function-scope `TUniqueId id;` assigned per arm - **does not compile**,
  `TUniqueId` has no default constructor. The previous run's "hoist a `const` local above the
  switch" was 71.38% and remains the best of that family.

### 3. Two inline decisions, both capped by a global flag

- `SetInFluid` (65.97%): **retail has two separate search loops** - a counted loop that only sets
  a `bool` in the `inFluid` branch (`mtctr`/`bdnz`, `li r8,0` / `li r8,1`, a moving pointer) and
  a `rstl::find` in the `else` branch. Ours hoists one shared `rstl::find` above the branch.
  Restructuring to match takes **106 -> 72 differing lines** and is semantically identical, but
  the remaining **22 missing instructions are exactly an inlined copy of
  `RemoveInvalidFluidIds`** (byte-identical loop, verified against the standalone function).
  MWCC 2.7 will not inline it: it is 156 bytes and the whole build carries
  `-pragma "inline_max_size(125)"`. **`#pragma inline_max_size(450)` in the file does not lift
  it** (measured: `SetInFluid` unchanged at 101 instructions), so the cap is not a per-file
  pragma setting here. Raising it would also inline `rstl::sort` (344 bytes), which retail calls.
- `__ct__reserved_vector<pair<ushort,SSound>,4>(int, const pair&)` (0.00%): retail inlines
  `uninitialized_fill_n`; ours outlines it. The outlined body is byte-identical to retail's
  inline one, null guard included, so only the decision differs. Confirms the second run's
  finding; still not reachable from `CActor.cpp`.

### 4. Source shapes that are measurably better but still short of 100%

- `AddLoopedSound` (62.43%): retail stores `first` and `second` through **two separately
  recomputed addresses** (`sthx r25,r28,r0`, then a fresh `mLoopingSoundCount` reload and
  `count*12+4`). Ours binds `TLoopingSound& sound = mLoopingSounds[mLoopingSoundCount];` and gets
  constant offsets. Writing `mLoopingSounds[mLoopingSoundCount].first = sfxId;` and
  `...second = SSound(...)` reproduces retail's store shape: **60 -> 50 differing lines**, and it
  is the same edit the second run made to `StopLoopedSound`. The rest is register allocation
  around the `CSfxManager::SfxStart` call.
- The `CFluidHeightCompare` sort family (`__cl__` 74.25%, `__insertion_sort` 81.53%, `sort`
  83.29%, `__sort3` 71.97%) all differ the same way: retail materialises **both** `cmp`
  arguments as stack copies (r1+8 and r1+12) and masks the key with `clrlwi r31,r0,16`; ours
  passes `&value` and the iterator `t1` directly. `__insertion_sort` also has a **peeled first
  shift** in retail (the `*t2 = *t1` block is the loop's back-edge target, entered by `bne`).
  This needs work in `include/rstl/algorithm.hpp`, which is shared.

## Instrumentation - rebuilt, and it is worth the hour

The second run asked for this. All under `.tmp/` (gitignored), throwaway:

1. **`.tmp/rc.sh`** - single-unit recompile in **0.49 s**, using the real `cflags` out of
   `build.ninja` (`tools/probe_cc.sh` is missing `-i extern/musyx/include` and
   `inline_max_size(125)` and fails on anything pulling in `CAudioSys.hpp`).
2. **`.tmp/fdiff.py`** - per-function instruction differ, `build/G2ME01/src/<unit>.o` vs
   `build/G2ME01/obj/<unit>.o`, with every branch displacement rewritten relative to the function's
   own start. Without that normalisation one moved instruction paints the whole function red.
3. **`.tmp/bytes.py`** - byte-exact check for named functions. Use this, not the percentage:
   `objdiff` reports section-level fuzzy numbers.
4. **`.tmp/div.py`** - first-divergence window; **`.tmp/mdiff.py`** - opcode-only diff, which is
   what isolates *genuinely missing operations* from register noise. On a 146-instruction
   function the plain LCS diff is misleading; the opcode diff is what found the missing null check
   in `PreRenderAllViewports`.
5. **`.tmp/g.py`** - batch variant runner: JSON list of `{name, find, replace, func}`, each
   applied, compiled, diffed, and the file restored. ~1 s per spelling. This run measured ~35.

## For the next run

- The highest-value thing left in this unit is **not** in `CActor.cpp`. It is applying the
  `mutable` insight to *other* headers: any `const` method that returns a member by value, in any
  unit, is a likely 100% for the cost of one word. That is a much better use of a lane than
  re-attempting anything in the wall list above.
- The two walls that are genuinely compiler-version differences and that I could not move with any
  spelling: the prologue hoist (#pragma `scheduling` does not reach it) and the redundant-load
  elision. Both are MWCC 2.7 being *smarter* than retail, which is the opposite of the usual
  decomp problem and is why source-level respelling does not reach them.

---

# progress-prime1-cactor - fourth run (lane 1, worktree `../wt-mp2-goal-L1`, 2026-09-30)

**Result: `main/MetroidPrime/CActor` 72 -> 78 matched functions** (19 of 98 still unmatched, fuzzy
91.63%). Whole-DOL `matched` 11232 -> **11238** (+6, exactly the six functions below, so nothing
anywhere regressed), `linked` unchanged at 5507. Two files changed; no `flip_test` (the item is
`progress`), no `configure.py`/`config/`/`asm`, nothing under `tools/` or `build/goal/` touched.

```
tools/goal_check.sh build/goal/item.json
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 11232 -> 11238   linked 5507 -> 5507
  ok    check_symbol_names.py
  ok    All:  32.37% fuzzy, 24.95% matched, 11.94% linked (11238 / 28465 functions)
  ok    target rose: main/MetroidPrime/CActor: 72 -> 78 / 98 functions
  ok    no asm added
goal_check: PASS progress-prime1-cactor
```

| function | before | after | Prime 1's source |
|---|---|---|---|
| `OnScanStateChange` | 99.79% | **100%** | **used, and it is what found the fix** |
| `SetModelData` | 90.93% | **100%** | exists, but Echoes forked it |
| `PreRenderAllViewports` | 92.73% | **100%** | does not exist in Prime 1 |
| `StopLoopedSounds` | 99.17% | **100%** | does not exist in Prime 1 |
| `StopLoopedSound` | 94.66% | **100%** | does not exist in Prime 1 |
| `GetVisorSoundVolume` | 84.96% | **100%** | does not exist in Prime 1 |

The instrumentation the third run asked for is still in `.tmp/` and still works (`.tmp/rc.sh` is a
0.47 s single-unit recompile with the real `build.ninja` flags, `.tmp/fdiff.py` the instruction
differ with normalised branch displacements, `.tmp/bytes.py` the byte-exact check, `.tmp/g.py` the
variant runner). I re-measured the baseline first: `.tmp/rc.sh .tmp/CActor.o` reproduces
`build/G2ME01/src/MetroidPrime/CActor.o` byte for byte, and 25 of the unit's 98 functions differ from
retail at the start of this run.

## The one the item pointed at: `OnScanStateChange`, and **default arguments**

Prime 1's `CActor::OnScanStateChange` calls `SendScriptMsgs(kSS_ScanStart, mgr, kSM_None)` - three
arguments, and Prime 1's declaration is `SendScriptMsgs(state, mgr, msg)` with no uid at all.
Echoes added `TUniqueId uid`, and its retail code loads `kInvalidUniqueId` and materialises
`kSM_None` (`li r7,-1`) at each call site anyway, so **retail's callers omit both trailing
arguments**, which is only expressible if the declaration has defaults.

That is not cosmetic - it is the whole difference:

```
ours (args spelled out)          retail
stwu  r1,-32(r1)                 stwu  r1,-16(r1)
addi  r6,r1,16 / sth r0,16(r1)   addi  r6,r1,8  / sth r0,8(r1)
addi  r6,r1,12 / sth r0,12(r1)   addi  r6,r1,8  / sth r0,8(r1)
addi  r6,r1,8  / sth r0,8(r1)    addi  r6,r1,8  / sth r0,8(r1)
```

MWCC 2.7 allocates **one 8-byte outgoing-argument block per call site**, and assigns them from the
top of the frame down (16, 12, 8), so three calls in disjoint switch arms cost 32 bytes. With the
trailing arguments defaulted, the substitution happens *after* the frame layout and all three sites
share one block: 16-byte frame, `r1+8` everywhere. The mangled name does not change (defaults are
not part of it) and no existing caller is affected, so `include/MetroidPrime/CEntity.hpp` is the
only header touched. **Generalisable: several calls in disjoint blocks to the same callee, all
passing the same stack argument, sharing one slot in retail, means the callee had defaults.**

Spellings measured first, all worse, so the next run skips them: assign the state and make one call
site (23 differing lines - MWCC merges the three arms into a single block instead of duplicating
them), an if/else-if chain with no `break` (25), `return` instead of `break` in each arm (8, i.e.
identical to the original), a `const TUniqueId uid` hoisted above the switch (35), a per-case
`const TUniqueId uid` (21), an explicit `TUniqueId(...)` temporary per arm (21).

## MWCC materialises the bool of a member call and short-circuits an inline `&&`

This is the single most reusable finding of the run, and it made two functions exact. MWCC 2.7
compiles `a && b` written at an `if` into nested branches, but compiles the **same expression
returned from a member function** into a materialised `bool`:

```
if (GetModelData() && GetModelData()->HasAnimation())   // ours, nested
if (HasAnimation())                                     // retail, materialised
```

where `CActor::HasAnimation()` is `GetModelData() && GetModelData()->HasAnimation()`. Retail's form
is `li r3,0` / `cmplwi` / `beq` / `li r3,1` / `clrlwi. r0,r3,24` / `beq`, and because the bool is
materialised the compiler no longer knows `mModelData` is still non-null, so it also **re-reads
`mModelData` and re-tests it** - a redundant test MWCC 2.7 otherwise proves and deletes. Both
effects show up, and both disappear together.

- `SetModelData` 90.93% -> 100%: `if (GetModelData() && GetModelData()->HasAnimation())` became
  `if (HasAnimation())`. Retail then re-derives `AnimationData()` for the body instead of reusing
  the pointer the inlined test left in a register. `const bool anim = HasAnimation(); if (anim)`
  also reaches 0.
- `PreRenderAllViewports` 92.73% -> 100%: the same substitution on `if (GetModelData()->HasAnimation())`
  got 35 -> 23 differing lines; the remaining 18 were all one register, fixed below.

**Every `if (a() && b())` in this repo is worth retrying as `if (memberFn())`.**

## Three register-allocation levers, all one line each

MWCC's allocator responds to *naming* things, not to the work:

- **Bind a returned object by reference.** `PreRenderAllViewports`: `bounds.AccumulateBounds(new_bounds->GetMinPoint())`
  gives MWCC two independent addresses and it recomputes both. `const CAABox& nb = *new_bounds;` once
  makes it keep the payload's address in a callee-saved register across both calls
  (`addi r29,r1,120` / `mr r4,r29` / `addi r4,r29,12`, exactly retail). `new_bounds.value()` does
  **not** compile on `rstl::optional_object`; `*new_bounds` does, and `.value()` in a different
  position was measured at 23 (no gain).
- **Name the loop index at the call.** `StopLoopedSound`: `RemoveLoopedSoundAt(i)` leaves the
  counter in a volatile register and copies it to r30 inside the body (`mr r30,r6` once per
  iteration). `const uint index = i; RemoveLoopedSoundAt(index);` keeps it in r30 for the whole
  loop. This is the direct fix for the WALL the second run wrote on this function, and it is the
  same lever as `RemoveLoopedSoundAt`'s own `uint` loop variable.
- **Unbind the loop element.** `StopLoopedSounds`: with `TLoopingSound& sound = mLoopingSounds[i];`
  the array base is r29 and the element r30; retail has them the other way round and nothing else
  differs. Deleting the binding and writing `mLoopingSounds[i].second.mHandle` / `.first` / `.second`
  reverses it and the function is byte-exact. Semantics are unchanged (`i` is not touched in the
  body). This is the same lever the second run used in the other direction on `StopLoopedSound`.

## `GetVisorSoundVolume` 84.96% -> 100%: an early return plus a `uint`

Retail keeps the volume in a **callee-saved** register across the `GetActiveVisor` call and masks it
once on the way out (`lbz r31,313(r30)` before the call, `lbz r31,314(r30)` on the echo arm,
`clrlwi r3,r31,24` for both). It branches *over* the whole visor block on the entry test
(`bne +0x54`), ours branched into it. The `uint` matters: with `uchar volume` MWCC re-truncates at
the return (1 differing line instead of 0).

```cpp
if (!mgr.fn_80036F10()) {
  uint volume = mNormalVolume;
  if (mgr.GetPlayer(0)->GetPlayerState()->GetActiveVisor(mgr) == CPlayerState::kPV_Echo) {
    volume = mEchoVolume;
  }
  return volume;
}
return mMaxVol;
```

**The member mapping is unchanged** and the third run's reading of it was right: 312 = `mMaxVol`
for the `fn_80036F10()` arm, 313 = `mNormalVolume` default, 314 = `mEchoVolume`. The win is the
*shape*, not a swap - a `?:` cannot keep a value in r31 across a call. Note this settles a question
the third run left open by reverting the swap on semantic grounds: the swap was never needed.

## Measured this run and deliberately NOT kept

A partial improvement raises no count, so it is not in the diff. Recorded here so the next run
starts from the measured shape rather than from the original one.

- **`AddLoopedSound` 62.43%, 97 insns vs retail's 101 - 60 differing lines. Best spelling reaches
  28.** Two independent edits, both needed: write the stores as
  `mLoopingSounds[mLoopingSoundCount].first = sfxId;` and `.second = SSound(...)` instead of
  binding `TLoopingSound& sound` (retail re-derives the second address from a *reloaded*
  `mLoopingSoundCount`), and invert the pitch arm to `if (pitchDuration > 0.f) {...} else if
  (!mEnablePitchBend) {...}`, which drops the `cror eq,lt,eq; bne` MWCC materialisation in favour
  of retail's single `ble`. Individually: 50 and 45. **What is left is pure register numbering**
  (`this` in r28 vs r29, array base r27 vs r28, an extra `mr r0,r8` for `useAcoustics`, and the
  `AddEmitter` argument shuffles) - the instruction sequences are otherwise identical.
- **`UpdateSfxEmitters` 88.62%, 127 vs 124 - 41 differing lines. Best spelling reaches 25.** Hoist
  the loop bound (`for (uint i = 0, count = mNonLoopingSounds.size(); i < count; ++i)` - retail loads
  `mSize` once before the loop, ours reloads it every iteration; a `const uint count` outside the
  loop is also 31, and `size_t`/`int` make no difference), and load `mMaxVol` into a local *before*
  the `mUseEchoVolume` test so the merge point needs no extra `b` (37 alone). **What is left is
  again pure register numbering** (r24/r25 and r26/r27 exchanged) plus loop 2's base: retail
  indexes `&mLoopingSounds[i]` and offsets `.second`'s members by +4, ours binds
  `const SSound& sound = mLoopingSounds[i].second` and uses +0 - dropping the binding entirely and
  spelling `mLoopingSounds[i].second.m*` is *worse* (42). `const rstl::reserved_vector<SSound, 2>&`
  is 30.
- **`PlayCustomSound` 64.44% - instruction set is identical, only r26-r31 assignment differs.**
  Six spellings measured, all 28-39: locals for `area`/`useRoomAcoustics`, hoisting `area` before the
  emitter, reordering the three member stores, locals for `position`/`direction`, a void return.
- **`GetRenderAlphaBufferAlpha` 98.15% - one dead `sth`.** Ours stores the `TUniqueId` argument
  twice (the outgoing slot *and* a stack home); retail stores once. `const TUniqueId id = GetUniqueId();`
  moves the outgoing slot to `r1+8` where retail has it (13 -> 12) but still leaves the dead store,
  now in the other direction. `m_uid` is private, so it cannot be read directly. This needs
  `CPlayerTargeting::GetScanTargetIndex`'s parameter to be `const TUniqueId&` - a shared-header
  signature change (mangled symbol change) that I did not try blind, as the second run also judged.
- **`SetValidTarget` (81.90%) and `SetVisorOrbitableFlags` (83.75%)** remain the "MWCC is smarter"
  wall the earlier runs recorded: retail re-loads the bitfield container byte twice, once for the
  `clrlwi` mask and once as the `rlwimi` destination, and `SetValidTarget` additionally masks the
  shift with `clrlwi r4,r4,28` in the `&=` arm only. Ours CSEs both loads.
- **`SetActorLights` (91.30%, 2 of 23) and `SSound::SSound` (98.75%, 2 of 8)** are unchanged walls:
  MWCC hoists `release()`'s `lwz r31,4(r4)` above its own `stb r0,0(r4)`, and SSound's ctor
  destroys the incoming parameter register (`lwz r4,0(r4)` vs retail's `lwz r7,0(r4)`). A local for
  the released pointer is 2 as well.
- **`PreRender` (83.42%), `ProcessSoundEvent` (70.46%), `UpdateAnimation` (85.87%), the `CActor`
  constructor (93.94%), `GetDistanceToCamera` (82.08%), `SetInFluid` (65.97%) and the
  `CFluidHeightCompare` sort family (4 functions) were not attempted this run.** One measurement
  worth keeping for `UpdateAnimation`, the item's named function: **retail's camera distance is a
  different algorithm, not a respelling.** It loads `&mPosition` into `r1+48/52/56`, runs
  `lfs f23,0(0)` over the relocated constant `lbl_8041A8B8`, then loops over
  `*(r26 + 5368)` (the state manager's player count), calling
  `CCameraManager::GetCurrentCamera(mgr, true)` per camera and keeping the **minimum squared**
  distance (`fmuls/fmadds` x3, `fcmpo cr0,f0,f23; bge` to skip). Ours calls
  `mgr.GetCameraManager(0)->GetCurrentCamera(mgr, false)` once behind `fn_80036F10()` and takes a
  `Magnitude()`. That is the 28 instructions retail has and we do not.
- **`__ct__reserved_vector<pair<ushort,SSound>,4>` (0.00%)** is unchanged and still not reachable
  from `CActor.cpp` - it is MWCC's `uninitialized_fill_n` inline decision, as runs 2 and 3 found.

## For the next run

- The two generalisable levers here - **default arguments on a callee** and **the member-call bool
  materialisation** - are not specific to this unit. `grep -rn "SendScriptMsgs(" src/` for other
  call sites that spell out the trailing arguments, and grep other units for `if (a() && b())` where
  an existing member function returns the same thing. Both are one-line edits worth many functions.
- The `CFluidHeightCompare` sort family (4 functions, 71-83%) is the biggest single block left in
  this unit, and the third run's measurement stands: retail materialises both comparator arguments
  as stack copies and masks the key, so the difference is in `include/rstl/algorithm.hpp`, not in
  `CActor.cpp`. Blast radius is every unit that sorts; measure it against the whole report.
- `AddLoopedSound` (28 lines) and `UpdateSfxEmitters` (25 lines) are both one register-allocation
  tie-break away; the *structure* is already reproduced by the spellings above, so a lane that has
  time for pure allocator experiments should start from those spellings, not from the file.

---

# progress-prime1-cactor - fifth run (lane 8, worktree `../wt-mp2-goal-L8`, 2026-09-30/10-01)

**Result: FAILED on the judge's only check - `main/MetroidPrime/CActor` stayed at 78 / 98 matched
functions.** `UpdateSfxEmitters` went **88.62% -> 99.35%**, i.e. **4 differing instructions out of
124**, and stopped there. Everything else in the gate is clean:

```
tools/goal_check.sh build/goal/item.json
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 11294 -> 11294   linked 5507 -> 5507
  ok    check_symbol_names.py
  ok    All:  32.48% fuzzy, 25.14% matched, 11.94% linked (11294 / 28465 functions)
  FAIL  target did not rise: main/MetroidPrime/CActor: 78 -> 78 / 98 functions
  ok    no asm added
goal_check: FAIL progress-prime1-cactor - 1 failing check(s)
```

`tools/report_diff.py .tmp/report.base.json build/report.json` over all 2066 units prints
`matched 11294 -> 11294  linked 5507 -> 5507  (+0 functions at 100%, 0 units newly linked)` and
**`no regression`** - no function anywhere lost a 100% match. One file changed,
`src/MetroidPrime/CActor.cpp` (+18 -11), no `configure.py` / `config/` / `asm`, no `flip_test`
(the item is `progress`), nothing under `tools/`, `docs/` or `build/goal/` touched except this file.
The diff is semantically identical to what it replaces (same reads, same calls, same order of
side effects); it is only re-spelled to make MWCC 2.7 emit retail's register assignment.

**The previous four runs' notes were right that this is not a Prime 1 problem** - Prime 1's
`CActor.cpp` has no `UpdateSfxEmitters`, no looping-sound system and no `CFluidHeightCompare`. The
item's `reason` should stop suggesting it for this unit. What *is* reachable is MWCC 2.7's
register allocator, and the recipe below is measured, not recalled.

## `UpdateSfxEmitters`: 37 -> 4 differing lines. Three edits, each independently necessary

Counted with `.tmp/fd.py` (branch displacements normalised relative to each function's own start;
lower is better). Each was measured on top of the previous one:

| # | edit | lines |
|---|---|---|
| 0 | baseline as committed | **37**/124 |
| 1 | **unbind the element** in *both* loops: drop `const SSound& sound = ...` and spell `mNonLoopingSounds[i].mLocator` / `mLoopingSounds[i].second.mLocator` / `.mHandle` / `.mUseEchoVolume` at each use | 25 |
| 2 | **hoist the loop-1 bound**: `for (uint i = 0, count = mNonLoopingSounds.size(); i < count; ++i)` | (in 1) |
| 3 | loop 2 volume: `const uchar maxVol = mMaxVol;` + `const uchar volume = <ternary>` | 8 |
| 4 | loop 2 volume: `uint volume = mMaxVol; if (mUseEchoVolume) { volume = GetVisorSoundVolume(mgr); }` | **4** |

The unbind is the same lever run 4 used on `StopLoopedSounds`: a named reference gives MWCC an
address it can keep in a callee-saved register and reuse, where retail re-derives it from the
element base plus a constant offset. Retail's loop 2 body is `add r26,r27,r29; addi r5,r26,8;
lbz r0,8(r26)` - base + i*12, then **+4 / +8 / +9** for `.second.mHandle / .mLocator /
.mUseEchoVolume`. Spelling `mLoopingSounds[i].second` reproduces exactly that; binding
`const SSound&` gives `+0 / +1 / +5` instead and MWCC re-allocates the whole function.

**Loop 1 and loop 2 need *different* volume spellings, and that is not a typo.** Loop 1 wants

```cpp
const uchar maxVol = mMaxVol;
const uchar volume = mNonLoopingSounds[i].mUseEchoVolume ? GetVisorSoundVolume(mgr) : maxVol;
```

and loop 2 wants

```cpp
uint volume = mMaxVol;
if (mLoopingSounds[i].second.mUseEchoVolume) {
  volume = GetVisorSoundVolume(mgr);
}
```

Measured on top of the 25-line shape: loop 2 as `uchar maxVol` + ternary = 40 lines (MWCC then
re-allocates the whole function - `this` moves from r30 to r28 and everything shifts); loop 2 as
`uint maxVol` + ternary = 7; loop 2 as `uint volume = mMaxVol;` + `if` = **4**; loop 2 as
`uchar volume = mMaxVol;` + `if` = 9; loop 2 as `uint volume` + ternary = 9; loop 2 as an explicit
`if (!echo) { volume = mMaxVol; } else { volume = ...; }` = 9. Loop 1 is the mirror image: `uchar`
+ ternary is right (4 lines for loop 1 alone), `uint` + ternary = 22, `uint` + `if` = 5, `uchar` +
`if` = 19.

### The 4 lines that are left, exactly

```
    addi     r27,r30,140          ; same
    addi     r28,r3,0            ; same
-   li       r25,0                ; retail: loop-2 induction variable in r25
+   li       r24,0                ; ours:  in r24  (r24 held loop 1's bound, dead by here)
    li       r29,0                ; same  (byte offset, strength-reduced += 12)
...
    mr       r4,r31
    bl       GetVisorSoundVolume  ; same
-   mr       r6,r3                ; retail: copy the uint result into the volume slot
+   clrlwi   r6,r3,24             ; ours:  mask it in place (redundant - r6 is masked again below)
    lwz      r0,4(r26)            ; same
...
    clrlwi   r6,r6,24             ; same (the narrowing at the UpdateEmitter argument)
-   addi     r25,r25,1
+   addi     r24,r24,1
    addi     r29,r29,12           ; same
    lbz      r0,340(r30)          ; same
    rlwinm   r0,r0,31,29,31       ; same
-   cmplw    r25,r0
+   cmplw    r24,r0
```

Three of the four are the induction variable's register. Spellings measured on this run that did
**not** move it: `int` index in loop 1 (30) or loop 2 (20), `size_t` (4, no change), `unsigned`
(4), `uint32_t` (4), `register uint i` in loop 1 or loop 2 or both (4), `i = i + 1` instead of
`++i` (4), the loop variable declared outside the loop as a `while` (18), a hoisted
`const uint total = mLoopingSoundCount` (14), a hoisted `const uint nonLoopingCount` in loop 1 (8),
no count hoist in loop 1 at all (14), one shared `uint i` for both loops (does not compile as
written), `uint i = 0; while (i < mLoopingSoundCount) { ... }` (18), and a `TLoopingSound& entry`
binding in loop 2 instead of unbinding (25 at the 18-line stage). The 4th line
(`mr r6,r3` vs `clrlwi r6,r3,24`) is MWCC narrowing a `uint` into the `uchar` argument one
instruction early and then masking the same register again; `static_cast<uchar>(volume)` at the
call, a `const uchar vol` temp, `static_cast<uchar>` on both ternary arms, and `register uint
volume` were all measured and none changes it.

WALL: UpdateSfxEmitters 99.35% - 4 differing lines out of 124 are all register allocation: the
loop-2 induction variable (r25 vs r24, three instructions) and one redundant `clrlwi` on the
volume; ~40 spellings of the index type, the bound, the loop form and the volume expression were
measured this run and none reaches 0.

## A real port discrepancy found, measured, not fixed: `CStateManager::GetObjectById`

`CFluidHeightCompare::operator()` is **not byte-reachable in this tree today**, and the reason is
not the comparator. `objdump -r` on dtk's retail object shows what the retail comparator calls:

```
4720: R_PPC_REL24  GetObjectById__13CStateManagerCF9TUniqueId   <- retail (const overload)
48e8: R_PPC_REL24  ObjectById__13CStateManagerF9TUniqueId       <- ours (non-const overload)
```

Retail calls the **const** overload and hands its result straight to
`TCastToPtr<12CScriptWater>__FP7CEntity`, which takes a **non-const** `CEntity*`. Our header
declares `const CEntity* CStateManager::GetObjectById(TUniqueId uid) const;`
(`include/MetroidPrime/CStateManager.hpp:160`), so our comparator is forced onto the non-const
overload and the `bl` displacement can never match. Retail's const overload must return
`CEntity*` (the same inference `src/MetroidPrime/Cameras/CCameraManager.cpp:315-320` already
records in a comment for `SetPathCamera`). The mangled symbol is unchanged by the return type, and
every existing caller keeps compiling (`TCastToConstPtr` and `static_cast<const T*>` both accept a
`CEntity*`). I changed the header, the definition (`CStateManager.cpp:561`, needs a `const_cast`)
and the comparator, measured `__cl__`, and **reverted all three**: with the callee fixed and the
best body shape the function is still 20 of 60 lines away (see below), so the change buys no
matched function and touches a header fifteen call sites depend on. **A `NEW:` item is not filed
for it** - on its own it raises no count.

### `__cl__` measured with the callee fixed: 41 -> 20, and the remaining 20 are also registers

The finding worth keeping is that the **evaluation order** of the comparison is source-controlled
here. Retail computes water A's plane first and keeps `this` in r29, waterA in r31 and waterB in
r30 - three callee-saved registers, a 112-byte frame. Ours uses two and a 96-byte frame.

| spelling | lines |
|---|---|
| as committed (one `<` over two full chains) | 41 |
| `const float zA = ...; const float zB = ...; return zA < zB;` | **31** |
| `const CVector3f zero;` + the two float locals | **20** |
| `const CVector3f& zero = CVector3f::Zero();` + the two float locals | 31 |
| `CVector3f()` instead of `CVector3f::Zero()` inline | 40 |
| `const CVector3f zero = CVector3f::Zero();` | 29 |
| `const CVector3f pointA = ...; pointB = ...; return pointA.GetZ() < pointB.GetZ();` | 31 |
| `const CScriptWater* const waterA/waterB` (either spelling) | no change |
| null test as two `if`s | 23 |
| null test as `!waterA \|\| !waterB` | no change |
| null test folded into the return: `waterA && waterB && zA < zB` | 24 |
| a free `static float FluidSurfaceZ(const CScriptWater*)` helper | no change |
| named `CEntity*` locals between the lookup and the cast | 29 |

**The 20-line shape is not keepable and must not be copied**: `const CVector3f zero;` is a
*default-constructed, uninitialised* vector passed by const reference to `GetClosestPoint`, which
reads it. It is the only spelling that makes MWCC evaluate A before B, and it is undefined
behaviour, so it is recorded here purely as a measurement. Retail passes the relocated
`sZeroVector`, so the source uses `CVector3f::Zero()`, and with `Zero()` MWCC always evaluates the
right operand of `<` first. The remaining ~14 lines after that are r29 vs r31 for `this` and the
merged null-test shape (`beq/beq` in retail, `beq/bne/li/b` in ours) - again an allocator choice,
not a missing expression.

## `AddLoopedSound`: run 4's two edits re-measured, 41 -> 25, and it is a hard tie-break

Run 4's recipe reproduces exactly, and it is the closest this function has ever been:

```cpp
  if (handle) {
    mLoopingSounds[mLoopingSoundCount].first = sfxId;
    mLoopingSounds[mLoopingSoundCount].second = SSound(handle, locator, useEchoVolume);
    if (mEnablePitchBend) {
      CSfxManager::PitchBend(handle, mPitchBend);
    }
    if (pitchDuration > 0.f) {
      CSfxManager::AddPitchBend(CSfxPitchBend(handle, pitchStart, pitchEnd, pitchDuration));
    } else if (!mEnablePitchBend) {
      CSfxManager::PitchBend(handle, pitchStart);
    }
  }
```

After that the two objects are **instruction-for-instruction identical** and differ only in which
callee-saved register each value got: retail `this`=r29, array base=r28, `nonEmitter`=r27,
`area`=r26, `sfxId`=r25; ours `sfxId`=r29, `this`=r28, array base=r27, `nonEmitter`=r26,
`area`=r25. A dozen spellings measured on top of it all leave it at exactly 25: a `const uint
slot = mLoopingSoundCount` local (35), a `TLoopingSound* const array` local (does not compile),
`static_cast<ushort>(sfxId)` (25), `const CSfxHandle& h = handle` (25), `const int areaId`
(25), `const ushort pitch = pitchStart` (25), `mLoopingSoundCount = static_cast<uint>(...) + 1`
(25), a `return;` before the closing brace (25), the two stores swapped (25), and the pitch arm
flattened either way round (39 each). Run 4's note that "what is left is pure register numbering"
is confirmed; there is no lever in `CActor.cpp`.

## `PlayLoopedSound`: the `musyxFlags` expression is already the best spelling

Retail computes the flags as `li r4,1; beq +0x80; ori r4,r4,8`; ours as `li r4,1; beq; li r4,9`.
Every spelling that could produce the `ori` is worse: `(flags & 8) | 1` (37), `1 | (flags & 8)`
(37), `(flags & 8) + 1` (37), `1 | ((flags & 8) << 0)` (37), all against a baseline of 31.
The rest of the function is a **4-byte frame-layout shift** (every local and outgoing-argument slot
is 4 lower than retail's) caused by one instruction I could not explain: retail materialises an
**8-byte** temporary for `RemoveEmitter(mLoopingSounds[0].second.mHandle)` - `lwz r0,144(r22)`
then `stw r0,20(r1)` **and** `stw r0,16(r1)`, the same 4-byte value into both halves of an 8-byte
slot at `r1+16`. Our `CSfxHandle` is 4 bytes and we pass `&` of a 4-byte stack temp. Whatever
retail's parameter type is, it is not expressible from this tree's headers.

## Dead ends measured this run, so nobody repeats them

- **`SetValidTarget` (81.90%) and `SetVisorOrbitableFlags` (83.75%)** - both differ from retail
  only by a redundant `lbz` that MWCC CSEs (retail loads the 4-bit container byte twice, once for
  the `clrlwi` mask and once as the `rlwimi` destination). The `&=` arm of `SetValidTarget` also
  masks the shift with `clrlwi r4,r4,28` in retail only. Spellings measured: `&= ~((1 << i) & 0xF)`
  (13), `= x & ~((1 << i) & 0xF)` (13), `^= x & (1 << i) & 0xF` (13), `static_cast<uint>` on the
  whole expression (11), `1u << i` (11), `~static_cast<uint>(flags) & 0xF` (11),
  `~(static_cast<uint>(flags) & 0xF)` (10). Nothing beats the committed spelling. The `~` is
  already applied to the **masked** value in retail (`slw r4,r0,r4; clrlwi r4,r4,28; andc`), so
  the source is masking - MWCC just deletes the redundant half.
- **`GetRenderAlphaBufferAlpha` (98.15%) is provably blocked, not merely hard.** Retail's callee
  is `_ZNK16CPlayerTargeting18GetScanTargetIndexERK13CStateManager9TUniqueId` - I read the mangled
  name out of `src/MetroidPrime/PortReachStubs.cpp:1436` - so retail also passes `TUniqueId` **by
  value**. Changing our parameter to `const TUniqueId&` (runs 2-4's suggestion) would change the
  mangled symbol and lose a matched function elsewhere. Closed.
- **`SSound::SSound` (98.75%)** is 2 of 8 instructions and nothing else: `lwz r4,0(r4)` vs
  `lwz r7,0(r4)`, then `stw` of the same register to `0(r3)`. Both objects are 32 bytes with an
  identical instruction set; MWCC coalesced the destination with the incoming parameter register.
- **`__ct__reserved_vector<pair<ushort,SSound>,4>` (0.00%)** is unchanged: retail inlines the fill
  loop (18 instructions, `mtctr`/`bdnz`, null guard included), we call an out-of-line
  `uninitialized_fill_n`. Runs 2 and 3 reached the same conclusion.

## The instrumentation, rebuilt (runs 2-4 all asked for it; this is the version that worked)

All under `.tmp/`, throwaway, gitignored. **Building it cost about 25 minutes and then paid for
itself** - about 70 spellings were measured this run at ~1.2 s each.

1. `.tmp/rc.sh <out.o>` - single-unit recompile in **0.63 s**, using the real `cflags` out of
   `build.ninja` (`tools/probe_cc.sh` is still missing `-i extern/musyx/include` and
   `inline_max_size(125)` and fails on anything pulling in `CAudioSys.hpp`). Verified: the first
   output was `cmp`-identical to `build/G2ME01/src/MetroidPrime/CActor.o`.
2. `.tmp/fd.py [our.o] [retail.o] [name-substring] [-v]` - per-function instruction differ against
   dtk's `build/G2ME01/obj/MetroidPrime/CActor.o`, with every branch displacement rewritten
   relative to its own function's start. Prints one `differing/total` line per function and a
   total; `-v` gives a unified diff. **Use the count, not objdiff's percentage** - it moved a
   function from 37 to 4 while objdiff only went 88.62% -> 99.35%, and it is what made the
   three-edit recipe above findable.
3. `.tmp/bytes.py` - byte-exact per function. Necessary: `objdiff` reports section-level fuzzy
   numbers, and `fd.py` can report a false difference when a `bl` target moves inside the same
   function (its `<sym+0xNN>` annotation is objdump's, not the branch's).
4. `.tmp/mdiff.py` - **opcode-multiset diff per function**. This is the one that should have been
   built first: it separates "missing operations" (a real semantic gap) from "reallocation" in one
   command. On this tree it says plainly that `SetInFluid` (34 missing instructions - the
   un-inlined `RemoveInvalidFluidIds`, confirmed), `UpdateAnimation` (28 - the camera loop), the
   `CActor` constructor and `PreRender` have **missing code**, while `AddLoopedSound`,
   `PlayCustomSound`, `UpdateSfxEmitters` and `GetRenderAlphaBufferAlpha` have an **identical
   opcode multiset** and are therefore pure allocator work.
5. `.tmp/g.py <variants.json> [name-substring]` - batch variant runner. Applies each
   `{name, find, replace}` to `CActor.cpp`, recompiles, diffs, and restores; ~1.2 s per spelling.
   It asserts the `find` matched exactly once and prints `SKIP` if not, which is how a stale
   variant list is caught. `.tmp/apply.sh <variants.json> <n>` applies one and leaves it, for
   inspecting a diff interactively.

## For the next run

- **`UpdateSfxEmitters` is 4 instructions from done and the exact 3-edit recipe is above.** Start
  from the committed source, not from the notes. The only untried lever I can think of is making
  loop 2's induction variable *not* be a fresh `li` - e.g. a loop form where the counter is an
  induction variable derived from the byte offset.
- **The generalisable lesson, and it is the fourth run in a row to hit it: MWCC 2.7's register
  allocator responds to *which names exist*, not to the work.** Binding an element reference,
  hoisting a bound, and replacing a `?:` with an `if`-assignment each moved `UpdateSfxEmitters`
  by 12, 12 and 4 instructions. Every function in the wall list above is instruction-complete;
  the remaining diffs are naming.
- **`mdiff.py` should be the first thing the next run builds.** It costs two minutes and it is
  the only tool here that says whether a function is missing code or just misallocated.
- The item's `reason` points at Prime 1's `CActor.cpp` for a fifth time and it has never helped:
  Prime 1 has none of `UpdateSfxEmitters`, the looping-sound system, `CFluidHeightCompare`,
  `SetInFluid`, `PreRender` or `ProcessSoundEvent`. **This unit is now an MWCC-codegen item, not a
  Prime 1 item.**

# progress-prime1-cactor - sixth run (lane 9, worktree `../wt-mp2-goal-L9`, 2026-10-02)

**Result: `main/MetroidPrime/CActor` 78 -> 79 matched.** `goal_check.sh` PASS (gate, 86 RELs, no asm).
Only `src/MetroidPrime/CActor.cpp` touched (`GetDistanceToCamera`).

## `GetDistanceToCamera` 82.08% -> 100% (spellings measured, in order)
| spelling | score |
|---|---|
| committed (`CVector3f - ` temp `.MagSquared()` inline, `int i`, `FLT_MAX`) | 82.08 |
| manual `dx*dx+dy*dy+dz*dz` floats, `uint i` | 92.24 |
| + literal `3.402823466e+38f` instead of `FLT_MAX` (macro loads via `__float_max`, retail reads sdata2 in place) | 90.69 (reloc now matches; other diffs remain) |
| `int i` + `mgr.CameraManager(i)` (non-const accessor) | 98.47 (`uint i` lost retail's `mgr`-base pointer induction var + `lwz 5404(r31)`) |
| + `(uint)i < (uint)mgr.GetNumPlayers()` (retail `cmplw`, with `int i` kept) | 99.49 |
| sum-order permutations of dx/dy/dz | 98-99.29, none better |
| **`const CVector3f d = camera->GetTranslation() - position; d.MagSquared()`** | **100%** |
The last one: a *named* const CVector3f local makes MWCC scalarise the temp (no stack stores) and load
y first like retail; the unnamed temp in one expression kept the stores.
The three levers (int counter with unsigned compare, literal FLT_MAX, named vector local) are each necessary.

## `SetActorLights` 91.30% - re-measured, still 2 of 23
Retail: `stb 0,0(r4)` then `lwz r31,4(r4)`; ours hoists the `lwz` above the `stb`. Measured unchanged
(all give the same hoisted order): `lights.mHas=false; p=lights.mItem`, `release(); get()`, comma form,
`p=lights.mItem; mHas=false` (worse: 3-reg frame), temp `single_ptr` (worse), auto_ptr copy (worse).
Not WALL-tagged by me: only ~8 spellings.
