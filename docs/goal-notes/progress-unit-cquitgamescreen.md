# progress-unit-cquitgamescreen

`kind: progress`, target `MetroidPrime/CQuitGameScreen`. The unit stays `NonMatching`; this is
judged on `report.json`'s per-function exact matches.

## Result

**3 -> 7 of 11 functions matched, +4 to the global count (11863 -> 11867).** `goal_check.sh
build/goal/item.json` exits 0, `goal_check: PASS`. Four functions were taken to 100%:

| function | bytes | before | after |
|---|---|---|---|
| `ProcessUserInput__15CQuitGameScreenFRC11CFinalInput` | 136 | 2.9% | **100%** |
| `DoAdvance__15CQuitGameScreenFP14CGuiTableGroup` | 144 | 2.8% | **100%** |
| `DoSelectionChange__15CQuitGameScreenFP14CGuiTableGroupi` | 68 | 43.5% | **100%** |
| `SetColors__15CQuitGameScreenFv` | 168 | 2.4% | **100%** |

Untouched: `Draw` (220 B, 1.8%), `FinishedLoading` (664 B, 1.7%), `fn_80221D90` (84 B, 0.0%),
`fn_80221D2C` (100 B, 0.0%). Nothing got worse; the three already-matching functions
(ctor, dtor, `Update`) are still 100%.

## Files

- `src/MetroidPrime/CQuitGameScreen.cpp` - the four bodies (the only decompilation).
- `src/MetroidPrime/PortCGuiAccessors.cpp` - **new**, port-only host definitions of the two
  `GuiSys` callees the new code calls. See "the port link" below; this is why it exists.
- `files.cmake` - one entry for the above, with the reason, in the same style as the neighbouring
  `Port*.cpp` entries.

Nothing in `tools/`, `build/goal/` or `docs/` was edited by me. `gate.sh` rewrites the derived
counts in `docs/HANDOFF.md` and `docs/RUNNING_THE_DECOMP.md` itself (it runs with
`MP_GATE_DOCS_WRITE=1`); I reverted those two so the diff is only the three files above.

## The port link: why a new file was needed

This is the part that is not obvious and cost the only real detour. `ProcessUserInput` and
`SetColors` call into `GuiSys`, and **`GuiSys/` is not in the port build at all** - every unit under
it is in `tools/check_files_cmake.py`'s `EXCLUDED` list, and a lane cannot un-exclude them
(`tools/` is the judge's). So each new call opened an undefined symbol in the port link and the
gate failed:

```
link_check: STRICT FAIL - regression gate: 326 undefined against a baseline of 324 (GREW)
  GATE FAIL: probe link-gap
```

The repo already has the arrangement for exactly this case: `Port*.cpp` files that carry host
copies of bodies whose units cannot be listed - `PortCTweakBall.cpp`, `PortTweakGlobals.cpp`,
`PortAudio.cpp`, `PortIOWins.cpp`, `PortModuleManager.cpp`. `PortCGuiAccessors.cpp` follows that
precedent. Bodies are copied character for character from `src/GuiSys/CGuiWidget.cpp` and
`src/GuiSys/CGuiFrame.cpp`; nothing is stubbed, skipped or made to announce itself.

It took three passes to get back to 324, and the middle pass is worth recording because I got it
wrong by checking the wrong thing. Adding `SetColor` and `CGuiFrame::ProcessUserInput` closed 2 and
opened 1 (`CGuiWidget::RecalcWidgetColor`). Adding `RecalcWidgetColor` closed that and opened 3
(`CGuiObject::Parent/NextSibling/ChildObject`). I had written in the file's header that those three
were "already defined in the port link" on the strength of a `grep -c` that returned 0 - I was
reading *absence from the undefined list* as *presence in the link*, which is the same slip
`PROCESS_LESSONS.md` warns about. Adding the three one-line accessors (each a single field read,
`lwz r3,off(r3); blr` in retail) closed them:

```
link_check: unique undefined symbols 324
link_check: duplicate definitions   0
link_check: unchanged from baseline (324 undefined, 0 duplicates)
```

`diff` of `build-port-link/link_undefined.txt` against the stashed tree's is now empty, so no
symbol was traded for another.

## The `CFinalInput` bitfield layout - worth knowing, and it is a trap

`ProcessUserInput` tests `lbz r0,42(r31); rlwinm. r0,r0,26,31,31`. Byte 42 is `btns3`. The
non-obvious part is that `rlwinm ...,SH,31,31` tests input bit `31-SH`, so retail wants **bit 5 of
btns3**, and **MWCC allocates these `uchar x : 1` fields counting *down* from bit 6, not up from
bit 0.** Compiling one caller per accessor measures it:

| accessor | emitted | bit |
|---|---|---|
| `PA()` | `rlwinm 25` | 6 |
| `PB()` | `rlwinm 26` | 5 |
| `PX()` | `rlwinm 27` | 4 |
| `PY()` | `rlwinm 28` | 3 |
| `PZ()` | `rlwinm 29` | 2 |
| `PL()` | `rlwinm 30` | 1 |
| `PR()` | `rlwinm 31` | 0 |
| `DA()` / `DB()` (btns1) | `rlwinm 29` / `30` | 2 / 1 |
| `PStart()` (btns4) | `rlwinm 28` | 3 |

So retail's 26 is **`PB()`**, not `PL()`. My first attempt wrote `PL()` and scored 99.85% - one
instruction off, and the kind of near-miss a percentage would have hidden. The packing is not just
my probe's word for it: **`CCredits` is Matching at 49/49 and its `input.PA()` emits
`rlwinm 25`** in the linked object, which anchors the rule to retail. Only 9 of the 32 accessors
were measured, so the *rule* is a guide and the *shifts* are the fact; the comment in the source
says so. The header's `btns3` field names are guesses, so what is pinned by this match is the
**bit**, not the name.

## Per-function notes

**`SetColors`** (168 B) - the only one that took iteration. Retail 0x802219EC builds two stack
`CColor`s then loops `i` in 0..1 over `mChoiceTable`'s worker widgets, recolouring the selected row
`(200,200,200,255)` and the other `(50,50,50,255)`. Two spellings:

| spelling | score |
|---|---|
| ternary inline in the `SetColor` call, selection re-read each iteration | 75.9% |
| hoist `selection` into a local **and** name the widget pointer | **100%** |

Both parts of the hoist are load-bearing. Hoisting only the selection still scored 75.9% (the
compiler kept the selection in a callee-saved register across the virtual call and materialised the
colour address too early). The vcall target is vtable offset 44, which I confirmed rather than
assumed by dumping `__vt__10CGuiWidget` (`.data` 0x803B9040) and resolving slot 11 to
`GetWorkerWidget__10CGuiWidgetFi`. Note this function does **not** null-check the widget the
virtual returns - there is no `cmplwi r3,0; beq` between the `bctrl` and the `bl SetColor`, unlike
`CGuiTableGroup::SetColors` (`fn_80279260`) which does. Retail relies on both choice rows existing.

**`DoAdvance`** (144 B) - reads the *caller's* selection, `lwz r0,200(r4)`, not a member. Both arms
play the **same** sound, 1507; row 0 sets `kQA_Yes`, row 1 `kQA_No`. 100% first try.

**`DoSelectionChange`** (68 B, was 43.5%) - `SetColors()` then sound 1505, pan 64. Neither argument
is read. 100% first try.

**`ProcessUserInput`** (136 B) - forwards to the loaded frame when there is one, then on the button
sets `mAction = kQA_No` and plays 3068 with **pan 63**, but only when `mType !=
kQT_ContinueFromLastSave`. One iteration only, for the `PL()` -> `PB()` correction above.

### The sfx argument pattern

All four SfxStart calls use the same tail, and retail's `-16600(r2)` / `-16604(r2)` are **SDA2**
loads, not SDA: `tools/sda.py s2:-16600` gives `kAllAreas__11CSfxManager` (0x8041E2E8) and
`s2:-16604` gives `kMedPriority__11CSfxManager` (0x8041E2E4). I first resolved them against the
default SDA base and got two anonymous float labels - a plausible wrong answer, exactly the
failure `tools/sda.py`'s header warns about. So the calls are
`SfxStart(id, vol, pan, CSfxManager::kAllAreas, false, false, CSfxManager::kMedPriority)`, and the
sound ids are plain `li` immediates (1505, 1507, 3068), not loads from `.sdata2` - so the source
spelled them as literals. I left them as literals rather than inventing named constants; a named
constant would also have to reproduce the immediate.

## What I did not do, and why

**`Draw` (220 B, 1.8%) and `FinishedLoading` (664 B, 1.7%) are untouched.** `Draw` needs an
unclaimed `.data` table at 0x8042CF50 (three consecutive floats indexed by `mType`, reached through
`lis r4,-32709; addi r6,r4,-12464`), plus a `CColor` built by a byte-splice
(`lbz r0,8(r1); rlwimi r0,r7,0,0,23` - reusing the alpha byte for the mType) and a
`CCameraFilterPass::DrawFilter` call. Getting the `.data` table right means either claiming a range
no unit owns or defining the floats here, and either way the object grows functions retail's does
not define - which `unit_fit.sh` says can never be `Matching`. Bad risk for one function, so it
stays for a lane that wants the carve.

**`fn_80221D2C` (100 B) and `fn_80221D90` (84 B) are untouched.** Both are 3-argument
`__ptmf_scall` thunks - `memcpy` a 12-byte ptmf to the stack, then `bl __ptmf_scall` - i.e.
compiler-generated pointer-to-member-function dispatch helpers. There is no retail source text for
one; they exist because the source called something through a `TFunctor`. Reproducing them means
reproducing the compiler's thunk emission, which is a different problem from decompiling.

**The unit stays `NonMatching` and I did not run `flip_test.sh`.** `unit_fit.sh` reports the same
two extra functions (`__dt__24TCachedToken<9CGuiFrame>Fv` +88, `__dt__18TToken<9CGuiFrame>Fv`
+84) and the same 63 unclaimed `.rodata` bytes as the **stashed** tree, so neither is mine; both are
pre-existing. `.text` is short by 880 bytes, which is the four functions plus `Draw` and
`FinishedLoading` still missing.

No `WALL:` line: the four functions I took to 100% did not sit at a plateau, and the two I left
(`Draw`, `FinishedLoading`) are blocked on a carve decision rather than on spelling, so per the
prompt that belongs here rather than in a `WALL:`. No `NEW:` items either - nothing I found blocks
work whose success raises a count.

## Verification

```
$ ./tools/goal_check.sh build/goal/item.json
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 11863 -> 11867   linked 5727 -> 5727
  ok    check_symbol_names.py
  ok    All:  33.61% fuzzy, 26.73% matched, 12.64% linked (11867 / 28465 functions)
  ok    target rose: main/MetroidPrime/CQuitGameScreen: 3 -> 7 / 11 functions
  ok    no asm added
goal_check: PASS progress-unit-cquitgamescreen
```

Also run directly: `./tools/link_check.sh` (324 undefined, unchanged from baseline, 0 duplicates),
`./tools/unit_fit.sh MetroidPrime/CQuitGameScreen.cpp`, and
`python3 tools/check_decl_order.py --unit MetroidPrime/CQuitGameScreen.cpp`
(`ok: 0 unit(s) checked` - the file's declarations were already in retail-descending order, which
is why no reorder was needed; the file declares functions in the order the original scaffold did).
Not committed, per the prompt.