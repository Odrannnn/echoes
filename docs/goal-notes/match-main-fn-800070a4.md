# match-main-fn-800070a4

`kind: match`, `target: MetroidPrime/main.cpp`. **`fn_800070A4` reaches 100.0%** (80 bytes) and
`fn_80007040` with it (100 bytes, 4.00% -> 100.0%). The unit does not flip, and it is not close;
the flip is blocked by the same four pre-existing link-level holes and the unit is still SHORT by
7708 bytes of `.text`.

## Re-measured first, and the item's reason was stale

The item's reason quotes "fn_800070A4 is 86%". On the tree I started from (`cf0bc6f`, clean,
`goal/lane-2`) that was **not** the state: both functions were `extern "C" void f() {}` one-
instruction stubs, so the report said

```
baseline  cf0bc6f  main/MetroidPrime/main  60 / 99 functions, matched_code 7364, fuzzy 47.053387%
  fn_800070A4    5.0%  80 B        fn_80007040    4.0%  100 B
                        tree      10092 matched functions, 4918 linked
```

Run 4 of `match-main-fn-80009274` is not on this branch, so **both bodies were written here from
scratch**, using run 4's identification (which was good and cost nothing to re-check) rather than
its code.

## The correction that closed the item: it was never register allocation

Run 4 filed this item with a diagnosis that is **wrong, and superseded here**:

> "the only thing separating it from 100% is the register allocation of the copy loop ... every
> register is one lower than retail's, and the 4th field lands in `r3` where retail keeps `r6` ...
> **one more volatile is live at the allocation point in retail and not in ours**."

It is not. The register set in run 4's 86% shape was an artifact of the loop condition, not of the
cursor. **The whole 14% was one branch word.** Measured, compiling the body with each spelling and
comparing raw bytes against `build/G2ME01/main.elf` 0x800070A4-0x800070F4:

```
retail   cmpwi r4,0 / 4c 81 00 20  blelr      <- BO=4 BI=1  : branch if CR0 GT  -> "ble"
!= 0    cmpwi r4,0 / 4d 82 00 20  beqlr      <- BO=12 BI=8 : branch if CR0 EQ -> "beq"   19/20
> 0     cmpwi r4,0 / 4c 81 00 20  blelr                                             20/20
```

Every other instruction - including the whole copy loop, the cursor in `r10` and the five fields in
`r9/r8/r7/r6/r0` - is byte-identical in the `!= 0` shape too. mwcceppc pre-tests a `!= 0` loop with
`beq` and a signed `> 0` loop with `ble`; retail wrote the second. The two-line experiment run 4
asked for is one operator.

Eleven spellings were compiled and measured on this run. Six are 20/20 byte-identical:

| spelling | result |
|---|---|
| `while (count-- > 0)` | **80 B, 20/20** (kept) |
| `while (0 < count--)` | 80 B, 20/20 |
| `for (; count > 0; --count)` | 80 B, 20/20 |
| `for (; 0 < count; --count)` | 80 B, 20/20 |
| `while (count > 0) { --count; ... }` | 80 B, 20/20 |
| `while (count > 0) { ...; --count; }` | 80 B, 20/20 |
| `while (count-- != 0)` | 80 B, 19/20 - the branch word only |
| `while (count) { --count; ... }` | 80 B, 19/20 - the branch word only |
| `while (count-- >= 0)` | 84 B |
| `do { --count; ... } while (count > 0)` | 76 B - the pre-test moves into the body |

Run 4's other two conclusions survive and were re-measured: the records must be **inline at
`+0x04`** (a pointer member reloads the word in the loop and unrolls to 336 B), and the null test
must be **written out** (`if (rec)`; without it one unrolled copy loop, no `cmplwi`).

## What `fn_80007040` needed beyond run 4's note

Run 4 got this one to 100% and its notes do not say how. It is the same body plus one thing: **the
call's result must be discarded and `self` returned instead.** Returning the call's result compiles
and is 88 bytes with no `r31` anywhere; `fn_800070A4(...); return self;` keeps `self` live across the
call, which is what produces retail's `stw r31,28(r1) / mr r31,r3` in the prologue and
`mr r3,r31 / lwz r31,28(r1)` in the epilogue - 25 instructions, 100 bytes. Run 4's other two
spellings for it were re-measured and confirmed: the four `self->` stores must precede the
temporary's five, and the temporary must be five explicit member stores (`= {0}` is hoisted into
`.rodata` and copied instead of zeroed on the stack).

## What changed (one source file: 93 insertions, 2 deletions)

| file | change |
|---|---|
| `src/MetroidPrime/main.cpp:43` | `#include "MetroidPrime/Player/CGameStateBlocks.hpp"` (for `SGameStateWorlds`) |
| `src/MetroidPrime/main.cpp:646-665` | `SGameStateRecord` / `SGameStateRecords` + `CHECK_SIZEOF`, with the measurements as comments |
| `src/MetroidPrime/main.cpp:666-732` | real bodies for `fn_800070A4` and `fn_80007040` |

The two struct types are **declared in `main.cpp` and not added to `CGameStateBlocks.hpp`**, even
though that header documents the block: it is shared with three `Matching` units
(`CGameStateBlockConstruct`, `CGameStateBlockCopyCtor`, `CGameStateBlockDtor`, all 1/1) and its
`SGameStateWorlds` already spells this same tail as `u32 x10_count` + `u8 x14_rec[4][16]`. Two
views of the bytes have to coexist and only one of them can be the one the constructor is written
against; the header's is not changed. Placement is unchanged and still correct - descending by
retail offset (`CheckTerminate` 0x800070F4, `fn_800070A4` 0x800070A4, `fn_80007040` 0x80007040,
`CheckReset` 0x80006BA4).

## Measured, from `build/report.json`

`main/MetroidPrime/main` **60 -> 62 of 99** functions, `.text` fuzzy
**47.053387% -> 48.030212%**, `matched_code` 7364 -> 7544 (**+180**). Tree-wide, full per-function
diff against the `cf0bc6f` baseline (`tools/report_diff.py .tmp/baseline-report.json
build/report.json`):

```
matched 10092 -> 10094   linked 4918 -> 4918   (+2 functions at 100%, 0 units newly linked)
+100%  main/MetroidPrime/main :: fn_80007040
+100%  main/MetroidPrime/main :: fn_800070A4
no regression
```

**No function anywhere got worse and no unit lost a match.**

`./tools/goal_check.sh build/goal/item.json` on this tree:

```
ok  no judge-owned path touched
ok  gate.sh (DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
ok  counts: matched 10092 -> 10094   linked 4918 -> 4918
ok  check_symbol_names.py
ok  All:  31.07% fuzzy, 23.36% matched, 11.78% linked (10094 / 28465 functions)
flip  flip_test MetroidPrime/main.cpp: FAIL - judged below as partial progress
ok  target rose: main/MetroidPrime/main: 60 -> 62 / 99 functions
ok  no asm added
goal_check: PARTIAL match-main-fn-800070a4 - flip_test ... FAIL, but the target rose; commit it and keep the item
```

Gates on their own: `sha1sum build/G2ME01/main.dol` = `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`;
`tools/probe_sources.sh` = `750 files, 0 failed, 0 errors; link: LINKED (250 undefined, 0
duplicates)` - the same 250 as the baseline, so no NEW and no GONE; `check_symbol_names.py` =
`checked 504 units; 0 declared names are missing`; `gate.sh`'s `hashes vs config.yml` ok for all 86
RELs. `docs/HANDOFF.md` appears in this run's `git diff` with only its state block re-derived
(10092 -> 10094) - that is `tools/check_docs_claims.py` inside `gate.sh`, not an edit of mine.

`tools/unit_fit.sh MetroidPrime/main.cpp`: `.text` claimed 17608, ours **9900**, **SHORT by 7708**;
`.ctors` SHORT by 4; the extras list is **16 functions / 1340 bytes**, unchanged; `.sbss` is
`over by 21`, which run 3 of `match-main-fn-80009274` already measured as inherited. The two new
bodies are retail's own bytes, so they move `.text` by exactly the 180 that `matched_code` moved.
`check_decl_order.py --unit "MetroidPrime/main" --list` reports the same inherited **41** permuted
functions; the two bodies stay in the descending-by-retail-offset run their addresses call for, so
this change adds **nothing** to the permutation.

## The flip: the same four pre-existing blockers, and I did not touch them

`./tools/flip_test.sh MetroidPrime/main.cpp` -> `FAIL -> reverted (tree rebuilt: DOL
6ef9b491d0cc08bc81a124fdedb8bfaec34d0010)`, and mwldeppc names the same four that runs 1-4 of
`match-main-fn-80009274` saw, unchanged:

```
multiply-defined: 'CErrorOutputWindow::__vt' in CErrorOutputWindow.o
undefined: 'fn_80008C28'      undefined: 'fn_80009224'
undefined: 'rstl::rc_ptr<CMapWorldInfo>::ReleaseData()'
```

WALL: MetroidPrime/main.cpp flip - the same four pre-existing link-level blockers
(`CErrorOutputWindow::__vt` multiply-defined, `fn_80008C28` / `fn_80009224` undefined,
`rc_ptr<CMapWorldInfo>::ReleaseData()` undefined) and .text still SHORT by 7708 bytes over 37
unmatched functions, so no amount of work on any single function in this unit can flip it; treat
this unit as `progress`-shaped and requeue it as such. (Re-measured this run: 7708 and 37, not
copied.)

## The port side

No change, and none needed. `src/MetroidPrime/main.cpp` is not in `files.cmake`, so neither
function reaches the host build and the two new struct types cannot collide with a port symbol.
The port's own `void fn_80007040(SGameStateWorlds* self)` (`CGameStateCtor.cpp:129,273` and
`CGameStateStreamCtor.cpp:221,430`) is **C++ linkage**, a different symbol from the `extern "C"`
one here, and that is pre-existing - the two cannot be unified without a config rename, which is
not this item. The port probe inside `gate.sh` is unchanged at the baseline's 250 undefined, no NEW
and no GONE. Neither function is on the boot path.

## Two things a next run should have

1. **When a decomp body sits at 86-95% with the right *size* and one instruction different, read
   the branch word's BO/BI before changing the source shape.** Retail `4c 81 00 20` is BO=4 BI=1
   (`ble`) and ours `4d 82 00 20` was BO=12 BI=8 (`beq`): the whole gap was a `!= 0` written where
   retail wrote a signed `> 0`, and run 4 spent a full item on register allocation that was never
   wrong. It cost this item 4 runs of a queue slot.
2. **The experiment harness, so the next one is not rebuilt.** `tools/probe_cc.sh` does **not**
   work for `src/MetroidPrime/main.cpp` (it carries `CIOWinCtor`'s flags, which lack
   `-i extern/musyx/include` and the four `MUSY_*` defines, so the probe compile dies on
   `musyx/musyx.h`). Copy main.cpp's `cflags` line out of `build.ninja:301-311` instead - a full
   compile is then **0.4 s**, which is what makes a ten-spelling sweep affordable. Two lane-local
   (uncommitted) helpers did the work this run:
   `.tmp/opencode/mainprobe.sh` (compile any source with those flags, disassemble) and
   `.tmp/opencode/score.py` (compare the 80/100 bytes against `build/G2ME01/main.elf` and print an
   instruction-by-instruction diff, normalising branch targets to `@`). A separate 40-line TU
   reproduces main.cpp's per-function register allocation exactly, so the sweep did not have to
   rebuild the unit.

## Leads still open, unchanged by this run (none measured here, so no `NEW:`)

- `CMain::SetMaxSpeed` 99.25% - run 4's `WALL`, still the closest thing in the unit. Its trigger
  (the tail `stb r0,144(r30)` deciding whether `lwz r0` comes first in the epilogue) is the one
  input not yet varied: which register the tail's value lives in.
- `__dt__80006678` (88 B) is still a genuine three-way tie between byte-identical `single_ptr`-
  shaped bodies. Break it from `CGameGlobalObjects`' real member layout, not from the score.
- `fn_80009224` (`rstl::rc_ptr<CWorldLayerState>::ReleaseData`) is one of the four link blockers and
  a `symbols.txt` rename would remove it. It adds **no** matched function - the body is emitted as a
  weak COMDAT in `CGameState.o`, not in `main.o`, so objdiff pairs nothing - which makes it a link
  fix, not a count.
- `fn_80008B04` (`TOneStatic<CGameGlobalObjects>::operator delete`, 44 B) is the last member of that
  family. Run 3 enumerated the three routes; #1 is a regression (`~CMain` is 100% today and would
  come off it) and #3 is a contrived host.

---

# Run 5 (lane 2, `b2f1420`, tree `wt-mp2-goal-L2`)

`fn_800070A4` **86.00% -> 100.00%** (80 B, 20 of 20 instructions byte-identical).
`fn_80007040` was already 100.00% when this run started, so only one function moved.

## Re-measured first: the item was live, and the previous run's fix was not

On the clean tree this run started from, `build/report.json` had `main/MetroidPrime/main`
**63 / 99**, with `fn_800070A4 86.00% / 80 B` and `fn_80007040 100.00% / 100 B`. Not stale, and
the previous run's `fn_800070A4` edit had **not** landed here - so run 4's and run 5's bodies both
had to be re-measured from scratch on this tree. They do not reproduce:

| spelling | run 4/5 notes claim | **measured in the real unit here** |
|---|---|---|
| `while (count-- > 0)` | 80 B, 20/20 (kept) | 80 B, **7 of 20** - byte-identical to `for (int i = 0; i < n; ++i)` |
| `while (count-- != 0)` | 80 B, 19/20 | 80 B, **7 of 20** |
| `for (; count > 0; --count)` | 80 B, 20/20 | 80 B, **7 of 20** |

**The whole spelling table in run 5's notes is measured in a TU that is not this unit.** The claim
"A separate 40-line TU reproduces main.cpp's per-function register allocation exactly" is false:
in the real unit the loop condition makes **no difference at all**. Measured: 14 loop forms
(`for i<n`, `while n-->0`, `for ;n>0;--n`, `while n>0` with the decrement before and after the
body, `for i=n;i>0;--i`, `for i!=n`, `while n`, the `u32` forms, `for ;n--;++rec`, ...) x 2 store
forms (struct assignment and five explicit member stores) - all 80 bytes, all the same
allocation, best 7 of 20. Every one of those spellings should be skipped by the next run; the
loop is not the variable.

## The actual cause: the function returns `self`, and that is what the allocation is made of

Run 4 said "one more volatile is live at the allocation point in retail and not in ours" and
run 5 rephrased it as register allocation. Both are right about the symptom and neither found
the lever. The lever is the **return type**: `self` (r3) is live across the whole copy loop when
the function returns it, which moves mwcceppc's loop temp pool up by one register.

```
before   extern "C" void fn_800070A4(...)                     7 of 20, objdiff 86.00%
after    extern "C" SGameStateRecords* fn_800070A4(...) {     20 of 20, objdiff 100.00%
           ...                                                 return self;
         }
```

The whole diff is the declaration's return type plus one `return self;`. The loop, the null test
and the struct assignment are untouched from the tree this run started on. Nothing about the
instruction sequence changes: only which registers hold it - cursor `r9` -> `r10`, fields
`r8/r7/r6/r3` -> `r9/r8/r7/r6`, everything else (`mtctr r4`, `cmpwi r4,0`, `blelr`,
`cmplwi r10,0`, `beq`, `bdnz`) already matched.

The diagnostic that finds it in one shot, and is worth reusing: **sweep the number of copied
fields and read the temp window off the disassembly.** With the null test written `if (rec)`:

```
fields copied   1      2         3          4            5
cursor          r6     r6        r7         r8           r9
field temps     r0     r3,r0     r6,r3,r0   r7,r6,r3,r0  r8,r7,r6,r3,r0
```

with the test written `if (self->x04_recs)` (which references `self`, so `r3` is live):

```
fields copied   1      2         3          4            5
cursor          r6     r7        r8         r9           r10
field temps     r0     r6,r0     r7,r6,r0   r8,r7,r6,r0  r9,r8,r7,r6,r0
```

The pool is a 5-slot descending window, and it is `{r3,r6,r7,r8,r9}` when `r3` is dead and
`{r6,r7,r8,r9,r10}` when `r3` is live. **Retail's allocation is the second window**, so the only
question is what keeps `r3` alive across the loop - and the answer is the returned pointer, which
needs no instruction at all (the epilogue is just `blr`, since `r3` already holds `self`).

That also kills the obvious-looking wrong answer: writing the null test as `if (self->x04_recs)`
does get the registers right, but then the address is *rematerialised* from `r3` every iteration
and the test compiles to `addic. r4,r3,4` instead of retail's `cmplwi r10,0` - 19 of 20. So the
test has to be on the cursor **and** `r3` has to be live, and the only way to get both is the
return. (`if (&self->x04_recs[0])` behaves the same 19-of-20 way.)

## Robustness, so the next run does not re-derive it

With the `SGameStateRecords*` return and `return self;`, **24 combinations** are 20 of 20: the
struct assignment `*rec = value` x 4 cursor spellings (`self->x04_recs`, `&self->x04_recs[0]`,
`self->x04_recs + 0`, a `char*` cast) x 6 loop forms. The copy must be the **struct assignment**:
the same body with the five member stores spelled out is only 18 of 20, because mwcceppc then
schedules `addi r10,r3,4` *before* `stw r4,0(r3)` instead of after it. That ordering, not the
registers, is the whole 90%.

Not in the 24, and not needed: `SGameStateRecords&` / `s32 n` / `u32 n` / a non-const `value`
reference all fail to compile with `-maxerrors 1` because `fn_80007040` passes the cast pointer
and the signature no longer agrees; a `static` helper or `__builtin_memcpy` do not compile or do
not match; `u32 i` counts give 25-30%, `!rec`-style tests unroll the loop to 312 B, an end-pointer
loop gives 88 B, and putting the count store after the loop gives 5%.

## What changed (one source file)

| file | change |
|---|---|
| `src/MetroidPrime/main.cpp:700-732` | comment rewritten with the measurements above; `fn_800070A4` declared `extern "C" SGameStateRecords*` and ends `return self;` |

`src/MetroidPrime/main.cpp` is **not** in `files.cmake`, so nothing here reaches the host build;
`src/MetroidPrime/mainMid.cpp:679`'s `extern "C" void fn_800070A4() {}` is a different TU that
never links with this one, and it is pre-existing. Placement is unchanged and still descending by
retail offset (`CheckTerminate` 0x800070F4, `fn_800070A4` 0x800070A4, `fn_80007040` 0x80007040,
`CheckReset` 0x80006BA4), so this adds nothing to the unit's pre-existing 41 permuted functions.

## Measured, from `build/report.json`

`main/MetroidPrime/main` **63 -> 64 of 99** functions, `.text` fuzzy
**48.129715% -> 48.193320%**, `matched_code` 7588 -> 7668 (**+80**). Tree-wide
(`python3 tools/report_diff.py .tmp/opencode/baseline-report.json build/report.json`):

```
matched 10096 -> 10097   linked 4918 -> 4918   (+1 functions at 100%, 0 units newly linked)
+100%  main/MetroidPrime/main :: fn_800070A4
no regression
```

`fn_80007040` stayed at 100.00% - the call site is unchanged and the return value is discarded.

`./tools/goal_check.sh build/goal/item.json`:

```
ok  no judge-owned path touched
ok  gate.sh (DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
ok  counts: matched 10096 -> 10097   linked 4918 -> 4918
ok  check_symbol_names.py
ok  All:  31.07% fuzzy, 23.36% matched, 11.78% linked (10097 / 28465 functions)
flip  flip_test MetroidPrime/main.cpp: FAIL - judged below as partial progress
ok  target rose: main/MetroidPrime/main: 63 -> 64 / 99 functions
ok  no asm added
goal_check: PARTIAL match-main-fn-800070a4 - flip_test ... FAIL, but the target rose; commit it and keep the item
```

Gates separately: `sha1sum build/G2ME01/main.dol` = `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`;
`tools/probe_sources.sh` = `750 files, 0 failed, 0 errors; link: LINKED (250 undefined, 0
duplicates)`, the baseline's 250; `check_symbol_names.py` = `checked 504 units; 0 declared names
are missing`; `tools/check_docs_claims.py` = `docs claims agree with the tree`. `docs/HANDOFF.md`
appears in this run's `git diff` with only its state block re-derived (10096 -> 10097) - that is
`check_docs_claims.py` inside `gate.sh`, not an edit of mine.

`tools/unit_fit.sh MetroidPrime/main.cpp`: `.text` claimed 17608, ours 9928, **SHORT by 7680**;
`.ctors` SHORT by 4; the extras list is unchanged at **16 functions / 1340 bytes**; `.sbss` is
`over by 21`, inherited.

## The flip: the same four pre-existing blockers, verified pre-existing this time

`./tools/flip_test.sh MetroidPrime/main.cpp` -> `FAIL -> reverted (tree rebuilt: DOL
6ef9b491d0cc08bc81a124fdedb8bfaec34d0010)` with

```
multiply-defined: 'CErrorOutputWindow::__vt' in CErrorOutputWindow.o
undefined: 'fn_80008C28'      undefined: 'lbl_80418EA0'      undefined: 'fn_80009224'
```

I re-ran the flip on the **stashed clean tree** and got byte-identical output, so none of the four
is mine. (Runs 1-4 saw `rstl::rc_ptr<CMapWorldInfo>::ReleaseData()` where this run sees
`lbl_80418EA0`; the count and the `CErrorOutputWindow::__vt` multiply-defined are the same, so the
set moves a little as other units land.)

WALL: MetroidPrime/main.cpp flip - the same four pre-existing link-level blockers
(`CErrorOutputWindow::__vt` multiply-defined; `fn_80008C28` / `lbl_80418EA0` / `fn_80009224`
undefined, all reproduced on the stashed clean tree) and .text still SHORT by 7680 bytes over 35
unmatched functions, so no amount of work on any single function in this unit can flip it; treat
this unit as `progress`-shaped and requeue it as such. (Re-measured this run: 7680 and 35.)

## The port side

No change and none needed - `src/MetroidPrime/main.cpp` is not in `files.cmake`, so neither
function is in the host build and the new return type cannot collide with a port symbol. The port
probe inside `gate.sh` is unchanged at the baseline's 250 undefined, no NEW and no GONE. Neither
function is on the boot path.

## The experiment harness (it was still in this tree from run 5; rebuild it if it is gone)

`.tmp/opencode/mainprobe.sh` compiles `src/MetroidPrime/main.cpp` with the exact `cflags` line
copied out of `build.ninja:304-315` - **0.4 s a compile**, which is what makes a 24-way sweep
affordable - and `.tmp/opencode/score.py` diffs the two functions against
`build/G2ME01/main.elf` instruction by instruction. `.tmp/opencode/sweepL2.py` +
`candL2*.py` sweep a list of body spellings through it and restore the file afterwards. New this
run and the one worth keeping: **probe the allocation by varying the number of copied fields and
printing the register numbers**, `.tmp/opencode/probe_regs.py`. Do not trust a spelling table
measured anywhere but the unit TU - that mistake is what cost runs 4 and 5 of this item.
