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
