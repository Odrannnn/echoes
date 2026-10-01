# cmorphball-unclaimed-80258790-802588dc (match, `MetroidPrime/Player/CMorphBall`)

Lane 5, 2026-10-01. **Result: `goal_check` PARTIAL** - `fn_800CD244` reached **100.00%** and the
unit's matched count rose **113 -> 114 of 158**, global matched **11931 -> 11932**, every other
check green, no asm. The flip still fails, now on **one** undefined name instead of five.

The item is the `NEW:` line from `docs/goal-notes/cmorphball-wakepool-order.md` (run 2), verbatim:
write `fn_800CD244` and `fn_800CD35C`, the two largest of the unit's six unwritten bodies, and add
host definitions of the two unclaimed-range workers they call. One of the two is done; the other is
not, and the reason is measured rather than guessed.

## What the two functions are

Neither is a `CMorphBall` member: both take a *path* in `r3` and a *cursor* in `r4`. Every call site
agrees - `CMorphBall::FindClosestSpiderBallWaypoint` passes its two stack locals at `r1+848` and
`r1+784` (`bl fn_800CD244` at 0x800CD004, `bl fn_800CD35C` at 0x800CCFB8), and the unclaimed
`fn_8021EDF4` does the same at 0x8021EEB8.

They are the two drivers of the **spider-ball path parser**, over two workers in the unclaimed
`.text` gap `auto_03_80257AF8_text.o` covers - `nm` over every object in `build/G2ME01/obj` finds
`fn_80258790` and `fn_802588DC` only there:

```
fn_80258790  0x80258790, 0x14C = 83 insns - decode up to four opcodes out of the path's halfword
                             array and dispatch each through a 16-entry jump table at 0x803B8AC0
                             (8 distinct targets, the exit block eight times); returns two parked
                             halfwords packed into one word.
fn_802588DC  0x802588DC, 0x94  = 37 insns - advance the cursor past one waypoint.
```

Both are spelled with `SMorphBallPathCursor` / `SMorphBallPath` / `SMorphBallWaypoint`, local structs
declared in `CMorphBall.cpp`, because no class in this port models them. Three layout facts are
measurements, not guesses:

- **The cursor's +0x20 word is a halfword index.** Every load of it in both workers is
  `slwi rX,rX,1` then `lhz`/`lhzx` on `path->mWaypoints` (0x802587A0/0x802587AC,
  0x802588E8/0x802588EC), which is what an `unsigned short*` index compiles to. So the waypoint
  record is **36 halfwords** - `fn_800CD244` advances the index by exactly 36 (`addi r0,r3,36`) and
  then builds a `CPlane` out of the three `CVector3f` at +0/+12/+24 and stores its four floats at
  +56 - i.e. 0x48 bytes, which is the record's size.
- **The record's +0x28/+0x2C pair is a `CMaterialFilter`**: `FindClosestSpiderBallWaypoint` loads
  those two words straight out of the returned pointer into a stack `CMaterialFilter` and hands it
  to `CMaterialFilter::Passes` (0x800CD018) with no copy.
- **The flag byte at +0x18 is a bit-field group**, and how many fields it is *declared* with is
  load-bearing - see "The two spellings that decide `fn_800CD244`" below.

## The two spellings that decide `fn_800CD244`

1. **The guard's polarity.** `if (cursor->mRemaining == 0) { ...workers... }` emits `bne` over the
   worker block; retail has `beq` **into** it (0x800CD268), with the record-consuming half as the
   fall-through. The same rule `GetMorphBallModel` in this file already records: mwcceppc branches to
   the continuation when the condition is false, so the condition has to be the negation -
   `if (mRemaining != 0) { ...consume, return... }` then the workers below it. That also puts the
   worker block after the record block, where retail has it, and gets the shared `b` to the
   epilogue for free.
2. **The flag byte's bit-field count.** `SMorphBallPathFlags` declares **exactly two** one-bit
   fields, `mLastWaypoint` (written) and `mBuildPlane` (read), and that is not cosmetic. Measured,
   with throwaway probes since deleted:

   - a bool store into a one-bit field is `lbz` / `cntlzw` / `rlwimi r0,rX,<SH>,24+bit,24+bit` /
     `stb`, and retail's store `rlwimi r0,r3,2,24,24` is a store of bit 0 **of a `cntlzw` value**
     (a store of the literal `1` into the same field emits `<SH>` = 7 instead - the shift follows
     the value, so the shift alone cannot identify the bit);
   - a bool read is `rlwinm. r0,r0,31-bit,31,31`, so retail's `rlwinm. r0,r0,26,31,31` is a read of
     **bit 5**;
   - the allocation is **not** declaration order over the fields a function uses. Writing all six
     fields of a six-field group allocates bits 0..5; reading only the last of them allocates
     **bit 1**, and `(void)`-ing the four in between changes nothing. Reading the last field of a
     **two**-field group allocates bit 5 - which is retail's rotate;
   - the alternative, a mask on the byte, cannot reach it: `(x & 0x10)`, `(x & 0x20)` and
     `(x & 0x40)` emit rotates **28, 27 and 26** - one bit lower than the mask in every case, on a
     byte member, a 32-bit member and a `unsigned char*` dereference alike. The mask that would
     produce retail's rotate 26 is `0x40`, which is not the bit retail tests.

   With six declared fields the function scores 99.70% and differs in exactly one instruction;
   with two it is byte-identical (`tools/dol_fd.py`: "70 retail insns, 70 ours, 0 differing lines").
   The bits retail's other two writers touch - bit 1 (`fn_802588DC`'s `rlwimi r0,r6,1,25,25`) and
   bit 6 (`FindClosestSpiderBallWaypoint`'s `rlwimi r0,r3,7,24,24`) - are therefore deliberately
   **left undeclared**, and the comment above the struct says so, because declaring them is exactly
   the edit that costs this function its match.

## `fn_800CD35C` is not done: five spellings, none reach the shape

It is the same two workers with the record-consuming tail dropped: run them until `mRemaining` is
non-zero, return `mNext`. Retail runs the worker block **three times** and then tail-calls itself
(0x800CD38C, 0x800CD3C8, 0x800CD40C, then `bl fn_800CD35C` at 0x800CD444). This source gets one
copy and the call, so 34 of retail's 65 instructions are absent and the score is 47.69%.

| spelling | score |
|---|---|
| `if (mRemaining != 0) return mNext;` + workers + `return fn_800CD35C(path, cursor);` | **47.69%** |
| `while (mRemaining == 0) { workers } return mNext;` | 45.06% |
| `do { workers } while (mRemaining == 0); return mNext;` | 43.92% |
| `for (;;) { if (mRemaining != 0) return mNext; workers; }` | 47.15% |
| `if (mRemaining == 0) { workers; return self(); } return mNext;` | 47.00% |

Three compiler behaviours, measured with throwaway probes (deleted), say why none of them can
produce that shape and where the next run should not go:

- mwcceppc **unrolls a loop whose trip count it knows**: a `for` with a constant bound of 4 emits
  four copies. It does **not** unroll `while (*p != 0)`, which comes out rotated with a backward
  branch. Nothing in this source gives it a trip count.
- mwcceppc **does not inline recursion at all** - measured for a `static` function and for an
  `extern "C"` one, tail-position and not: the self-call is always a `bl`. So retail's three copies
  are neither a loop unroll of this source nor an inlined recursion.
- The bit-field and mask measurements above mean the difference is **not** register allocation: the
  missing instructions are two whole copies of the worker block and their two guards.

Per the brief this is a measured wall, not a `NEW:` item: the spellings and scores are here so the
next run skips them.

## The two host definitions, and why they are transcriptions

`fn_80258790` and `fn_802588DC` are defined in `src/MetroidPrime/PortGlobals.cpp` beside
`fn_8033D2F4`, for the reason `fn_80045E18` is: they live in an unclaimed gap that `build.ninja`
links into `main.dol` (so the DOL needs nothing), while the **host** link needs them now that
`CMorphBall.cpp` calls them. Measured without them:

```
link_check: STRICT FAIL - regression gate: 326 undefined against a baseline of 324 (GREW), 0 duplicate(s)
GATE FAIL: probe link-gap
```

and with them `build/gate-probe.log` reads `LINKED (324 undefined, 0 duplicates)` - back to the
baseline, and `build/gate-linkcheck.log` reads "unchanged from baseline". They are transcriptions of
retail's own instruction sequences, not stubs and not stand-ins: every store is a store retail makes
on the field retail reads it from, including the 8-way opcode dispatch (read out of the jump table at
0x803B8AC0, `tools/dol_read.py`) and the index rounding-up-to-even at the end of `fn_80258790`.
Retail's two flag folds in `fn_802588DC` are **not** written out, and the comment says why: each is
a `cntlzw` into a `rlwimi` whose shift field never selects a bit `cntlzw` can produce (it returns
0..32), so both stores write back the byte they read.

Neither is reachable from the port's boot - their only callers are `fn_800CD244` / `fn_800CD35C`,
which are reached only from `FindClosestSpiderBallWaypoint` (a scaffold) and the unclaimed
`fn_8021EDF4`. The struct layouts are repeated on both sides (they are file-local in each) and each
side says so; the host's are wider than retail's for the `fn_8033D2F4` reason (a 64-bit pointer
narrowed to `uint` would truncate).

## Measured

```
$ ./tools/goal_check.sh build/goal/item.json
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 11931 -> 11932   linked 5727 -> 5727
  ok    check_symbol_names.py
  ok    All:  33.71% fuzzy, 26.89% matched, 12.64% linked (11932 / 28465 functions)
  flip  flip_test MetroidPrime/Player/CMorphBall.cpp: FAIL - judged below as partial progress
            build failed: mwldeppc undefined: 'CAnimRes::kDefaultCharIdx'
  ok    target rose: main/MetroidPrime/Player/CMorphBall: 113 -> 114 / 158 functions
  ok    no asm added
goal_check: PARTIAL cmorphball-unclaimed-80258790-802588dc - flip_test ...: FAIL, but the target rose;
commit it and keep the item

$ python3 tools/report_diff.py build/goal/judge/report.base.json build/report.json
  matched  11931 -> 11932   linked 5727 -> 5727   (+1 functions at 100%, 0 units newly linked)
    +100%    main/MetroidPrime/Player/CMorphBall :: fn_800CD244
  no regression
```

Per function (`build/report.json`): `fn_800CD244` (280 B) **`None` -> 100.0**, `fn_800CD35C` (260 B)
**`None` -> 47.692307**, every other one of the unit's 158 unchanged (`report_diff.py` over all
units: +1, 0 worse, 0 changed). Unit `matched_code` **15064 -> 15344** of 66600 (exactly +280, the
size of the function that matched), `.text` fuzzy **29.799759 -> 30.406366**, `matched_functions`
**113 -> 114**. Functions with no body in our object: **four -> two** (`fn_800D0584` and
`fn_800C5420` are what is left).

`sha1sum build/G2ME01/main.dol` = `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`, unchanged. All 86 RELs
unchanged (`rel_module_order: 86 modules, unchanged`). `build/gate-probe.log`: `probe: 745 files, 0
failed, 0 errors; link: LINKED (324 undefined, 0 duplicates)` - **324, the baseline**.
`build/gate-link.log`: `ok: 319 MISSING symbol(s), all accounted for in port_link_gap_list.md`.
`check_symbol_names.py`: 515 units, 0 missing. `check_raw_offsets.py`: `ok: 166 raw-offset site(s) in
70 file(s)` - **this diff adds none** (`CMorphBall.cpp`'s own count is still 4; the new cursor and
record layouts use named members, not `this + N`). `unit_fit.sh`: **51 functions / 5272 bytes**
present in ours but not in the retail unit object, identical to the previous head's measurement -
both new functions are symbols retail's object has. `check_decl_order.py --unit
main/MetroidPrime/Player/CMorphBall`: the unit is permuted, as it was at HEAD and as
`build/gate-order.log`'s "31 permuted, all 31 accounted for in decl_order.md" already lists; the two
new bodies are declared **descending** (0x800CD35C then 0x800CD244, between `fn_800CD460` at
0x800CD460 and `fn_800C93B0` at 0x800C9380), so this diff adds no inversion.

## Codegen rules this run adds

- **MWCC's mask test is one bit lower than the mask.** `(x & 0x20)` emits
  `rlwinm. r0,r0,27,31,31`, which selects bit **4**; `(x & 0x10)` emits 28 and `(x & 0x40)` emits
  26. Measured on a byte member, a 32-bit member and a `unsigned char*` dereference - the same
  rotate each time. **This is why `fn_800CD4B8` (96.32% in this build) is one instruction short**:
  its `(*flags & 0x10) != 0` gives 28 where retail has `rlwinm. r0,r0,27,31,31`. Not fixed here -
  it is another function's item - but the next run on it should not spend a search on the spelling.
- **A one-bit bool store's `rlwimi` shift field follows the value being stored**, not the field:
  the same field emits `<SH>` = 7 for a literal `1` and 2 for a `cntlzw` result. Only the mask
  (`24 + bit`) identifies the field.
- **`cntlzw` into `rlwimi` is always a no-op store** (measured on retail's own bytes in three
  functions here: `fn_800CD244`, `fn_802588DC`, `FindClosestSpiderBallWaypoint`). The idiom is
  reproducible from a bool store into a one-bit field; the byte it produces is the byte it read.

## Not filed as `NEW:`

- `fn_800CD35C` is a measured wall (spellings and scores above), which the brief says belongs in the
  notes rather than the queue.
- **The flip's `undefined:` list is now one name: `CAnimRes::kDefaultCharIdx`.** It was five at the
  start of this run (`CAnimRes::kDefaultCharIdx`, `fn_800C33DC`, `fn_800C88C0`, `fn_800CD244`,
  `fn_800CD35C`); this diff removed four of them. `CAnimRes::kDefaultCharIdx` is declared at
  `include/MetroidPrime/CAnimRes.hpp:37` as `static const int`, has no definition in any unit (there
  is no `src/MetroidPrime/CAnimRes.cpp`), is **not** in `config/G2ME01/symbols.txt`, and is already
  in the port's tolerated baseline (`docs/research/port_link_baseline.txt`). It blocks the flip
  test's *link*, not any count, so it is not filed - but it is the one thing standing between this
  unit's flip test and a matching failure, and the next run that wants the flip should start there.
- **Pre-existing, not mine:** `docs/research/raw_offsets.md`'s summary line reads "165 sites in 69
  files" where the tool measures **166 in 70**. The enforced part (the per-file headings) agrees
  with the tree, and this diff adds no raw-offset site, so the staleness is a previous head's.

## Files

- `src/MetroidPrime/Player/CMorphBall.cpp:8` (`#include "Kyoto/Math/CPlane.hpp"`), `:491-644` - the
  three local structs, the two `fn_8025xxxx` declarations, `fn_800CD35C` and `fn_800CD244` and the
  measurement behind every shape in them.
- `src/MetroidPrime/PortGlobals.cpp:1147-1294` - host `fn_80258790` / `fn_802588DC` beside
  `fn_8033D2F4`, with the layouts and the reason.

No `configure.py`, no `config/`, no `splits.txt`, no `files.cmake`, no `.s`, no asm, nothing under
`tools/` or `build/goal/`; `docs/HANDOFF.md` shows as modified after `goal_check.sh`, which is the
judge rewriting its own derived counts, and it was reverted. Not committed, per the brief.