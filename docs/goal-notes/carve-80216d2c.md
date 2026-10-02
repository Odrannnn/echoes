# carve-80216d2c — `MetroidPrime/Tweaks/Carve80216D2C`, four functions, `Matching`

`kind: match`. Carved 0x80216D2C..0x80216D5C (0x30 = 48 bytes, 4 functions) out of
`main/auto_03_80216CC4_text` as one `Matching` unit, all four functions. Result: **the judge
passed.**

`./tools/goal_check.sh build/goal/item.json` printed, verbatim:

```
goal_check: item carve-80216d2c (match) target=MetroidPrime/Tweaks/Carve80216D2C
goal_check: baseline .../wt-mp2-goal-L12/build/goal/judge/report.base.json
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 12519 -> 12523   linked 5892 -> 5896
  ok    check_symbol_names.py
  ok    All:  35.33% fuzzy, 29.17% matched, 12.92% linked (12523 / 28465 functions)
  ok    flip_test MetroidPrime/Tweaks/Carve80216D2C.c: PASS, Object(Matching) in configure.py
goal_check: PASS carve-80216d2c
```

## The four files

| file | line | entry |
| --- | --- | --- |
| `src/MetroidPrime/Tweaks/Carve80216D2C.c` | new (79 lines) | 4 functions, plain C, descending by address: `fn_80216D50` (l. 65), `fn_80216D44` (69), `fn_80216D38` (73), `fn_80216D2C` (77) |
| `config/G2ME01/splits.txt` | 1219-1220 | `MetroidPrime/Tweaks/Carve80216D2C.c:` / `.text start:0x80216D2C end:0x80216D5C` |
| `configure.py` | 556 | `Object(Matching, "MetroidPrime/Tweaks/Carve80216D2C.c"),` |
| `files.cmake` | 567 (with the note at 565-566) | `src/MetroidPrime/Tweaks/Carve80216D2C.c` |

`splits.txt` and `files.cmake` place the entry at its address position (ascending: after
`CTweakGui.cpp`'s claim ending 0x80216CC4 / after `Carve80212A24.c`, before `CTweakBall.cpp`'s
claim starting 0x80216DF8 / before `Carve80229410.c`). `configure.py` has no such slot here:
its two bounding units are listed out of order - `CTweakBall.cpp` (0x80216DF8) at 549 comes
*before* `CTweakGui.cpp` (0x80215F80..0x80216CC4) at 555 - so the entry follows `CTweakGui.cpp`,
the unit whose `.text` ends immediately below the claim. `goal_check` is green with it there.
`total_functions` is still **28465** after the `splits.txt` edit.

## Two supporting changes the carve forced

**1. `src/MetroidPrime/Tweaks/CTweakGameHardModeDamageMultiplier.cpp` deleted (30 lines).** The
carve defines the plain symbol `fn_80216D38`, and this port-only unit defined the same one:

```
$ nm build-port-link/CMakeFiles/mp_game.dir/src/MetroidPrime/Tweaks/CTweakGameHardModeDamageMultiplier.cpp.o
0000000000000000 T fn_80216D38
$ find build-port-link -name '*.o' | xargs nm | grep -E 'fn_80216D(2C|38|44|50)'
0000000000000000 T fn_80216D38            # one T, no U
```

A `.c` carve is host-compiled (261 `Carve*.o` under `build-port-link/.../mp_game.dir`), so both
objects would define `fn_80216D38` and `tools/link_check.sh` counts any duplicate as a failure.
Nothing referenced the port-only definition - the carve that used to call it
(`CGameStateGetHardModeDamageMultiplier.cpp`) was superseded by upstream's `CGameState.cpp`,
which calls the mangled `CTweakGame::GetHardModeDamageMultiplier` - and its body (a single
dereference at +0x54) did not reproduce retail's two loads anyway. Removing it is net-zero:
the probe after the change is `763 files, 0 failed, 0 errors; LINKED (291 undefined, 0
duplicates)` and `link_check.log` reads `unchanged from baseline (291 undefined, 0 duplicates)`.
`tools/check_files_cmake.py` requires a configured object to be listed in `files.cmake` or in
its `EXCLUDED` map - which lives under judge-owned `tools/` - so deleting the duplicate is the
only route a lane has. `files.cmake:370-378` records the removal where the entry was.

**2. Two comments this change made false.** `src/MetroidPrime/CWorldTransManager.cpp:28-35`
declared `fn_80216D50` "stays `extern "C"` and undefined for the reason that file gives" - the
file it named is the deleted one, and the symbol is now defined. The edit is comment-only and
measured inert: `main/MetroidPrime/CWorldTransManager` is `50.64655` fuzzy, `6816` matched
bytes, `30` of `76` functions before *and* after. The same stale claim in the excluded
`src/MetroidPrime/Player/CGameStateGetHardModeDamageMultiplier.cpp:18-20` was corrected there.

## What the bodies are

`build/G2ME01/asm/auto_03_80216CC4_text.s`, all four the same two loads:

```
fn_80216D50  80216D50  lwz r3,0(r3) / lfs f1,0x30(r3) / blr
fn_80216D44  80216D44  lwz r3,0(r3) / lfs f1,0x34(r3) / blr
fn_80216D38  80216D38  lwz r3,0(r3) / lfs f1,0x54(r3) / blr
fn_80216D2C  80216D2C  lwz r3,0(r3) / lfs f1,0x58(r3) / blr
```

The item's twins are four matched `CTweakGui` accessors, and their idiom (`mData->misc.<float>`:
`lwz` the pointer member, `lfs` the float at a constant offset) is what the unit uses, through a
`STweakGameBlock` whose fields are named by offset. The double indirection is settled by the
callers, not guessed: `src/MetroidPrime/CWorldTransManager.cpp:29-33` (written by an earlier
lane from the same retail bytes) calls 0x80216D50 "the float at +0x30 of the block
`*gpTweakGame` points at", and `CGameState::GetHardModeDamageMultiplier` (0x80142498) calls
0x80216D38 with `gpTweakGame.get()`. The tree declares no `CTweakGame` block layout, so the
struct names fields by offset and asserts nothing about the words between them.

## Verification, measured

- `./tools/flip_test.sh MetroidPrime/Tweaks/Carve80216D2C.c` -> `PASS  -> kept as Matching`,
  `kept: 1 / 1   failed: 0   skipped: 0`.
- `sha1sum build/G2ME01/main.dol` -> `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`. All 86 RELs
  re-hashed against `config/G2ME01/config.yml` by hand: `RELs compared: 86 mismatched: 0`
  (ninja's `CHECK` edge, `dtk shasum -c config/G2ME01/build.sha1`, also passed).
- `main/MetroidPrime/Tweaks/Carve80216D2C`: `100.0` fuzzy, `matched_code 48 / 48`,
  `matched_functions 4 / 4`, `complete_units 1`.
- The re-split lost nothing: `main/auto_03_80216CC4_text` 12 functions / 308 B -> **6 / 104 B**,
  the carve **4 / 48 B**, and the new `main/auto_03_80216D5C_text` **2 / 156 B**;
  6+4+2 = 12 functions and 104+48+156 = 308 bytes.
- `python3 tools/carve_diff.sh 80216D2C 0x30 build/G2ME01/obj/MetroidPrime/Tweaks/Carve80216D2C.o`
  -> `retail: 12 instructions, 48 bytes` / `ours: 12 instructions, 48 bytes`,
  `differing instructions: 0`, **`BYTE-EXACT`**. First spelling tried.
- `./tools/unit_fit.sh MetroidPrime/Tweaks/Carve80216D2C.c` -> `.text claimed 48 ours 48 retail
  48 fits`, `no extra functions: our object defines only what the retail unit object does`.
- `powerpc-eabi-nm` on the object -> `T fn_80216D2C` 0x0, `T fn_80216D38` 0xC, `T fn_80216D44`
  0x18, `T fn_80216D50` 0x24: ascending `.text` out of a descending source, which is the
  descending-order rule working.
- `python3 tools/check_files_cmake.py` -> `files.cmake: 756 sources; configure.py declares 650
  DOL objects`, `every configured DOL object is either in files.cmake or excluded with a reason`,
  `0 on-disk sources are in no manifest at all`.
- `python3 tools/check_symbol_names.py` -> `checked 525 units; 0 declared names are missing`.
- `python3 tools/check_decl_order.py --unit MetroidPrime/Tweaks/Carve80216D2C.c` -> `ok: 0
  unit(s) checked` - the tool does not resolve `.c` unit names, so a `.c` carve gets no check
  from it (same as `carve-800f1a10`). Moot here: `carve_diff` is byte-exact and `flip_test`
  passes.

## Notes for the next run

1. **The carve rule "remove any `PortLinkStubs.cpp` duplicate" has a second form: a port-only
   *unit* in `files.cmake`.** It cannot be excluded instead - `tools/check_files_cmake.py`'s
   `EXCLUDED` map is under judge-owned `tools/` - so the duplicate definition has to be deleted.
   Measure the reference before deleting: `nm` over `build-port-link/**/*.o` showed one `T` and
   no `U`, which is why the port's undefined count is unchanged at 291 / 0 duplicates.
   `build-port-link/.../CTweakGameHardModeDamageMultiplier.cpp.o` is still on disk afterwards -
   a stale artifact like the stale `auto_*_text.o` the other carve notes mention. It is *not*
   linked: the regenerated `build-port-link/build.ninja` has `grep -c
   CTweakGameHardModeDamageMultiplier` -> `0` (and `Carve80216D2C` -> 4), and the linker's own
   duplicate count is 0. An `nm` sweep of that directory therefore shows one extra `T
   fn_80216D38` and is not evidence of a duplicate.
2. **What is left in this neighbourhood**, all still unsourced, all seeder-shaped:
   `auto_03_80216CC4_text` (6 functions, 104 B: `fn_80216CC4`, `fn_80216CD0`, `fn_80216CE4`,
   `fn_80216CF8`, `fn_80216D0C`, `GetTotalPercentage__10CTweakGameFv`) and
   `auto_03_80216D5C_text` (2 functions, 156 B: `GetPakFile__10CTweakGameFv`,
   `ReadTweaks__16CPlayerCameraBobFRC18SLdrTweakCameraBob` - both need mangled C++ names or a
   `.cpp`, and `ReadTweaks` writes 15 `.sdata2` floats). No `NEW:` line: the seeder already
   emits these.
3. `docs/HANDOFF.md` shows as modified in `git status`; that is `tools/goal_check.sh` rewriting
   the derived counts (12523 / 5896). I did not touch it.
