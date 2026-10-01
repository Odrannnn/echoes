# match-prime1-cgameoptions

`MetroidPrime/Player/CGameOptions.cpp` — **judged PARTIAL** (`goal_check.sh`: gate clean,
`matched_functions 34 -> 35 of 35`). The unit is now **100.00% fuzzy, 100.00% matched code,
35 / 35 functions**; it still cannot be flipped, and the reason is now measured rather than
guessed. Diff: `configure.py`, `src/MetroidPrime/Player/CGameOptions.cpp`,
`docs/research/port_link_gap.md`, `docs/research/port_link_gap_list.md`. Nothing committed.

## The three things that were fixed, and the one that is left

The item arrived with two stated blockers. Both are now resolved, and the second turned out to
hide a third.

### 1. `fn_8029AF00` — real blocker, fixed, free

`0x8029AF00` is retail's `SetAreaVolume__11CSfxManagerFiUc` (`config/G2ME01/symbols.txt:11796`),
written in this tree as `CSfxManager::SetAreaVolume(int, uchar)` at
`src/Kyoto/Audio/CSfxManager.cpp:1246`. `SetSfxVolume` declared `extern "C" void
fn_8029AF00(int, uchar);`, a name nothing defines once our object is the one in the link.

Fixed by calling `CSfxManager::SetAreaVolume(0, sfxVol)` and deleting the declaration.
**Measured cost: none** — the unit is unchanged at 35/35 before and after. objdiff does not
compare `R_PPC_REL24` target names.

Side effect the tooling demanded and I applied: `fn_8029AF00` is no longer a missing port
symbol, so `docs/research/port_link_gap_list.md` lost the `- fn_8029AF00` row,
`docs/research/port_link_gap.md`'s table row `unmangled: fn_/lbl_/globals` went **64 -> 63**,
and the port link went from 250 to **249** undefined. `tools/link_gap.py` and
`tools/check_docs_claims.py` both fail without those two edits ("stale: `fn_8029AF00` is listed
but no longer missing"). These are derived-state updates the tools themselves ask for, not
incidental churn.

### 2. The `rstl::sort` COMDAT copies — real blocker, and the item's reason understated it

The item said the flip's second obstacle was "the 8 COMDAT/template extras". They are real, but
the failure mode is much worse than "extra bytes", and this is the finding worth keeping:

**`rstl::sort_by_key(vec);` — which scores 100.00% — makes this object emit weak COMDAT copies of
`rstl::sort`, `__insertion_sort` and `__sort3` (696 bytes). mwldeppc places them at `0x80161D04`,
which is the range `auto_03_80161D04_text.cpp` owns, and pushes `fn_80161D04` itself to
`0x80161FAC` — eight bytes short of `MetroidPrime/CEnvFxManager.cpp`.**

Measured, flipped link:

```
0x80161d04 W sort<...pointer_iterator<pair<Ui,Ui>,...>__4rstlF...>
0x80161ec8 W __sort3<pair<Ui,Ui>,...>
0x80161f40 W __insertion_sort<...>
0x80161fac T fn_80161D04          <- should be 0x80161d04
0x80162264 T AreaLoaded__13CEnvFxManagerFv   <- should be 0x80161fbc
DOL 907d8fc56e32d3cb6d171ad9b7b7cb8b8fc7ef14   (want 6ef9b491...)
```

So `rstl::sort` (the template) and `fn_80161D04` (the `extern "C"` name in
`auto_03_80161D04_text.cpp`) are the same retail code under two names, and the linker keeps both.
This is why the unit was `34 / 35` at 99.75% *and* unflipable at the same time.

**The fix is not "call `fn_80161D04` directly".** Doing exactly that leaves
`ResetControllerAssets` at **99.93662%** — the same 17 instructions, nine of them
outgoing-argument-slot assignments (`addi r3,r1,20` vs `addi r3,r1,32`, `stw r6,28(r1)` vs
`stw r6,24(r1)`, ...), because the call is no longer written in `sort_by_key`'s own argument
order. What reaches 100.00% with no COMDAT copy is a wrapper with `sort_by_key`'s exact shape:

```cpp
template < typename T >
inline void sort_by_key_fwd(T& container) {
  fn_80161D04(container.begin(), container.end(),
              rstl::pair_sorter_finder< typename T::value_type,
                                       rstl::less< typename rstl::select1st<
                                                       typename T::value_type >::value_type > >(
                  rstl::less< typename rstl::select1st< typename T::value_type >::value_type >()));
}
```

I checked that the instruction stream of `ResetControllerAssets` is byte-identical between
`rstl::sort_by_key(vec)` and `fn_80161D04(vec.begin(), vec.end(), ...)` written directly — the
COMDAT copies are the *only* difference; objdiff's per-function score is not what distinguishes
them. **A `sort_by_key`-shaped wrapper, not `sort_by_key` itself, is the lever.** Same shape as the
`CScriptSpindleCamera` finding: the *call form* moves the score, the declaration form does not.

### 3. `mw_version="GC/2.0p1"` — the unit needed it to be 35 / 35

The fourth `progress` run found this but its `configure.py` change was not in the tree when this
item was queued, so `TuneScreenBrightness` was still 84.12% and the unit 34 / 35. Re-applied
(`configure.py:557`). Under 2.0p1 the whole unit is retail's instruction stream. All 35 functions
verified by `build/report.json`, not by objdiff's summary line.

## What still stops the flip: 28 bytes of `.sdata2` and an 8-byte section alignment

**Not a content mismatch.** Our pool and retail's bytes are identical, word for word:

```
ours   .sdata2 (0x1c = 28 B, align 8):  3b808081 00000000 43300000 80000000 3f800000 3ec00000 3e800000
retail 0x8041C4F4..0x8041C510:          3b808081 43300000 80000000 3f800000 3ec00000 3e800000 00000000
```

The 8-byte `43300000_80000000` (= 2^52 + 2^31) is MW's int->float bias constant, which
`GetHudAlpha`, `GetHelmetAlpha` and `TuneScreenBrightness` all build. **It gives the section
8-byte alignment**, and mwldeppc honours the section's alignment rather than the claim's start, so
our 28 bytes land at `0x8041C4F8` instead of `0x8041C4F4` and overrun into the next symbol.
Measured: 2081 symbols move `+8`, first one `lbl_8041C510` -> `0x8041c518`, `DOL
8485d0b7a27232021e819495c2ffb71c444383fe`.

Claiming the range does **not** help. I added `.sdata2 start:0x8041C4F4 end:0x8041C510` to
`splits.txt` (then `unit_fit.sh` reports `claimed 28 / ours 28 / retail 28 / fits`) and the DOL is
still wrong, because the 8-byte alignment moves the data regardless:

```
claim 0x8041C4F4..0x8041C510 -> 8485d0b7a272
claim 0x8041C4F0..0x8041C510 -> 85763c862768
claim 0x8041C4F8..0x8041C514 -> 85763c862768
```

**The route that works is to name the four 4-byte `.sdata2` globals so only the 8-byte bias double
stays pooled — and `lbl_8041C4F8` is exactly that double's DOL address, so the claim would then be
`0x8041C4F8..0x8041C500`, 8 bytes, 8-aligned, correct.** That is one `extern "C"` per constant, and
it is blocked on `TuneScreenBrightness` (see below). `lbl_8041C4F4` (= `3b808081` =
0.003921569f, the alpha divisor) **is** reachable today, but only through a wrapper with the
operands the other way round:

```cpp
namespace { inline float alpha_of(int v, float s) { return s * v; } }
// hudAlpha * lbl_8041C4F4                      -> GetHudAlpha / GetHelmetAlpha  97.31%
// lbl_8041C4F4 * hudAlpha                      -> same
// alpha_of(hudAlpha, lbl_8041C4F4)  /* s * v */ -> 100.00%      <- works
```

Measured, all on this tree with `tools/fast_try.sh`. **The `.sdata2` floats are not interchangeable
with the `.sdata` table**: naming the table cost nothing, naming these costs functions, because MW
numbers the destination float register from the order it sees the loads.

### `TuneScreenBrightness` — the last measured wall

Naming any of `lbl_8041C500` (1.0f) / `lbl_8041C504` (0.375f) / `lbl_8041C508` (0.25f) drops it
from 100.00% to **87.64706%**, and naming them one at a time does not help:

| spelling | % |
|---|---|
| `f / 4.f * 0.375f + 1.f` (baseline, literals) | **100.00** |
| `f * lbl_8041C508 * lbl_8041C504 + lbl_8041C500` | 87.64706 |
| `lbl_8041C500 + f * lbl_8041C504 * lbl_8041C508` | 87.64706 |
| `f * lbl_8041C504 * lbl_8041C508 + lbl_8041C500` | 87.64706 |
| `f * (lbl_8041C508 * lbl_8041C504) + lbl_8041C500` | 68.82353 |
| name `lbl_8041C508` only: `f * lbl_8041C508 * 0.375f + 1.f` | 87.64706 |
| name `lbl_8041C504` only: `f * 0.25f * lbl_8041C504 + 1.f` | 87.64706 |
| name `lbl_8041C500` only: `f * 0.25f * 0.375f + lbl_8041C500` | 87.64706 |
| `static const float kQ = lbl_8041C508; f * kQ * 0.375f + 1.f` | 87.64706 |
| `inline float tune_of(float x) { return x * C508 * C504 + C500; }` | 87.64706 |
| `inline float tune_of(float x) { return C500 + x * C508 * C504; }` | 87.64706 |

The diff is **one hoisted load** and nothing else: the same 17 instructions, the same
`fsubs / fmuls / fmadds`, the same five registers, the same five constant addresses. Retail puts
`lfs f1` in slot 6 (after `xoris`); ours hoists it to slot 2 (right after `lis`). Retail's
`lfd f2,8(r1)` / `lfs f0` order is reproduced. So it is MWCC's scheduler reacting to the
external symbol, the same class as the `TuneScreenBrightness` wall the `GC/2.0p1` flag closed —
but unlike that one, **this one is caused by the naming, so it is a fixable spelling rather than a
compiler-version difference**. I did not find the spelling; I stopped rather than spend the budget
guessing.

### Also tried and do not repeat

* `sort_by_key_fwd` alternatives that all leave 99.93662% or worse: `fn_80161D04` declared without
  `extern "C"`; declared in an anonymous namespace; via `TIt`/`TCmp` typedefs; the comparator passed
  as a named local; by `const &`; passing `&vec[0], &vec[0] + vec.size()`; `begin`/`end` bound to
  locals; swapping the two iterator arguments (94.21831%); reversing the declaration order (96.77465%).
* `sort_by_key` / `rstl::sort` spelled out directly: 99.93662% but with the COMDAT copies.
* `rstl::default_pair_sorter_finder< rstl::pair< uint, uint > >()` — does not compile.
* `tools/fast_try.sh` takes the unit path **without** `.cpp` (`MetroidPrime/Player/CGameOptions`).
  With `.cpp` it prints `ninja: error: unknown target` and `no such unit in the report`, and reads
  the *previous* `build/report.json` — so it silently returns stale scores. This cost two bogus
  readings early in this run; the script does print `BUILD FAIL`, so check its output.

## Verification

```
sha1sum build/G2ME01/main.dol                6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
./tools/decomp_build.sh                     All: 32.97% fuzzy, 25.83% matched, 12.18% linked
                                            (11511 / 28465 functions)
                                            main/MetroidPrime/Player/CGameOptions:
                                            100.00% fuzzy, 100.00% matched (35 / 35)
./tools/probe_sources.sh                    probe: 753 files, 0 failed, 0 errors;
                                            link: LINKED (249 undefined, 0 duplicates)
python3 tools/check_symbol_names.py         checked 515 units; 0 declared names are missing
python3 tools/check_decl_order.py --unit MetroidPrime/Player/CGameOptions
                                            ok: 2 unit(s) checked, none emits its functions
                                            out of retail order
tools/goal_check.sh build/goal/item.json
  ok    no judge-owned path touched
  ok    gate.sh (DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 11510 -> 11511   linked 5590 -> 5590
  ok    check_symbol_names.py
  ok    All:  32.97% fuzzy, 25.83% matched, 12.18% linked (11511 / 28465 functions)
  flip  flip_test MetroidPrime/Player/CGameOptions.cpp: FAIL - judged below as partial progress
  ok    target rose: main/MetroidPrime/Player/CGameOptions: 34 -> 35 / 35 functions
  ok    no asm added
goal_check: PARTIAL match-prime1-cgameoptions - flip_test ... FAIL, but the target rose;
            commit it and keep the item
```

`unit_fit.sh` on the final tree: `.text claimed 4324 ours 4916` (the 592 B are the
`__as__`/`reserve`/`clear`/`__dt__` COMDAT helpers, which the linker **does** discard here — the
flipped `.text` at `0x80160C40..0x80161D04` and `fn_80161D04` at `0x80161D04` are at retail's
addresses), and `.sdata2 claimed - ours 28`.

Not committed, per the brief. `docs/HANDOFF.md` is rewritten by `gate.sh` inside `goal_check.sh`
and was reverted; the driver regenerates it.

## NEW

(none — `TuneScreenBrightness` needs a source spelling, and the two earlier notes already warn
that spelling hunts on it are unproductive under 2.7; this is the same function under a *different*
cause, but no spelling reached it in this run, so it is characterised above rather than re-queued.)

WALL: TuneScreenBrightness__12CGameOptionsFv 87.64706% - only when the three `.sdata2` constants are named (`lbl_8041C500`/`C504`/`C508`); MW hoists `lfs f1` from slot 6 to slot 2, the other 16 instructions are retail's. 11 spellings measured. Naming `lbl_8041C4F4` (the alpha divisor) instead is free and reaches 100% **via `inline float alpha_of(int v, float s) { return s * v; }`** — `s * v`, not `v * s`.

---

# Carried forward from `progress-prime1-cgameoptions`

## The wall that was a compiler version, not a source spelling

`TuneScreenBrightness` was 84.12% for three runs and ~60 spellings. It is **not** a spelling
problem: under `GC/2.7` MWCC hoists the int->float bias `lfd f3` to slot 2 and pulls `lfs f1` /
`lfs f4` ahead of `xoris`; retail has both one slot later. `Object(..., mw_version="GC/2.0p1")` is
what fixed it, and under 2.0p1 all 35 functions are retail's instruction stream.

Measured over every `GC/*` in `MetroidPrimePort/build/compilers/GC/` (differing instructions,
whole unit): 2.7 = 5, 2.6 = 5, 2.5 = 5, 2.0 = 5, **2.0p1 = 0**, 1.3.2 = 5 + 3 + 3,
1.3 = 5 + 3 + 3 + 74. No `-O`/`-opt`/`-schedule` combination reaches it under 2.7.

**Rule worth keeping: when a unit's only remaining function differs by pure load scheduling,
compile the unit under each `GC/*` and count differing instructions per function before writing
another spelling. `Object(..., mw_version=...)` is supported and `build.ninja` honours it.**

## The `fn_8029AF00` trap, and `fn_` placeholders

Retail's object references several bodies as `U fn_80161D04`, `U fn_8029AF00`,
`U fn_80227694`. A definition of the real function mangles to `SetAreaVolume__11CSfxManagerFiUc`,
leaves `fn_8029AF00` undefined, and the link fails. `tools/check_symbol_names.py` skips
`fn_`/`lbl_` placeholders, so nothing here is a `symbols.txt` rename — the fix is to *call the
mangled name*, as `src/MetroidPrime/Player/CGameOptions.cpp` now does.
---

# match-prime1-cgameoptions — **DONE, `goal_check: PASS`** (lane 2, 2026-10-01)

The unit is **`Matching`**, `flip_test.sh` **PASS**, DOL sha1 retail's
`6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`, `linked 5590 -> 5625`. Not partial.

```
goal_check: PASS match-prime1-cgameoptions
  ok    no judge-owned path touched
  ok    gate.sh (DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 11525 -> 11525   linked 5590 -> 5625
  ok    check_symbol_names.py
  ok    flip_test MetroidPrime/Player/CGameOptions.cpp: PASS
```

Diff: `configure.py`, `config/G2ME01/splits.txt`,
`src/MetroidPrime/Player/CGameOptions.cpp`,
`docs/research/port_link_gap_list.md`. Not committed.

## What the previous run got wrong, measured here

Its notes said the flip was blocked by "28 bytes of `.sdata2` and an 8-byte section alignment",
`TuneScreenBrightness` being an unfixable naming wall, and that naming **any** of the `.sdata2`
globals costs functions. All three are superseded by measurement on this tree:

- The blocker was **never `.sdata2` alignment** — it was that **nothing was claimed at all**, for
  either data section. I added the two claims and the link was correct on the first try.
- `TuneScreenBrightness` needed no change and is no longer a wall. It is untouched, at 100%.
- Naming a `.sdata2` global is **free**, as is naming `.sdata`. The rule that survived is narrower
  than "naming costs": it is **naming a global the source reads with a literal in an expression
  that MW numbers a float register from**. One of those (`lbl_8041C4F4`, the alpha divisor) needed
  the operand order flipped; the other three (`lbl_8041C500/C504/C508`) needed nothing at all.

## The four changes, all measured

**1. `fn_8029AF00` -> `CSfxManager::SetAreaVolume(0, sfxVol)`** (the item's stated blocker #1).
Reproduced exactly first (`undefined: 'fn_8029AF00'` on the flipped link), then fixed by
`src/Kyoto/Audio/CSfxManager.hpp:243`. Unit unchanged at 35/35. Confirms the previous run.

**2. `rstl::sort_by_key(vec)` -> a `sort_by_key`-shaped wrapper** (blocker #2, the 8 COMDAT
extras). The previous run's `sort_by_key_fwd` transcription works verbatim; unit still 35/35, and
`unit_fit`'s extra-function list drops from 8 entries / 1272 B to 5 entries / 592 B (the remaining
five are the `__as__`/`reserve`/`clear`/`__dt__` COMDAT helpers the linker does discard, which is
why the flip holds).

**3. `extern "C" float lbl_8041C4F4` + `alpha_of(int v, float s) { return s * v; }`.** This is the
only naming that needed help, and the previous run's spelling was right: `s * v`, not `v * s`.
Measured on this tree:

| spelling | `GetHudAlpha`/`GetHelmetAlpha` | unit `.sdata2` |
|---|---|---|
| `hudAlpha * 0.003921569f` (baseline, literal) | 100.00% | 16 B |
| `hudAlpha * lbl_8041C4F4` | **97.31%** (both functions) | 8 B |
| `alpha_of(hudAlpha, lbl_8041C4F4)`, `s * v` | **100.00%** | 8 B |

`.sdata2` 16 B -> 8 B is the whole point: with the literal, MW pools an anonymous copy of the
alpha divisor *and* the 8-byte int->float bias double, and our section is then 16 bytes where
retail's is 8.

**4. Claim the two data ranges** — this is what actually unblocked the flip, and it is a
`splits.txt` addition, not a spelling:

```
MetroidPrime/Player/CGameOptions.cpp:
	.text       start:0x80160C20 end:0x80161D04
	.sdata      start:0x80418448 end:0x80418498   <- added, 80 B
	.sdata2     start:0x8041C4F8 end:0x8041C500   <- added, 8 B
```

`.sdata 0x80418448..0x80418498` is the two remap tables, and `.sdata2 0x8041C4F8..0x8041C500` is
exactly the `43300000_80000000` int->float bias double (`= 2^52 + 2^31`) that `TuneScreenBrightness`,
`GetHudAlpha` and `GetHelmetAlpha` all build. Both were `UNCLAIMED` per `tools/range_owner.py`
before, so dtk's `auto_*` objects supplied the bytes *and* mwldeppc placed our own copies past the
end of `.sdata`/`.sdata2`, moving 5734 symbols by +64/+80/+84/+88. With the claims,
`unit_fit.sh` says `fits` on both and the DOL hash is retail's.

`total_functions` is still 28465 after the `splits.txt` edit (verified). `files.cmake` needed no
change: no source file was added, and the unit was already listed at `files.cmake:242`.

### Why the `.sdata` 80 bytes needed no source change

Worth recording, because the obvious next step is wrong. Retail's object references the 20 table
words as **20 separate `R_PPC_EMB_SDA21` relocs** (`lbl_80418448` .. `lbl_80418494`, one per word),
which reads like "name all 20". It is not needed. The two `const ... CStickToDPadRemap[]` arrays
already produce **20 anonymous `@NNN` SDA21 relocs at identical offsets** — byte-identical
`.text`, identical relocation *shape*, only the symbol *names* differ, and objdiff does not compare
SDA21 target names. So the array-of-`pair` spelling stays; the claim is what makes the bytes land
at retail's addresses.

Measured and rejected on the way (all leave `.sdata` at 0 B but break `ResetControllerAssets`):

| spelling | `ResetControllerAssets` |
|---|---|
| baseline (`const pair[]` arrays, literal contents) | **100.00%** |
| `extern "C" pair lbl_80418448[10]`, indexed `lbl_80418448[i]` | 50.15% |
| same, through two local pointers | 53.98% |
| 20 separate `extern "C" uint lbl_8041844X` | build FAILED (MWCC emits `R_PPC_ADDR16_HA`/`LO` for the extern array, not `SDA21`) |

The extern-array form also changes the *addressing mode* — `lis`+`addi` instead of one `lwz` — which
is the same non-`const`-declaration lesson `RUNNING_THE_DECOMP.md` records for retail `.sdata`
globals, in the other direction.

## Derived-state edits the tools demanded (not incidental churn)

`fn_8029AF00` left the port's missing-symbol list and `lbl_8041C4F4` entered it, so
`docs/research/port_link_gap_list.md` loses one row and gains one, and the **net count is
unchanged at 64** — `tools/check_docs_claims.py` now says "docs claims agree with the tree", and
`docs/research/port_link_gap.md` needed **no** edit in the final tree. Note for the next run: I
applied the count edit before the `--write-list` run and briefly wrote 63, which the tool caught
("the generated list has 64"). `port_link_gap.md`'s table is derived from the list, so regenerate
the list first.

## Also measured, do not repeat

* `tools/fast_try.sh` takes the unit path **without** `.cpp`. It rebuilds only the named object and
  regenerates `build/report.json`, so a `BUILD FAILED` line still prints the *previous* report's
  scores. Check for that line before believing a number.
* Claiming a data range does **not** need the `align:` key that some `.bss` claims carry; `.sdata`
  and `.sdata2` claims are plain `start`/`end` (compare `CStaticInterference.cpp`, a `Matching` unit
  that emits `.sdata2` with a plain claim).
* The 592 B of `.text` over the claimed range are the five `__as__`/`reserve`/`clear`/`__dt__`
  COMDAT helpers. They do **not** stop the flip — `flip_test.sh` is the only thing that decides, and
  it passes. Do not chase `unit_fit`'s "over by 592".

## Gates

```
sha1sum build/G2ME01/main.dol                6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
./tools/flip_test.sh MetroidPrime/Player/CGameOptions.cpp
                                            PASS -> kept as Matching
./tools/unit_fit.sh ...                      .text claimed 4324 ours 4916 (COMDAT, discarded)
                                            .sdata  claimed 80 ours 80 retail 80 fits
                                            .sdata2 claimed  8 ours  8 retail  8 fits
./tools/probe_sources.sh                     probe: 754 files, 0 failed, 0 errors;
                                            link: LINKED (249 undefined, 0 duplicates)
python3 tools/check_symbol_names.py          0 declared names are missing
python3 tools/check_docs_claims.py           docs claims agree with the tree
python3 tools/check_decl_order.py --unit MetroidPrime/Player/CGameOptions
                                            ok: none emits its functions out of retail order
./tools/decomp_build.sh                      All: 33.03% fuzzy, 25.90% matched, 12.24% linked
                                            (11525 / 28465 functions)
```

`docs/HANDOFF.md` is rewritten by `gate.sh` inside `goal_check.sh` and was reverted; the driver
regenerates it.

## NEW

(none. `TuneScreenBrightness` is not a wall and is not re-queued; the unit it belonged to is
`Matching`.)

WALL-SUPERSEDED: the previous run's `WALL: TuneScreenBrightness ... 87.64706%` and its
`.sdata2`/8-byte-alignment blocker are both **superseded by this run's measurement** — the unit
flips. Do not re-queue either.
