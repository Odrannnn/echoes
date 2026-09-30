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
