# progress-prime1-cscandisplay

`kind: progress`, target `MetroidPrime/Player/CScanDisplay`, worktree `../wt-mp2-goal-L2`
(branch `goal/lane-2`).

## Result

`tools/goal_check.sh build/goal/item.json` -> **PASS**.

```
goal_check: item progress-prime1-cscandisplay (progress) target=MetroidPrime/Player/CScanDisplay
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 9876 -> 9879   linked 4895 -> 4895
  ok    check_symbol_names.py
  ok    All:  30.44% fuzzy, 22.36% matched, 11.74% linked (9879 / 28465 functions)
  ok    target rose: main/MetroidPrime/Player/CScanDisplay: 6 -> 9 / 54 functions
  ok    no asm added
goal_check: PASS progress-prime1-cscandisplay
```

Unit: `main/MetroidPrime/Player/CScanDisplay` 6/54 -> **9/54** matched functions;
matched code 3.28% -> 5.99%; fuzzy 15.68% -> 16.37%. Still `NonMatching` in
`configure.py:607`, as a `progress` item requires.

Diff is one file, `src/MetroidPrime/Player/CScanDisplay.cpp` (+27 / -7). No header, no
`configure.py`, no `config/`, no `tools/` change. `docs/HANDOFF.md`'s state block was
rewritten by `goal_check.sh` itself, not by me.

## Per function, as `item.json` asked

All percentages are `fuzzy_match_percent` for that function in `build/report.json`.

| function | before | after | Prime 1's source |
|---|---|---|---|
| `SetScanMessageTypeEffect__12CScanDisplayFP12CGuiTextPaneb` | 5.00 | **100.0** | **matched unchanged** |
| `StopScan__12CScanDisplayFv` | 65.75 | **100.0** | no help (see below) |
| `__dt__12CScanDisplayFv` | 90.43 | **100.0** | no help (no user dtor in Prime 1) |
| `__ct__12CScanDisplayFPC9CGuiFrame` | 99.36 | 99.36 (unchanged) | no help |
| `ProcessInput__12CScanDisplayFRC11CFinalInput` | 0.29 | 0.29 (not attempted) | no help |

### `SetScanMessageTypeEffect` - 5.00% -> 100.0%, Prime 1 verbatim

The item flagged this as "same size as Prime 1", and it is: Prime 1's body compiles
byte-for-byte to retail `0x80116698` with no edit at all. It needed the real
`CGuiTextPane` include and nothing else.

`pane->TextSupport()` is `addi r3,r3,212`; the constants are read as
`lfs f1,-26464(r2)` / `-26528(r2)` / `-26552(r2)`. Resolved against
`_SDA2_BASE_ = 0x804223C0` (not `_SDA_BASE_`) they are `0.1f`, `60.0f`, `0.0f`, in that
order - the `if (type)` arm then the `else`. Useful for the rest of the unit: **float
literals in this TU are SDA2-relative.**

### `StopScan` - 65.75% -> 100.0%, not a Prime 1 problem

Prime 1's `StopScan` is useless here: its enum has a sixth value `kSS_5` and its
`kSS_Done` arm walks `mDataDots`, which Echoes does not have.

Retail `0x801156D0` is 48 bytes and 12 instructions, and two of them are **dead**:

```
lwz r0,12(r3); cmpwi r0,4; beqlr; bgelr; cmpwi r0,0; beqlr
bge  +0x0c        ; -> 0x24, the case body
blr               ; 0x1C  unreachable
blr               ; 0x20  unreachable
li r0,4; stw r0,12(r3); blr
```

Those two `blr`s are what `return` in every arm produces; `break` gives `bltlr`
instead and an 8-byte-shorter (44-byte) body. Spellings measured, all compiling to
`/tmp` and byte-compared against retail:

| spelling | bytes | instrs identical |
|---|---|---|
| `break` in every arm, `kSS_Inactive`/`kSS_Done` grouped | 40 | 6/10 |
| same, `kSS_Done`/`kSS_Inactive` reversed | 40 | 6/10 |
| no `default:` label | 40 | 6/10 |
| `kSS_Inactive` and `kSS_Done` as two separate arms | 40 | 6/10 |
| `default:` first | 32 | 3/8 |
| `if (state == 1 \|\| 2 \|\| 3)` instead of a switch | 40 | 1/10 |
| a 5-case switch followed by an `if` | 36 | 1/9 |
| **`return` in every arm** | **48** | **12/12 - exact** |

So the only thing that mattered was `return` vs `break`; the case order never did.

### `__dt__` - 90.43% -> 100.0%, no Prime 1 source

Prime 1 has no user-written `~CScanDisplay` (all its members are trivially
destructible), so this came straight from retail `0x80116124`. All the member
destruction is compiler-generated and was already matching; the missing 9.5% was a
5-instruction block in front of it. Retail, and `Update` at `0x80114B64` and
`StartScan` at `0x80115830`, all touch the same pair: test a flag, then write
`gpGameState` +0xD9 and clear the same flag.

**The one trap here is that the guard and the clear are the same flag, spelled two
different ways.** I first read the test and the clear as two different members and
wrote `if (mCanOpenLogbook) { ...; mHintsSuppressed = false; }`, which compiles to
`rlwinm. r0,r0,29,31,31` and sits at 99.95%. Measuring all five bitfields one at a
time gave:

| member | test (`rlwinm.`) | clear (`rlwimi`) |
|---|---|---|
| `mScanComplete` | SH=25 | 7,24,24 |
| `x314_1_` | SH=26 | 6,25,25 |
| `mHintsSuppressed` | **SH=27** | **5,26,26** |
| `mPreparePending` | SH=28 | 4,27,27 |
| `mCanOpenLogbook` | SH=29 | 3,28,28 |

Retail is SH=27 for the test and (5,26,26) for the clear - both `mHintsSuppressed`.
With `if (mHintsSuppressed) { ...; mHintsSuppressed = false; }` all 94 instructions are
byte-identical. Do not re-derive this; the flag is at +0x314 and the header's guessed
names happen to be right for it.

`gpGameState` is a `CGameState*` and +0xD9 is inside `CHintOptions` (0xC4..0xDB), but
`CHintOptions`'s two recovered bools sit at +0x10 and +0x11, i.e. 0xD4/0xD5 - the byte
retail writes is 4 past the last recovered member. I did **not** touch `CHintOptions`
or `CGameState` (see `NEW:` below); the byte is written through
`reinterpret_cast<char*>(gpGameState)[0xD9]`, the same `reinterpret_cast` idiom
`src/MetroidPrime/ScriptObjects/ScriptFrontEndDataNetwork.cpp` already uses for
unrecovered members. It compiles to exactly retail's `lwz r3,gpGameState; li r4,0;
stb r4,217(r3)`.

### `__ct__` - 99.36%, blocked, and Prime 1 cannot help

Prime 1's constructor initialises `mDataDots`/`mPaneStates` and stops at
`mScanComplete`; Echoes has 30 more members, so it is not a source.

Ours is 620 bytes, retail 624, and the whole difference is the address of the
`"TXTR_DataDot"` literal:

```
retail  0x80116430  lis  r5,0x803B ; addi r5,r5,-29552 ; addi r5,r5,83   (12 bytes)
ours                lis  r5,@stringBase0@ha ; addi r5,r5,@stringBase0@l   (8 bytes)
```

`0x803A8C90 + 0x53 = 0x803A8CE3 = "TXTR_DataDot"`. Retail emits `HA/LO` against the
**start of the rodata group** and then adds the member's byte offset, because
`"TXTR_DataDot"` is 0x53 bytes into it. Our group holds only that one string, so it
sits at offset 0 and mwcc emits `HA/LO` straight at it. The group is 0x60 bytes in
retail and the 7 strings in front of ours are real:

```
LogbookLineSpacing / %d%% / DownloadedLogBookMsgLeftPart / DownloadedLogBookMsgRightPart
```

`build/G2ME01/obj/.../CScanDisplay.o`'s `.rodata` is 96 bytes; ours is 13 bytes of
content. Those strings belong to Echoes' logbook path, which lives in the
not-yet-written `Update`/`ProcessInput`. Adding unreferenced string literals purely to
move an offset is exactly the kind of change the reviewer rejects, so I left the ctor
alone. The other 616 bytes of the constructor are already correct, including the four
bitfield writes and the whole quaternion/vector tail.

### `ProcessInput` - not attempted

1356 bytes, and it is where the logbook path lives: it is the only caller of
`SetScanMessageTypeEffect` inside the unit and it walks
`mMessage`/`mScrollMessage` text support, `mXMark`/`mAButton`/`mDash` visibility, page
turning, and a `CColor` lerp into `SetFontColor`. Prime 1's `ProcessInput` is close to
this (`GetTotalAnimationTime`, `SetCurTime`, `SetPage`, `SfxStart(0x59f)`) but Echoes
adds the logbook branch, so it is a rewrite of a large function, not an adaptation.
Filling it in is also what would unblock the constructor's rodata group. Not reachable
in one item.

## Reusable codegen facts (not `NEW:` items)

- A `switch` on an enum where every arm ends the function: `return` and `break` differ
  by 8 bytes of dead `blr`s. Read the branch target before assuming the spelling.
- `rlwinm`/`rlwimi` on a `bool : 1` are the only way to find a bitfield's bit; the two
  forms for the *same* bit use different immediates (SH=27/31,31 vs 5/26,26 here).
  Compile five variants and read the immediates rather than reasoning about
  declaration order - mwcc's allocation was forward here, but the pair did not tell
  you which member was which.
- Float literals reached through `r2` with no `la`/`lis` for the base resolve against
  `_SDA2_BASE_` here.
- `mwcceppc` + `wibo` on one source file takes **0.56 s**; the full
  `tools/decomp_build.sh` is minutes. Byte-diffing candidate spellings against retail
  with the same `cflags` from `build.ninja` is cheap and is what found the `return`
  and the bitfield answers.

## Verified

- `./tools/goal_check.sh build/goal/item.json` -> `PASS` (output above).
- Byte equality checked independently of objdiff: with
  `build/binutils/powerpc-eabi-objdump` on `build/G2ME01/src/.../CScanDisplay.o`
  against `build/G2ME01/main.elf`, `StopScan` and `~CScanDisplay` differ only in
  unresolved `bl`/`lwz` relocations, i.e. 0 differing instructions.

## Caveats / follow-ups

- `IsValid__Q212CScanDisplay20CScanTargetPredicateCFRC13CStateManager9TUniqueId` is at
  43.37% (108 bytes) and I did not move it. Retail `0x80116790` keeps the result of
  `TCastToPtr<CScriptPointOfInterest>` in `r3` and reads the active flag at +0x20 off
  *that*; ours keeps the pre-cast `CEntity*` in `r31` and reads it off `r31`, and our
  frame is 32 bytes against retail's 16. Writing it needs
  `poi->GetActive()` on a **complete** `CScriptPointOfInterest`, and this repo has only
  a forward declaration of that class - no `CScriptPointOfInterest.hpp` anywhere. That
  is a missing header, not a spelling problem, so no variant of this file can finish
  it. I left it rather than invent a class layout.

NEW: MetroidPrime/Player/CScanDisplay | progress | MetroidPrime/Player/CScanDisplay | `CScriptPointOfInterest` has no header in this repo, so `CScanDisplay::CScanTargetPredicate::IsValid` (0x80116790, 108 bytes, 43.37%) cannot be finished - retail reads +0x20 off the `TCastToPtr` result, which needs the complete type. Recovering the class (size, the +0x20 active bit, the TCastToPtr class id) is worth a function on its own.

  I did not measure what a recovered `CScriptPointOfInterest` would look like, so the
  100% is an expectation, not a measurement. If the next run on this unit prefers
  spending its time elsewhere, `IsValid` is the thing to skip.

- The `gpGameState` +0xD9 discrepancy is a real finding but not a safe fix from this
  lane: `CHintOptions` is 0x18 bytes at +0xC4, so its bools would have to move from
  +0x10/+0x11 to +0x14/+0x15, which changes `CGameState`'s layout and every unit that
  reads it. Not filed as `NEW:` because I have no measurement showing which members
  occupy +0x12..+0x13; the evidence is only that retail writes +0xD8 and +0xD9
  together (`0x8022B148`, `0x8022B198`).

- I deliberately left `CScanDisplay` `NonMatching`. It has not been near the flip
  (`Update` alone is 3620 bytes at 6.32%, `Draw` 1772 at 0.23%, `PrepareScanDisplay`
  3172 at 0.13%), and `flip_test.sh` was not run, per the item's instruction.

---

# Run 2 (lane L4, 2026-09-30) - re-measured, different approach

The run above is a record of a **failed-then-passed** earlier attempt; its conclusions were
hypotheses, and two of them turned out to be wrong for this tree. Everything below is measured
on **this** worktree.

## Result

`./tools/goal_check.sh build/goal/item.json` -> **PASS**.

```
goal_check: item progress-prime1-cscandisplay (progress) target=MetroidPrime/Player/CScanDisplay
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 10371 -> 10398   linked 5048 -> 5048
  ok    check_symbol_names.py
  ok    All:  31.57% fuzzy, 24.04% matched, 11.83% linked (10398 / 28465 functions)
  ok    target rose: main/MetroidPrime/Player/CScanDisplay: 9 -> 36 / 54 functions
  ok    no asm added
goal_check: PASS progress-prime1-cscandisplay
```

`main/MetroidPrime/Player/CScanDisplay`: **9 -> 36 of 54** matched functions; unit fuzzy
16.37% -> 40.54%, matched code 1112 -> 5748 bytes. Whole-DOL `All:` 31.50% -> 31.57%.
`sha1sum build/G2ME01/main.dol` = `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010` (unchanged).
Still `NonMatching` in `configure.py`, as a `progress` item requires; `flip_test.sh` not run.

Diff is two files: `src/MetroidPrime/Player/CScanDisplay.cpp` (+61/-4) and
`config/G2ME01/symbols.txt` (26 lines, 26 renames). No header, no `configure.py`, no `tools/`.

Two independent gains, described in the order I found them.

## Gain 1: `__ct__` 99.36% -> 100.0% - the previous run's blocker, resolved

The earlier note said the constructor was blocked because retail's `.rodata` group is 0x60 bytes
with 4 logbook strings ahead of `"TXTR_DataDot"`, and that adding unreferenced literals to move
an offset is a change the reviewer rejects. **That reasoning about the reviewer was right and I
kept it** - so the fix is the real code that uses those strings, which is `Update`'s logbook
path. Measured result: `__ct__` 156 instructions, **0 differing** (was 620 bytes vs retail 624).

Retail's four strings and the code that owns each, all in `Update`:

| offset | string | retail use |
|---|---|---|
| 0x00 | `LogbookLineSpacing` | `0x80114D5C` `GetString` -> `mCategoryName` |
| 0x13 | `%d%%` | `0x8011539C` `CBasics::Stringize` -> history number pane |
| 0x18 | `DownloadedLogBookMsgLeftPart` | `0x80115210` `GetString`, appended to the message |
| 0x35 | `DownloadedLogBookMsgRightPart` | `0x80115244` `GetString`, appended after the category |

**mwcceppc lays a string group out in the order the literals are first referenced, so the
offsets are a direct readout of source order.** Probe: compiling the two logbook messages alone
put them at 0x00 and `TXTR_DataDot` at 0x3D; adding `LogbookLineSpacing` and `%d%%` *in that
order* reproduced retail's 96-byte group byte-for-byte
(`diff` of the two `objdump -s -j .rodata` outputs: identical). The ctor's
`lis/addi @stringBase0@l` + `addi r5,r5,83` then falls out on its own - that is the whole 4-byte
difference the earlier run was measuring. `Update` went **6.32% -> 20.11%** from the same code,
so the strings are not decoration.

Correctness of the new `Update` code, since a `progress` count must be earned:

- No `optional_object` is dereferenced unchecked. My first draft did
  `mScanString->Lock()`; retail **never** locks `mScanString` in `Update` (its three `Lock` sites
  are `0x80114BD4` on `mDataDotTexture`, `0x80114C24` on `mScanTexture`, `0x80114C74` on
  `mScanModelToken`) and `mScanString` may be empty, so `operator->` would read past it. Removed.
- The widget loop is bounded by `min(mHistoryWidgets.size(), mHistory.size())`; retail indexes
  `mHistoryStrings` with the node stride and would read past it if the two disagreed, which the
  host build (64-bit pointers) would fault on rather than wrap. Added the bound deliberately -
  it is a host-safety fix, not a byte-match claim, and `Update` is at 20% so nothing was lost.
- Nothing was deleted: the `kSS_Inactive` early-out and every existing store are untouched.

## Gain 2: 26 `fn_*` functions renamed after our own byte-identical symbols - +26

The unit's 54 functions are only 16 named; the other 38 are `fn_<addr>` because retail's DOL has
no symbol for them. **Many were already byte-identical to code we emit** and scored 0% purely for
want of a name. `tools/fnmap.py` pairs by instruction-text hash; the sanctioned follow-up is
`tools/apply_rename.py`.

I did **not** use `tools/autorename.py` blind. It renames every hash match including ambiguous
ones, and here that is wrong: `fn_80114E8C`/`fn_80115178` are byte-identical **to each other** and
both match our single `construct<CScannableObjectInfo>`, so one rename would be a duplicate
symbol. I required (a) the hash unique among retail functions, (b) exactly one distinct
candidate name on our side, (c) the target name already present in our object's symbol table,
(d) no duplicate old or new name across the batch. That yields **26 of 28** hash matches; the 2
dropped are the ambiguous pair above. `fn_80115594`/`fn_8011605C` match **none** of our code
(0 candidates) and were left alone.

Verified independently of objdiff: for all 26, the retail object's function under the new name
hash-matches a function in our object, and the new name exists in our object's symbol table
(26/26). `tools/report_diff.py` calls each one a `RENAMED ... (0.00% -> 100.00%)` and prints
`no regression`; `+27 functions at 100%` (26 renames + `__ct__`), `linked 5048 -> 5048`,
0 `GONE`, 0 `WORSE`, 0 `UNLINKED`.

## Corrections to the earlier run's notes (both were wrong on this tree)

- **"Adding unreferenced string literals to move an offset is what the reviewer rejects"** -
  right about the reviewer, and the resolution is that the literals must be *used*. They belong
  to `Update`'s logbook path, so write that path. This is what took `__ct__` to 100%.
- **"38 unnamed functions, 16 named"** is right, but the earlier run did not try the renames.
  They are worth 26 functions and need no source change at all. Worth checking first next time.

## A trap that cost me most of this run's wall-clock

`tools/fnmap.py` output depends on a **freshly built** `build/G2ME01/src/.../*.o` **and** a
`build/G2ME01/obj/.../*.o` regenerated by objdiff. Reading either stale gives wrong answers with
no error: I got 25 candidates from a stale pair, then 1, then 0, then the correct 28 after
`./tools/decomp_build.sh`. **`./tools/decomp_build.sh` first, then `fnmap.py`, every time.** If
the counts move without a source edit, suspect this before suspecting the compiler.

A second trap: `apply_rename.py` replaces lines in place and is safe, but a hand-rolled
`open(...,'w').writelines(dict.values())` truncated all 25933 lines of `symbols.txt`. I noticed
it via `git diff --stat` and restored with `git checkout`. Keep the edits in `apply_rename.py`.

## Reusable facts (not `NEW:` items)

- **A string literal's offset in `.rodata` is a readout of source order.** Add the literals in
  the order the offsets demand and the group assembles itself; then check the consumer function
  byte-for-byte. This turned a "blocked" 99.36% function into 100% with no layout change.
- **Fast iteration without ninja:** `mwcceppc` on one file with the `cflags` from `build.ninja`
  takes **0.25 s** (the earlier run said 0.56 s). Copy the `cflags`/`$mw_version` block out of
  `build.ninja`, wrap it in `wibo build/tools/sjiswrap.exe`, and the result is **byte-identical**
  to the ninja object (verified with `cmp`). Diff candidate spellings against retail at that
  speed; run the real build only to confirm.
- **Byte-compare functions, not percentages.** Comparing the `objdump -d` instruction streams of
  retail vs ours per function is what found the rodata cause and proved 0 regressions. It is
  independent of objdiff, so it cannot be fooled by a scoring change.
- **Hash-pair with a uniqueness test, not `autorename.py`.** Where several retail functions are
  byte-identical to each other, the pairing is ambiguous and a rename would duplicate a symbol.

## Still open (unchanged, and I did not attempt them)

- `IsValid` 43.37% (108 bytes) - still needs a complete `CScriptPointOfInterest`; retail reads
  `+0x20` off the `TCastToPtr` result. The earlier run's `NEW:` for that still stands; I did not
  measure it further and am not re-filing it.
- `Update` 20.11% (3620 bytes), `ProcessInput` 0.29% (1356), `Draw` 0.23% (1772),
  `PrepareScanDisplay` 0.13% (3172), `StartScan` 60.00% (1696) - all still large.
- 12 `fn_*` remain unnamed and unmatched: `fn_80114CB8`, `fn_80114D78`, `fn_80114DFC`,
  `fn_80114E4C`, `fn_80114E8C`, `fn_80115178`, `fn_80115594`, `fn_801155CC`, `fn_8011605C`,
  `fn_80116094`, `fn_801166E8`, `fn_80116704`, `fn_80116BA0`. `fn_80114E8C`/`fn_80115178` are the
  ambiguous pair. The others have no byte-identical counterpart in our object, so they need real
  source, not a rename. `fn_80116BA0` (212 bytes, 6 `stw` of r4..r9 to r3+0..20) looks like a
  6-pointer struct copy and is the cheapest of them to name if someone recovers the struct.
