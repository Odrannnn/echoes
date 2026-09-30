# progress-prime1-cgunweapon — `MetroidPrime/Weapons/CGunWeapon`

Worktree: `../wt-mp2-goal-L3` (lane 3), 2026-09-30. One file changed:
`src/MetroidPrime/Weapons/CGunWeapon.cpp` (+32 / -14). No header change, no `asm`, no
`configure.py` / `config/` / `tools/` / `build/goal/` edit. The unit stays `NonMatching`;
`flip_test.sh` was not run (correct for a `progress` item).

## Result (measured, `./tools/decomp_build.sh` + `build/report.json`)

| | before | after |
|---|---|---|
| `main/MetroidPrime/Weapons/CGunWeapon` `matched_functions` | **13 / 59** | **20 / 59** |
| unit fuzzy | 27.14 % | **29.98 %** |
| unit matched code | 824 / 16228 B (5.08 %) | **1580 / 16228 B (9.74 %)** |
| `All:` (report.json `measures`) | 29.89 % fuzzy, 9665 / 28465 functions | **29.90 % fuzzy, 9672 / 28465** |

Seven functions moved from sub-100 % to an exact match, and **no function anywhere got
worse** (every other function's percentage is unchanged or higher — see the per-function
table). Gate, all green:

```
sha1sum build/G2ME01/main.dol   6ef9b491d0cc08bc81a124fdedb8bfaec34d0010   (pinned value)
./tools/probe_sources.sh        probe: 744 files, 0 failed, 0 errors;
                                link: LINKED (250 undefined, 0 duplicates)
python3 tools/check_symbol_names.py   checked 502 units; 0 declared names are missing
./tools/decomp_build.sh         All:  29.90% fuzzy, 21.70% matched, 11.74% linked (9672 / 28465 functions)
```

`python3 tools/check_docs_claims.py` now reports the `docs/HANDOFF.md` state block as stale
(`9672 / 28465`, `8261 / 16726`) — that is the count this item moved, and the driver rewrites
those derived numbers from the tree, so `docs/` was deliberately left alone.

## The seven new matches, and what each one needed

`tools/fast_try.sh MetroidPrime/Weapons/CGunWeapon` is the loop (0.5 s). Two throwaway
scripts made it cheap: a side-by-side disassembler of one function (ours from
`build/G2ME01/src/...o`, retail's from `orig/G2ME01/sys/main.dol` at the address
`build/report.json` gives, **big-endian** — `objdump -D -b binary -m powerpc -EB`, the
default is little-endian and decodes garbage), and a resolver for retail `bl` targets out of
`config/G2ME01/symbols.txt`.

| function | before | after | Prime 1's source | change made |
|---|---|---|---|---|
| `GetWeaponIndex` | 84.45 % | **100 %** | n/a (Prime 1 has the same switch) | added `case kWT_Power: return kBI_Power;`. Retail's tree is a binary search over the **four** cases {0,1,2,3}; ours only had {1,2,3} + default, so the shape differed. Prime 1's file needed no adaptation here. |
| `IsFidgetLoaded` | 40.36 % | **100 %** | Prime 1 has no null guard (it dereferences unconditionally) | Echoes' retail checks `mGunController` first. Rewrote the `&&` form as `if (null()) return false; return IsFidgetLoaded();` so the compiler stops materialising a bool. Prime 1's spelling does **not** compile to Echoes' bytes. |
| `IsAnimsLoaded` | 71.19 % | **100 %** | **Prime 1's body, unchanged** — `for (rstl::vector<CToken>::const_iterator it = mAnims.begin(); it != mAnims.end(); ++it)`. | We had an index loop; retail walks pointers and precomputes the end pointer. Prime 1's source transferred verbatim. |
| `GetMuzzleFx` | 49.00 % | **100 %** | Prime 1 has no such function | retail does `if (!mMuzzleGenerators.empty() && mMuzzleGenerators[index].get()) return ...; return nullptr;` — two tests, not one. The ternary spelling only emits the `empty()` test. |
| `GetAnimDuration` | 90.87 % | **100 %** | Prime 1 has no such function | retail has **two** separate `return 0.f` sites: one for `!mLoaded`, one for the out-of-range type. Splitting our single `if (!mLoaded \|\| ...)` restores the second `lfs` + branch. |
| `FreeResPools` | 85.63 % | **100 %** | **Prime 1's body, unchanged** for the loop bound | Prime 1 writes `for (int i = 0; i < mMuzzleEffects.capacity(); ++i)`. We had `.size()`; retail's loop test is `cmpwi r28,2 / blt`, i.e. the compile-time capacity. Same one-word fix in `Load`. |
| `Load` | 85.54 % | **100 %** | as above | `.size()` → `.capacity()` in both loops. |

Three more functions went from stub/low to **one instruction away**, all blocked by the same
thing (below), and one improved without a match:

* `EnableFrozenEffect` 56.69 % → **99.98 %**. Prime 1's `switch (type)` over
  `kFFT_Thawed` / `kFFT_Frozen` / `default` (with `mFrozenEffects[1]` / `[0]`) is exactly
  Echoes' shape: 109 instructions, byte-identical except one immediate.
* `IsChargeAnimOver` 55.77 % → **99.97 %**. Prime 1's body (`if (mEnableCharge) { if
  (IsAnimTimeRemaining(...)) return false; } return true;`) is Echoes' body; our `||` form
  was not. Byte-identical except one immediate.
* `LoadMuzzleFx` was already 99.99 % and still is — it is blocked by the same immediate.
* `FillTokenVector` 43.77 % → **70.72 %**. Prime 1's body transfers: the iterator loop and
  `token.GetReferenceType() == 'TXTR'` (we had `tags[i].GetType()`, which is the same value
  but a different load). It still does not match for a reason that is **not** Prime 1's
  fault — see "FillTokenVector" below.

## The wall: this unit's string pool, worth three functions

`rs_new` is `new ("\?\?(\?\?)", nullptr)`, and `rstl::string_l("Whole Body")` in
`IsChargeAnimOver`, so those three functions carry a *baked-in immediate*: the offset of the
literal from `@stringBase0`, which the compiler computes from **its own** object's layout.

* ours: `"\?\?(\?\?)"` at **+50**, `"Whole Body"` at **+57**, `"VariaArm"` at **+68**;
* retail: `"\?\?(\?\?)"` at **+12**, `"Whole Body"` at **+19**.

Both are self-consistent (ours `50→57` is 7 bytes = `"\?\?(\?\?)"` + NUL; `57→68` is 11 =
`"Whole Body"` + NUL), so the fix is a pure **-38 byte shift**. 38 is exactly the four
`skBeamXferNames` literals (`"PowerXfer"` 10 + `"IceXfer"` 8 + `"WaveXfer"` 9 +
`"PlasmaXfer"` 11 = 38), which sit in our pool between the locators and the code literals.

Retail does **not** have them there. In the merged DOL the object pool is
`@stringBase0 = 0x803AABD8` = `"LBEAM"`, and the bytes run
`[LBEAM][elbow][??(??)][Whole Body]...`; the four Xfer strings are at `0x803B0017`, 0x5440
bytes later, i.e. in a **different** run. (Retail's own object,
`build/G2ME01/obj/MetroidPrime/Weapons/CGunWeapon.o`, has an **empty** `.rodata` — dtk
externalised every literal — so this offset comes out of the original compiled object and is
visible only in the `addi` immediates.)

**Spellings tried, none moves the pool** (each measured, offsets re-read from the object
afterwards):

1. move `skBeamXferNames` after the two locator definitions → run becomes
   `[LBEAM][elbow][Xfer x4][??(??)][Whole Body][VariaArm]`, offsets still 50/57/68. No gain,
   so it was **reverted**; the diff leaves the array where it was.
2. make it a class static (`static const char* skBeamXferNames[4]` in the header, defined at
   the bottom of the .cpp) → identical run, identical offsets. Reverted; **the header is
   untouched in the final diff**.
3. file-scope tentative definition at the top + real definition at the bottom → build fails,
   MW rejects the redefinition.
4. give `skElbowLocator` a code reference (temporary `ProbeElbow()` returning it) to test the
   theory "strings referenced from code join the literal run, data-only strings get their own
   run" → no split, offsets unchanged. Theory disproved; probe removed.

So with this compiler and these flags **every** string in a translation unit lands in one
run, and a `const char* const[]` array's literals are always in it. Either the four Xfer
strings have to stop being literals in this file (which would change the `.data` the
constructor indexes) or the pool rule has to change. This is codegen, not a spelling, so it
is a note and not a `NEW:` item.

## Other functions measured, with what still blocks them

`Reset` (96.00 %, 120 B) and `ActivateCharge` (95.07 %, 460 B) are register-allocation
differences only. `Reset` is instructive: ours keeps the bitfield byte in `r4` across the
test and reuses it for the store, retail reloads into `r0` and reuses `r5=0` for the
`rlwimi` — 15 differing instructions out of 30 with no semantic difference, and three
spellings of the same `if (mEnableCharge) { mEnableCharge = false; } else if (...)` all
land in the same place. `ActivateCharge` additionally carries the same string immediate.

Worth a later run, not attempted here:

* `LockTokens` (52 B) / `UnlockTokens` (56 B) are **unmapped** (objdiff gives them no
  percentage at all) because ours inlines `AsyncLoadSuitArm` (26 instructions against
  retail's 13). Retail `LockTokens` is `call AsyncLoadSuitArm; call lock_tokens(mDeps)`.
  Both are one inlining decision from being small matches.
* `FillTokenVector` (70.72 %, 244 B): retail inlines `rstl::vector::push_back`
  (`lwz size / lwz data / slwi / addi / stw size+1 / beq` then the `CToken` copy ctor) while
  ours emits an out-of-line `push_back` call, 68 instructions against retail's 61. That is a
  header question (`include/rstl/vector.hpp`), not this unit, and it would move every unit
  that pushes back — deliberately not touched.
* Echoes-only bodies with no Prime 1 ancestor, each still a stub here and each needing real
  reverse engineering: `GetBounds()` (62.32 %, 368 B — ours walks the PAS bbox list; retail
  also uses a `CAABox` cache flag at `this+0x78` and a float constant from `.rodata`),
  `Touch` (2.94 %, 136 B — retail calls something on `mgr` first, then tests
  `this+0x78`, a bit in `this+0x271`, `this+0x118/0x11C` and `this+0x250`),
  `GetDamageInfo` (9.20 %, 344 B), `AsyncLoadSuitArm` (71.19 %, 108 B — retail's
  no-argument version calls with `this+0xCC` and a 76-byte stack temporary),
  `BuildAnimationIdList` (17.01 %), `CVelocityInfo::Clear` (5.43 %), `LoadAnimations`
  (1.35 %), `AllocResPools` (0.81 %), `BuildDependencyList` (1.33 %), `LoadGunModels`
  (1.27 %), `LoadProjectileData` (0.89 %), `Unload` (0.90 %), `LoadFxIdle` (0.64 %),
  `Update` (0.65 %), `UpdateGunFx` (1.16 %), `Draw` (0.80 %), `DrawHologram` (1.39 %),
  `DrawClipCube` (0.24 %, 1660 B), `LoadSuitArm` (2.33 %), `PointGenerator` (2.70 %),
  `InitializeResources` (1.22 %), `ReleaseResources` (1.22 %), `Fire` (0.22 %, 1804 B),
  `__dt__` (68.98 %, 524 B), `__ct__` (94.29 %, 616 B), `DrawMuzzleFx` (75.39 %),
  `UpdateMuzzleFx` (70.54 %), and four unnamed retail functions
  (`fn_801D9334`, `fn_801D9E20`, `fn_801DBDD8`, `fn_801DBE6C`).

## Codegen rules this run paid for

* `rstl::reserved_vector<_, 2>` loops in this class must be written
  `for (int i = 0; i < v.capacity(); ++i)`. `.size()` is wrong for the code and right for the
  meaning; retail's test is `cmpwi rN,2 / blt`.
* Two `return <same constant>;` statements beat one `if (a || b) return c;` whenever retail
  has two loads of the constant (`GetAnimDuration`).
* An `if (p.null()) return false; return p->f();` is not the same code as
  `return !p.null() && p->f();` — the `&&` forces a bool materialisation into a callee-saved
  register (`IsFidgetLoaded`).
* Retail's ternary-in-`switch` shape is a faithful transcription of the source's `switch`,
  but the source order of the `case` labels does not matter — MW sorts the case values. What
  does matter is that every case value that falls through to `default` still needs its own
  `case` label if the compiler emitted a separate return for it (`GetWeaponIndex`).
* Iterating with `begin()/end()` rather than `operator[]` is what retail does wherever the
  loop body only needs the element (`IsAnimsLoaded`, `FillTokenVector`).
* `objdiff` treats a relocated SDA reference (`lfs f1,disp(r2)`) as equal to ours, so a
  differing `lfs` in a side-by-side diff is **not** necessarily a difference objdiff sees —
  check the percentage before chasing it.

---

# Run 2 — `../wt-mp2-goal-L4` (lane 4), 2026-09-30. PASS, 20 -> 25.

Files changed: `src/MetroidPrime/Weapons/CGunWeapon.cpp` (+13/-8),
`include/MetroidPrime/Weapons/WeaponCommon.hpp` (+4), `files.cmake` (+6), and one new
port-only source `src/MetroidPrime/Weapons/NWeaponTypesTokens.cpp`. No `configure.py`,
no `config/`, no `tools/`, no `build/goal/`, no `asm`. The unit stays `NonMatching`;
`flip_test.sh` not run (correct for a `progress` item). `docs/HANDOFF.md` and
`docs/RUNNING_THE_DECOMP.md` are dirty in the worktree but **not hand-edited** — the
build tooling rewrote their derived counts (probe file count 750 -> 751, from the new
source file); the judge discards those.

## Result (measured, `./tools/goal_check.sh build/goal/item.json`)

| | before | after |
|---|---|---|
| `main/MetroidPrime/Weapons/CGunWeapon` `matched_functions` | **20 / 59** | **25 / 59** |
| unit fuzzy | 30.60 % | **31.27 %** |
| unit matched code | 1580 / 16228 B (9.74 %) | **2576 / 16228 B (15.87 %)** |
| `All:` (report.json `measures`) | 10257 / 28465 | **10262 / 28465**, 31.23 % fuzzy |
| port link undefined | 250 | **250 (unchanged)** |

`goal_check: PASS`. Zero of the 28465 functions anywhere got worse (measured by
diffing every per-function percentage in `report.base.json` against `report.json`;
only the 5 below moved up). Gates, all green:

```
sha1sum build/G2ME01/main.dol   6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
./tools/probe_sources.sh        probe: 751 files, 0 failed, 0 errors;
                                link: LINKED (250 undefined, 0 duplicates)
python3 tools/check_symbol_names.py   checked 505 units; 0 declared names are missing
python3 tools/check_files_cmake.py   every configured DOL object is either listed or excluded
python3 tools/check_decl_order.py --unit MetroidPrime/Weapons/CGunWeapon
                                ok: 1 unit(s) checked, none emits its functions out of retail order
./tools/decomp_build.sh         All: 31.23% fuzzy, 23.54% matched, 11.82% linked (10262 / 28465)
```

## The five new exact matches

| function | before | after | what it took |
|---|---|---|---|
| `LoadMuzzleFx` | 99.99 % | **100 %** | the string-pool fix below; nothing else |
| `IsChargeAnimOver` | 99.97 % | **100 %** | the string-pool fix below; nothing else |
| `EnableFrozenEffect` | 99.98 % | **100 %** | the string-pool fix below; nothing else |
| `LockTokens` | 0 % (unmapped) | **100 %** | call the out-of-line `NWeaponTypes::lock_tokens`, and **define it** |
| `UnlockTokens` | 0 % (unmapped) | **100 %** | `mArmModel.Unlock()` + a call to `fn_8018A6EC`, and **define it** |

## Run 1's wall is broken: a `char[]` array is not a string-pool literal

Run 1 concluded "with this compiler and these flags **every** string in a translation
unit lands in one run, and a `const char* const[]` array's literals are always in it",
and recorded four failed spellings. All four spellings kept the four `skBeamXferNames`
**as string literals**. The premise was too strong: they land in one run, but only
because a string literal is a pool literal. Give them a type that is not a literal and
they leave the pool:

```c++
static const char skPowerXfer[] = "PowerXfer";    // array, not a literal
static const char skIceXfer[] = "IceXfer";
static const char skWaveXfer[] = "WaveXfer";
static const char skPlasmaXfer[] = "PlasmaXfer";

static const char* const skBeamXferNames[] = {
    skPowerXfer, skIceXfer, skWaveXfer, skPlasmaXfer,
};
```

Measured on the object afterwards: `.rodata` loses the 38 bytes of `PowerXfer`/`IceXfer`/
`WaveXfer`/`PlasmaXfer` and the pool starts at `LBEAM`; the four array addresses move to
their own `.rodata` slots with plain `R_PPC_ADDR32` relocations against
`skPowerXfer` &c, and `.sdata`'s relocations become plain `@stringBase0` and
`@stringBase0+0x6` (was `+0x26` / `+0x2c`). That is the `-38` shift run 1 measured, and
it is what turns three 99.9x% functions into exact matches. **No new undefined symbol,
no header change, no other unit touched.**

This is the same fact as run 1's "iterating with `begin()/end()`": MW inlines what the
type makes inlineable, and a `const char[]` array is data it can address directly. It is
worth trying on any other unit whose `addi rX, @stringBase0, N` immediates are short.

## `LockTokens` / `UnlockTokens`: call the helper, and define it

Run 1 left these at 0 % "**unmapped**" and called them "one inlining decision from being
small matches". They were not an inlining problem — they were **wrong bodies**. Ours
looped `mDeps` inline (25-26 instructions); retail is 13 instructions: a call, a
prologue and an epilogue. Two things had to be true at once.

**(a) The bodies.** Prime 1's shape transfers, with Echoes' members:

```c++
void CGunWeapon::LockTokens() {
  AsyncLoadSuitArm();
  NWeaponTypes::lock_tokens(mDeps);
}

void CGunWeapon::UnlockTokens() {
  mArmModel.Unlock();
  NWeaponTypes::fn_8018A6EC(&mDeps);
}
```

`mArmModel`, not `mXferEffect`: retail's is `addi r3,r31,384 / bl Unlock__6CTokenFv`
with **no** preceding store, i.e. a plain `CToken::Unlock()`. `TCachedToken::Unlock()`
inlines `mItem = nullptr` first, which emits `li r0,0 / stw r0,428(r3)` and calls from
`this+420` — that is what `mXferEffect.Unlock()` produced, and it was 3 instructions
longer than retail. The member at `+384` is the `TToken<CModel>`. **When retail calls
`Unlock` with no preceding store, the member is a `TToken`, not a `TCachedToken`** —
that is the discriminator, and it is cheaper than reading the header.

**(b) The definitions.** Calling `NWeaponTypes::lock_tokens` is a *new external
reference*, and the port link's regression gate counts undefined symbols. Both helpers
are real, and **nothing in the tree defined either**: `lock_tokens` was declared in
`WeaponCommon.hpp` and called from `src/MetroidPrime/Player/CGrappleArm.cpp` (lines 117
and 251), but `CGrappleArm.cpp` is **not in `files.cmake`**, so the port never compiled
it and the symbol never showed up as undefined. Adding the call from `CGunWeapon.cpp`,
which *is* in `files.cmake`, took the port from 250 to **252**. Run 1's "both are one
inlining decision from being small matches" would have hit the same wall.

They live at `0x8018A6EC` (`fn_8018A6EC`) and `0x8018A748` (`lock_tokens`) — in the
**unclaimed gap** between `MetroidPrime/CDamageInfo.cpp` (ends `0x8018A188`) and
`MetroidPrime/Player/CMorphBallShadow.cpp` (starts `0x8018A9CC`). Claiming a gap is a
four-file carve and out of scope for a `progress` item, so this uses the repo's existing
one-function-per-file port arrangement (cf. `src/MetroidPrime/CGameAreaSetAreaAttributes.cpp`):
a new port-only source, `src/MetroidPrime/Weapons/NWeaponTypesTokens.cpp`, listed in
`files.cmake` with the reason. Both bodies reproduce retail's loop exactly — pointer
walk over `+12`, bound `data + (size << 3)`, branch tested before the body — and
`lock_tokens` lands on retail's own mangled name. The gate is back to **250**.

`fn_8018A6EC` is declared `extern "C" void fn_8018A6EC(rstl::vector<CToken>*)` inside
`namespace NWeaponTypes`, so callers write `NWeaponTypes::fn_8018A6EC`. MWCC rejects
`asm("...")` on a reference type — *"type cannot be made into a global register
variable; only scalers, doubles, floats and vectors are supported"* — so the `fn_` name
has to come from the declaration itself, and it must be the name retail's object has or
the link will not resolve it.

**Rule this confirms, worth more than the two functions:** *a call you add may need a
definition you did not know was missing.* The undefined count is a port-wide number and
a declaration is not a definition; check the callee is compiled into `files.cmake`, not
merely declared in a header.

## Still blocked, re-measured on this tree (not copied from run 1)

* **`FillTokenVector` 70.72 %, 244 B.** Unchanged, and the reason is confirmed: retail
  inlines `rstl::vector::push_back` (`lwz size / lwz data / slwi / addi / stw size+1`),
  ours emits an out-of-line call. Retail's loop is also precomputed-pointer while ours
  walks an index. That is `include/rstl/vector.hpp`, which every pushing unit shares;
  still deliberately not touched.
* **`DrawMuzzleFx` 75.39 % and `UpdateMuzzleFx` 70.54 %: one wall, and it is real.**
  Both call `GetMuzzleFx(mMuzzleEffectIdx)`, which compiles to an out-of-line `bl`;
  retail inlines the guard and **re-loads `this+588` (`mMuzzleEffectIdx`) after every
  call**, which only happens if the indexing is written out at each use. I tried
  spelling it out in `DrawMuzzleFx` — `mMuzzleGenerators.mCount != 0` then
  `mMuzzleGenerators[mMuzzleEffectIdx]` — and **the compiler still emitted the
  out-of-line `bl`**, identical bytes, 75.39 % unchanged. Reverted. The
  `-inline_max_size(125)` in the flags is the lever, not the spelling, and changing it
  is a whole-tree decision.
* **`AsyncLoadSuitArm` 71.19 %, 108 B.** Retail has a 96-byte frame and a 76-byte stack
  temporary; ours has a 16-byte frame. Not an Echoes-only stub — it is a real body that
  needs real reverse engineering, not a spelling.
* **`__ct__` 94.29 %, `Reset` 96.00 %, `ActivateCharge` 95.08 %**: register allocation
  only, same as run 1.
* Unchanged Echoes-only stubs, all still needing real RE: `GetBounds()` 62.32 %,
  `BuildAnimationIdList` 47.33 %, `GetDamageInfo` 9.20 %, `Touch` 2.94 %,
  `CVelocityInfo::Clear` 5.43 %, and the ~20 at single digits (`Fire` 0.22 %, 1804 B,
  `DrawClipCube` 0.24 %, 1660 B, ...), plus the four unnamed
  (`fn_801D9334`, `fn_801D9E20`, `fn_801DBDD8`, `fn_801DBE6C`).

## `BuildDependencyList` — located for the next run, not attempted

1.33 % (300 B) and a TODO stub, but **fully decompilable**, and Prime 1's body
transfers. The two missing tables are both in the merged DOL, measured:

* `skDependencyNames` — the `lis r5,-32709 / addi r5,r5,-21252 / lwzx r5,r5,r0` at
  `0x801D8FD4` indexes a **`const char*[]` in `.rodata` at `0x803AAD34`** (five
  entries), i.e. a data array, not a pool of literals.
* `skAnimDependencyNames` — `lwz r5,-20484(r2)` = **`0x8041AD7C`** in `.sdata` (from
  `tools/sda.py`), also a five-entry pointer table.

The strings themselves are at `0x803AAEF3` (`Power_Anim_DGRP`, `Power_DGRP`, ...) and
`0x803AB03D`. Prime 1's body is
`reserve(a.size()+b.size())` then two `FillTokenVector(..., true)` / `(..., false)`
calls, and `include/Kyoto/CDependencyGroup.hpp` already has `GetObjectTagVector()`. The
reason it was not done here: it needs two new name tables in the class, i.e. a header
change, and this run's budget went on the five confirmed matches. **A `NEW:` line is
deliberately not filed** — it is a function inside the item's own unit, not new work.
