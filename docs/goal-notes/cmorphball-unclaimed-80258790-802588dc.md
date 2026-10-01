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

---

Lane 5 (second run on this item), 2026-10-01. **Result: `goal_check` PARTIAL** -
`DampLinearAndAngularVelocities` reached **100.00%** (was 57.27%), the unit's matched count rose
**116 -> 117 of 158**, global matched **11957 -> 11958**, every other check green, no asm.

## First: the previous run's open question is already closed

`fn_800CD35C` - the wall run 1 stopped on - **is at 100.00% on this tree's head**
(`build/report.json`, `fn_800CD244` and `fn_800CD35C` both 100.0), so the item no longer has an
unwritten body to attack. Run 1's note also said "two functions with no body in our object:
`fn_800D0584` and `fn_800C5420`"; re-measured, **`fn_800C5420` has a body at 96.76%** - only
`fn_800D0584` (0.00%, 188 B) has none. Everything below is what I measured on this tree.

## What landed: `DampLinearAndAngularVelocities` 57.27% -> 100.00%

`src/MetroidPrime/Player/CMorphBall.cpp:1932-1962`. Retail 0x800C5614, 0x100 = 64 insns, ours 64.
Four things are load-bearing, and the ladder below is the measurement of each (all this run,
`tools/fast_try.sh` + `tools/dol_fd.py`):

| spelling | score |
|---|---|
| previous body (both operands of `pow` named locals, one expression each) | 57.27% |
| + named `CVector3f vel`, `vel *= scale` | 51.66% |
| + `CAxisAngle ang` / `ang *= scale` (the `__amu__` call), copy **after** its `pow` | 66.73% |
| both copies **before** their `pow` | **99.53%** |
| both copies before their `pow`, `60.f * dt` written out in both `pow` calls | **100.00%** |
| same, angular half as `ang = ang * scale` (`__ml__`) | 88.44% |

1. **The world velocity is a named local, scaled in place.** Retail copies the three components
   to r1+0x18 *before* `bl pow` (`lfs f0,424(r3)` / `stfs f0,24(r1)` x3) and only then reloads
   and scales them in place (`lfs f2,24(r1)` / `fmuls f2,f2,f3`), passing `addi r4,r1,24`.
   Written as one expression, mwcceppc fuses the copy into the multiply and sinks the lot past
   the call - that is the 57.27% body.
2. **The angular half is `CAxisAngle::operator*=`** (retail calls `__amu__10CAxisAngleFRCf`),
   not `operator*` (`__ml__`). Its parameter is `const float&`, which is why retail spills the
   scalar to r1+8 and passes its address (`addi r4,r1,8` / `stfs f0,8(r1)` / `bl __amu__`).
3. **Both copies are statements before their `pow`**, not after.
4. **`60.f * dt` is written out twice instead of hoisted into a `frames` local.** This is the
   last 0.47%: with the local the function is 99.53% and differs in exactly three register
   numbers - the `60.f` constant goes to **f4** instead of **f0**, the `1.f` constant to **f0**
   instead of f4, and retail's `lfs f0,424(r3)` (velocity x) therefore reuses f0 instead of f3.
   mwcceppc evidently does CSE on the repeated product (one `fmuls f31,f0,f3`, both
   `fmr f2,f31`), and the *unhoisted* form allocates the constant's temp to f0 where the named
   local's does not.

**Codegen rule worth keeping (general, not CMorphBall):** *hoisting a repeated subexpression
into a named local can cost the match even when the instruction sequence is identical.* When
mwcceppc emits one copy of a CSE'd expression, the temp's register can differ from the hoisted
form's, and every later use of that register moves with it. `tools/dol_fd.py` reporting only
register numbers is the tell.

## Measured, not fixed (each is 2-3 instructions of register allocation or branch polarity)

- **`ComputeMaxSpeed` - the best of the ~20 spellings measured this run is 96.97%** (the
  body at HEAD is 96.84%). Retail's tail is
  7 instructions (`lfs f0,95` / `fcmpo cr0,f2,f0` / `bge` / `fmr f1,f2` / `b` / `fmr f1,f0` / `b`);
  every spelling I tried emits 5 or 6, because mwcceppc always coalesces the `95.f` load into the
  **destination** register f1 and drops one arm's `fmr` + `b`. The repository's
  `rstl::min_val(a,b)` is `(b<a)?b:a` and emits `fcmpo cr0,b,a`, where retail compares
  `(a,b)` - so retail's min helper is `(a<b)?a:b`, and spelling it that way
  (`maxSpeed < 95.f ? maxSpeed : 95.f`) gets the operand order and both arm semantics right and
  still coalesces. Also measured: `min_val(95,maxSpeed)` 94.34, `(95.f<maxSpeed)?95.f:maxSpeed`
  96.84, `min_val` with both operands named 96.84, `if (maxSpeed<95.f) return maxSpeed; return
  95.f;` **96.97** (best), `maxSpeed>=95.f?95.f:maxSpeed` 94.21, both halves in one expression
  88.95/89.21. I left the body alone: no spelling reaches 100%, and the 0.13% is not a result.
- **`GetSpiderBallControllerMovement` - still 97.41%, six more spellings all worse.** The
  residue is unchanged: retail's `-55` test is `blt` **to the shared `fneg` block placed after the
  `145` test**, mwcceppc emits `bge` over a duplicated `fneg` instead.
  `if(<-55) return -m; if(>145) return 0; return -m;` 95.93, `else if(<=145)` + trailing return
  94.63, `if(!(>=-55))` 89.88, nested `if(>=-55){if(<=145) return 0;}` 92.41, ternary tail 94.63
  x2. Run 1 measured ~60 more; do not spend another search here.
- **`fn_800C8CE0` - still 74.05%, eight more spellings all <= 74.05.** Retail needs 19 insns
  with a 32-byte frame, three spills (`+8` = `it+1` dead, `+12` = `it+1`, `+16` = `*it`) and
  `mr r31,r3` (the `out` pointer, never read). `SUniqueIdFloats::iterator` **is a class**
  (`include/rstl/pointer_iterator.hpp`), so `*it + 1` is a class temporary - but mwcceppc elides
  it in every spelling I could write: named `lastCopy`, `const iterator&` bound to the temporary,
  temporary + copy into a third local, temp bound and cast, and a by-value `it` parameter are
  all 74.05%. What I could not produce is the third spill and the r31.
- **`fn_800C5420` (96.76%) is register allocation only** - 111 retail insns, 111 ours, and the
  diff is `r28`/`r29` swapped in all three loops plus one `lwz r3` vs `lwz r4`.
- **`fn_800D0584` (0.00%, 188 B, no body in our object)** is a static initialiser: byte-wise
  stores into two `.sdata2` tables, five word stores out of a `lbl_803A84B8` object, a
  conditional pointer, then `__register_global_object` with `lbl_8040D568` (type descriptor) and
  `fn_800D0640` (destructor) as relocations. Writing it needs those four external symbols to be
  linkable, which is a `port`-shaped risk for a decomp item; I did not attempt it.

## Measured

```
$ ./tools/goal_check.sh build/goal/item.json
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 11957 -> 11958   linked 5728 -> 5728
  ok    check_symbol_names.py
  ok    All:  33.79% fuzzy, 26.99% matched, 12.64% linked (11958 / 28465 functions)
  flip  flip_test MetroidPrime/Player/CMorphBall.cpp: FAIL - judged below as partial progress
            build failed: mwldeppc undefined: 'CAnimRes::kDefaultCharIdx'
  ok    target rose: main/MetroidPrime/Player/CMorphBall: 116 -> 117 / 158 functions
  ok    no asm added
goal_check: PARTIAL cmorphball-unclaimed-80258790-802588dc - flip_test ...: FAIL, but the target
            rose; commit it and keep the item

$ python3 tools/report_diff.py build/goal/judge/report.base.json build/report.json
  matched  11957 -> 11958   linked 5728 -> 5728   (+1 functions at 100%, 0 units newly linked)
    +100%    main/MetroidPrime/Player/CMorphBall :: DampLinearAndAngularVelocities__10CMorphBallFfff
  no regression
```

Per function (`build/report.json`): `DampLinearAndAngularVelocities` (256 B) **57.266 -> 100.0**,
every other one of the unit's 158 unchanged (`report_diff.py`: +1, 0 worse, 0 changed). Unit
`matched_code` **15756 -> 16012** of 66600 (exactly +256, the size of the function that
matched), `.text` fuzzy **31.264025 -> 31.428288**, `matched_functions` **116 -> 117**.

`sha1sum build/G2ME01/main.dol` = `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`, unchanged. All 86
RELs unchanged (checked by `gate.sh`, which the judge reports green). The port probe is **323 undefined,
the baseline exactly** (`build/goal/judge/undef.base.count` = 323): dropping `operator*` removed
`__ml__10CAxisAngleRCfRCf`, and the new `__amu__10CAxisAngleFRCf` call is satisfied by
`src/MetroidPrime/CAxisAngle.cpp:29`, so the diff adds no undefined symbol.
`check_raw_offsets.py`: `ok: 166 raw-offset site(s) in 70 file(s)` - this diff adds none.
`unit_fit.sh`: **51 functions / 5272 bytes** present in ours but not in the retail unit object -
identical to the previous head's measurement. `check_decl_order.py --unit
main/MetroidPrime/Player/CMorphBall` still lists this unit as permuted, as it was at HEAD and as
`build/gate-order.log` already accounts for; this diff adds no declaration.

## Not filed as `NEW:`

- The flip's `undefined:` list is **still the one name** run 1 left: `CAnimRes::kDefaultCharIdx`
  (declared `static const int` at `include/MetroidPrime/CAnimRes.hpp:37`, defined nowhere, not
  in `config/G2ME01/symbols.txt`, already in the port's tolerated baseline). It blocks the flip
  test's *link*, not any count. It is also not the binding constraint any more: the unit is at
  117/158, so the flip needs 41 more functions before the link is even reached.
- Everything else above is a measured wall inside this item's own unit, which the item itself
  covers - requeuing it is the driver's job, not a new queue entry.

## Files

- `src/MetroidPrime/Player/CMorphBall.cpp:1932-1962` - `DampLinearAndAngularVelocities` and the
  four measurements behind its spelling.

No `configure.py`, no `config/`, no `splits.txt`, no `files.cmake`, no `.s`, no asm, nothing under
`tools/` or `build/goal/`; `docs/HANDOFF.md` shows as modified after `goal_check.sh`, which is the
judge rewriting its own derived counts, and it was reverted. Not committed, per the brief.

## Lane 5: passed, then failed on the moved tip (2026-10-01 19:03:54Z)

The judged change failed goal_check.sh (exit 1) once rebased onto 38068b1312aa; re-do it against the current tip.

---

Lane 5 (third run on this item), 2026-10-01. **Result: `goal_check` PARTIAL** -
`DampLinearAndAngularVelocities` reached **100.00%** (was 57.27%), the unit's matched count rose
**116 -> 117 of 158**, global matched **11959 -> 11960**, every other check green, no asm.

Run 2's `DampLinearAndAngularVelocities` spelling was measured on a tree whose tip moved under it,
so the driver requeued the item to have it re-done here; this run re-measured everything first and
its recipe still reaches **100.00%** byte-for-byte (`tools/dol_fd.py`: "64 retail insns, 64 ours,
0 differing lines"). The spelling and the ladder behind it are recorded in the source above the
function.

## First, the tree at this tip does not pass `gate.sh` - two pre-existing failures

`HEAD` is `38068b13` ("port: real CGMSinglePlayer"), which removed the reach stub for
`CGMSinglePlayer::CGMSinglePlayer()` and linked the real body. **Neither of these is mine, and
both fail on the clean tree before I touch anything:**

```
$ python3 tools/link_gap.py --rebuild          # clean tree, HEAD 38068b13
port link gap, measured over 742 object(s)
     15  c++ runtime / linker   40  libc/libm   187  aurora source
      1  aurora header only     317  MISSING
link gap not accounted for:
  stale: _ZN15CGMSinglePlayerC1Ev is listed but no longer missing - delete the entry
  GATE FAIL: link-gap
```

That commit's own message records `link_check 322 undefined (323 before)` but did not re-run
`link_gap.py`, so `docs/research/port_link_gap_list.md` still listed the now-provided symbol and
`docs/research/port_link_gap.md`'s generated table still said 244 / rows summing to 318. With the
list entry deleted the first check passes; `python3 tools/check_docs_claims.py` then reported

```
stale:   the gap table says other game methods is 244, the generated list has 243
stale:   the gap table's rows sum to 318, the generated list holds 317
```

**Both files are fixed in this diff**, because nothing can be judged on this tip until they are:
one line deleted from the generated list, `244 -> 243` in the table row, and the prose "318
decompilation proper, 318 unidentified" at `port_link_gap.md:261` corrected to 317 (same generated
number; `check_docs_claims.py` does not check that line). `python3 tools/check_docs_claims.py` now
prints **"docs claims agree with the tree"** and `link_gap.py` prints **"ok: 317 MISSING
symbol(s), all accounted for in port_link_gap_list.md"**.

**This is a documentation fix and the brief says not to file one as `NEW:`** - but it does need to
reach the driver, because it is not only this item's: **every item on `38068b13` fails `gate.sh`
until these two files agree with the tree.**

## `fn_800C8CE0`: still 74.05%, and the residue is not reachable from the source shape

Retail 0x800C8CE0, 19 insns: a **32-byte frame**, `stw r31,28(r1)` / `mr r31,r3` / `lwz r31,28(r1)`,
**three** stack cells - `8(r1)` = `*it+8`, `12(r1)` = `*it+8` again, `16(r1)` = `*it` - and the call
`r5 = r1+16` / `r6 = r1+12`. The tree's spelling (a named `last`, a named `first`, a dead
`lastCopy = last`) gives a 16-byte frame, no `r31` and two cells.

One new spelling measured this run, and it is **worse than useless**: declaring the `it + 1` result
as its own local and then copying it - `iterator tmp = *it + 1; iterator last = tmp; iterator first
= *it;` - produces **byte-identical output to the tree's body** (74.05%, same 15 instructions,
same frame). mwcceppc folds the copy away entirely, so the dead cell cannot be created that way.

Three earlier runs (`docs/goal-notes/cmorphball-wakepool-order.md`, this file's run-1 predecessor
`cmorphball-wakeeffects-outofline-resize.md`) already measured ~20 spellings between them: the
best reaches 19/19 instructions with the right frame and the right `r31` and 7-8 of 19 words
differing, **all of them scheduling order**; a local `struct` of three `iterator` members reaches
the 32-byte frame and the right store shape but zero-initialises itself (`li r8,0` + three
`stw r8`) and still has no `r31`. Per the brief this is a wall, not a `NEW:`.

WALL: fn_800C8CE0 74.05% - 32-byte frame + `mr r31,r3` + a third dead cell all at once; ~20
spellings across four runs leave only scheduling-order residue.

## `fn_800C5420`: 96.76% -> 97.84% measured, not taken - and why the last 2.16% is out of reach

This is `CActorLights::operator=` written out in this unit (111 retail insns, 111 ours). The
difference is **register allocation and instruction order only**: in all three copy loops retail
gives `src` the *higher* of the two induction registers, and the tree's spelling gives it the
lower one. The ladder, all measured here with `tools/fast_try.sh` + `tools/dol_fd.py`:

| spelling | score |
|---|---|
| the tree's body (`dst`, `src`, `first`, `end = first + count`; `dst`, `src`, `first`, `end`; `dst`, `first`, `src`, `end`) | 96.76% |
| loop 3 only: `end` declared before `src`, `end = first + count` | 96.13% |
| loop 1 only: `while (src != end) { ...; ++src; ++dst; }`, `end = base + count` | 94.86% |
| loop 1 only: `src` declared **before** `dst` | **96.98%** |
| loops 2+3: `src` first, `end = src + count` | 97.66% |
| loops 2+3: loop 2 `dst, src, end = src + count`; loop 3 `src, dst, end = src + count` | **97.84%** |
| loop 2 only: `src, end, dst` with `end` independent of `src` | 96.76% |

Two facts the ladder pins down, both new this run:

1. **mwcceppc allocates the callee-saved induction registers in *reverse* declaration order** and
   schedules the address computations in *forward* declaration order. Declaring `src` before `dst`
   therefore gets loop 1's registers exactly right (`end`=r27, `dst`=r28, `src`=r29) **and** puts
   `src`'s `addi` first where retail has `dst`'s - the two requirements are in direct conflict, so
   loop 1 cannot be fixed by reordering alone.
2. **The tie-break flips when `end` is computed *from* `src`.** With `end = first + count` (a
   separate local for the same address) allocation is reverse-declaration; with `end = src + count`
   it is forward-declaration. That is why loop 2 wants `dst, src, end` and loop 3 wants
   `src, dst, end`, and why neither reaches retail's `dst` < `end` < `src` ordering.

**I left the tree's spelling alone.** 97.84% is not a result, the judge counts matched *functions*,
and the brief says to keep the diff to what the item needs.

WALL: fn_800C5420 97.84% (tree 96.76%) - induction-register allocation flips with the `end`
expression, and forward vs reverse declaration order conflict between loops 1 and 2/3.

## Measured

```
$ ./tools/goal_check.sh build/goal/item.json
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 11959 -> 11960   linked 5728 -> 5728
  ok    check_symbol_names.py
  ok    All:  33.79% fuzzy, 26.99% matched, 12.64% linked (11960 / 28465 functions)
  flip  flip_test MetroidPrime/Player/CMorphBall.cpp: FAIL - judged below as partial progress
            build failed: mwldeppc undefined: 'CAnimRes::kDefaultCharIdx'
          FAIL  -> reverted (tree rebuilt: DOL 6ef9b491d0cc08bc81a124fdedb8bfaec34d0010)
  ok    target rose: main/MetroidPrime/Player/CMorphBall: 116 -> 117 / 158 functions
  ok    no asm added
goal_check: PARTIAL cmorphball-unclaimed-80258790-802588dc - flip_test MetroidPrime/Player/CMorphBall.cpp: FAIL, but the target rose; commit it and keep the item

$ python3 tools/report_diff.py build/goal/judge/report.base.json build/report.json
  matched  11959 -> 11960   linked 5728 -> 5728   (+1 functions at 100%, 0 units newly linked)
    +100%    main/MetroidPrime/Player/CMorphBall :: DampLinearAndAngularVelocities__10CMorphBallFfff
  no regression
```

Per function (`build/report.json`): `DampLinearAndAngularVelocities__10CMorphBallFfff` (256 B)
**57.265625 -> 100.0**, every other one of the unit's 158 unchanged (`report_diff.py`: +1, 0 worse,
0 changed). Unit `matched_functions` **116 -> 117**; `.text` fuzzy **31.264025 -> 31.428288**.

`sha1sum build/G2ME01/main.dol` = `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`, unchanged. All 86
RELs unchanged (gate.sh green). `build/gate-probe.log`: `probe: 749 files, 0 failed, 0 errors;
link: LINKED (322 undefined, 0 duplicates)` - 322 is this tip's baseline
(`build/goal/judge/undef.base.count`), unchanged by the diff: `operator*` is no longer called, the
`__amu__10CAxisAngleFRCf` it now calls is defined by `src/MetroidPrime/CAxisAngle.cpp`, and the
`CVector3f` copy is inline. `check_raw_offsets.py`: `ok: 166 raw-offset site(s) in 70 file(s)` -
this diff adds none. `unit_fit.sh`: **51 functions / 5272 bytes** present in ours but not in the
retail unit object, identical to the previous head's measurement. `check_decl_order.py --unit
main/MetroidPrime/Player/CMorphBall` still lists the unit as permuted, as it was at HEAD and as
`build/gate-order.log` already accounts for; this diff adds no declaration.

## Not filed as `NEW:`

- The flip's `undefined:` list is **still the one name** runs 1 and 2 left:
  `CAnimRes::kDefaultCharIdx` (declared `static const int` at
  `include/MetroidPrime/CAnimRes.hpp:37`, defined nowhere - there is no
  `src/MetroidPrime/CAnimRes.cpp` - not in `config/G2ME01/symbols.txt`, already in the port's
  tolerated baseline). It blocks the flip test's *link*, not any count, and the unit is at 117/158
  so the flip needs 41 more functions before the link is reached anyway.
- The two `gate.sh` documentation failures are a documentation fix, which the brief says belongs in
  these notes rather than in the queue - but see above, they block every item on this tip.
- `fn_800C8CE0` and `fn_800C5420` are measured walls inside this item's own unit, which the item
  itself covers.

## Files

- `src/MetroidPrime/Player/CMorphBall.cpp:1932-1969` - `DampLinearAndAngularVelocities` and the
  ladder behind its spelling (comment rewritten, body changed).
- `docs/research/port_link_gap_list.md` - one line: the now-provided
  `_ZN15CGMSinglePlayerC1Ev` removed, as `tools/link_gap.py` instructs.
- `docs/research/port_link_gap.md` - the generated table row `244 -> 243`, and the prose
  `318 decompilation proper, 318 unidentified` -> `317`.

No `configure.py`, no `config/`, no `splits.txt`, no `files.cmake`, no `.s`, no asm, nothing under
`tools/` or `build/goal/`; `docs/HANDOFF.md` shows as modified after `goal_check.sh`, which is the
judge rewriting its own derived counts, and it was reverted. Not committed, per the brief.

## Lane 5: passed, then failed on the moved tip (2026-10-01 19:28:31Z)

The judged change failed goal_check.sh (exit 1) once rebased onto 9f2d849f224a; re-do it against the current tip.

---

Lane 5 (fourth run on this item), 2026-10-01, HEAD `e2c47bce` (the eighth upstream sync). **Result:
`goal_check` PARTIAL** - **two** functions reached 100.00%, the unit's matched count rose
**116 -> 118 of 158**, global matched **12087 -> 12089**, every other check green, no asm.

Run 3's judged change was lost to a rebase onto the moved tip, so on this tree
`DampLinearAndAngularVelocities` was back at **57.27%** and `GetSpiderBallControllerMovement` at
**97.41%**. I re-measured everything, re-landed the first from its recorded recipe (reaching 100%
byte-for-byte again), and - the real find - **matched the second**, which runs 2 and 3 had both
recorded as a wall.

## 1. `DampLinearAndAngularVelocities`: 57.27% -> 100.00% (re-landed, re-measured)

Run 3's spelling still reaches 100% on this tip (`tools/dol_fd.py`: "64 retail insns, 64 ours,
0 differing lines"), so I re-applied it and rewrote the comment to carry the ladder. Nothing new
was learned about it, so the ladder is not repeated here; read it at the body.

## 2. `GetSpiderBallControllerMovement`: 97.41% -> 100.00% - `||`, not a wall

Retail's tail is

```
fcmpo cr0,f31,f0(-55)  /  blt  -> 0x864        (the shared fneg)
fcmpo cr0,f31,f0(145)  /  ble  -> 0x86c        (return 0.0f)
0x864: fneg f1,f1
0x868: b     -> join
0x86c: lfs f1,0.0f
```

i.e. **one** `fneg`, reached both by the `-55` branch and by falling out of the `145` test. The
tree's two-armed spelling emits two `fneg`s and differs in 3 instructions; the residue run 2
described as "mwcceppc keeps making the *first* source branch the fall-through" and run 3 wrote a
`WALL:` line about.

**The wall was an artefact of only ever trying two-armed spellings.** With **one arm carrying two
conditions** mwcceppc emits the shared tail exactly, and the function is byte-identical:

```cpp
if (angle < -55.f || angle > 145.f) {
  return -magnitude;
}
return 0.f;
```

Ladder, all measured on this body this run (81 retail insns; differing lines from
`tools/dol_fd.py`, throwaway scripts since deleted):

| spelling | differing lines |
|---|---|
| `if (<-55) return -m; else { if (>145) return -m; return 0; }` (the tree's body) | 4 |
| `if (<-55) return -m; if (>145) return -m; return 0;` | 4 |
| `if (<-55) return -m; if (<=145) return 0; return -m;` | 11 |
| `const float neg = -m; if (<-55) return neg; if (>145) return neg; return 0;` | 5 |
| `if (>= -55) { if (<=145) return 0; return -m; } return -m;` | 10 |
| `if (>145) return -m; if (<-55) return -m; return 0;` (tests swapped) | 6 |
| `float r = 0; if (<-55) r = -m; else if (>145) r = -m; return r;` | 10 |
| **`if (<-55 \|\| >145) return -m; return 0;`** | **0** |

**Codegen rule this run adds (general, and it is the one to reuse):** *mwcceppc tail-merges
identical return values **only when the source arm has one body**. Two `return -m` in two arms stay
two `fneg`s; `return -m` under one `||` is one `fneg` reached by both conditions, and the
`blt`/`ble` polarity then falls out of the arm order for free.* A duplicated `fneg`/`fmr`/`stfs`
tail in a near-miss is a hint to look for the **one-armed** spelling, not for a register-allocation
fix. I also corrected the superseded paragraph above the function, in place, per `AGENTS.md` - it
claimed the shared tail "was never true", which was only ever true of the two-armed bodies.

## 3. `ComputeMaxSpeed`: 96.84% -> 97.4% measured, not taken - and the last instruction is named

Not a wall by the brief's standard (it moved 1 instruction closer), so here is the measurement.
Retail's min tail is 7 instructions

```
lfs f0,95 / fcmpo cr0,f2,f0 / bge -> fmr f1,f0 / fmr f1,f2 / b / fmr f1,f0 / b
```

and the *only* difference from the tree is that the `95.f` constant is materialised in **f0** and
copied with `fmr f1,f0`, where mwcceppc always materialises it in the **destination** register f1
and drops the copy. 38 spellings measured across four runs; this run adds 42, and the ladder is:

| spelling | differing lines |
|---|---|
| tree: `maxSpeed = rstl::max_val(maxSpeed, 0.01f); return rstl::min_val(maxSpeed, 95.f);` | 7 |
| `return maxSpeed < 95.f ? maxSpeed : 95.f;` (= run 2's `ternary`, and the ternary with a named `const float cap`, a `float out`, a `float& ref`, a union member, a nested scope, and 4 other variants) | 6 |
| `if (maxSpeed < 95.f) return maxSpeed; return 95.f;` (run 2's `if-return`, also as a braced `if`/`else`, and with named/pointer/file-scope `const` caps) | 5 |
| `return rstl::min_val(95.f, maxSpeed);` (= run 2's `min_val_rev`; right operand order and right `bge` polarity, constant still in f1, and the `b`/`fmr` pair missing because the constant arm then needs no instruction) | 6 |
| `return maxSpeed >= 95.f ? 95.f : maxSpeed;` / `<=` forms | 10 |
| `return 95.f > maxSpeed ? maxSpeed : 95.f;` | 8 |
| `if (!(maxSpeed < 95.f)) return 95.f; return maxSpeed;` | 9 |
| `if (95.f > maxSpeed) return maxSpeed; return 95.f;` | 7 |
| `static float kMorayCap = 95.f;` (file scope, non-const) + `if/return` | 5 |
| `static const float kMorayCap = 95.f;` + `if/return`, + `min_val`, + `const float* p = &k` | 5, 6, 3 |
| 4 inlined-helper shapes (`(a<b)?a:b` and `if (a<b) return a; return b;`, literal and named-constant arguments) | 6 |
| **`volatile const float cap = 95.f; if (maxSpeed < cap) return maxSpeed; return cap;`** | **2** |

The `volatile` local is the only thing that has ever moved the constant into f0, and it gives the
right instruction count (38/38) and the right branch structure - the single remaining difference is
that a second `volatile` *read* re-loads instead of copying, so it emits `lfs f1,95` where retail
has `fmr f1,f0`. **`volatile` is a real lever in this codebase** (`src/MetroidPrime/main.cpp:1270`,
`src/MetroidPrime/CIOWinManagerRemoveIOWin.cpp:39-44`), but it buys one instruction at the price of
a second load, and 2 differing lines is not a match, so **I left the tree's body alone**.

The open question for the next run, stated so it does not have to be re-derived: *mwcceppc
rematerialises a constant into the destination register whenever the constant's only live use is
that destination, and there is no source spelling found in four runs that makes it keep the
constant in a register and copy.* The one untried route is a value the compiler cannot fold -
`extern const float` defined in another translation unit, which is `lfs f0,sym(r2)` in rodata and
therefore structurally retail's instruction. That needs a definition somewhere else, which would
put a new `.sdata2` word in a unit this item does not own; I did not do it.

## Measured

```
$ ./tools/goal_check.sh build/goal/item.json
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 12087 -> 12089   linked 5795 -> 5795
  ok    check_symbol_names.py
  ok    All:  34.22% fuzzy, 27.30% matched, 12.75% linked (12089 / 28465 functions)
  flip  flip_test MetroidPrime/Player/CMorphBall.cpp: FAIL - judged below as partial progress
            build failed: mwldeppc undefined: 'CAnimRes::kDefaultCharIdx'
          FAIL  -> reverted (tree rebuilt: DOL 6ef9b491d0cc08bc81a124fdedb8bfaec34d0010)
  ok    target rose: main/MetroidPrime/Player/CMorphBall: 116 -> 118 / 158 functions
  ok    no asm added
goal_check: PARTIAL cmorphball-unclaimed-80258790-802588dc - flip_test ...: FAIL, but the target rose;
            commit it and keep the item

$ python3 tools/report_diff.py build/goal/judge/report.base.json build/report.json
  matched  12087 -> 12089   linked 5795 -> 5795   (+2 functions at 100%, 0 units newly linked)
    +100%    main/MetroidPrime/Player/CMorphBall :: DampLinearAndAngularVelocities__10CMorphBallFfff
    +100%    main/MetroidPrime/Player/CMorphBall :: GetSpiderBallControllerMovement__10CMorphBallCFRC11CFinalInput
  no regression
```

Per function (`build/report.json`): `DampLinearAndAngularVelocities__10CMorphBallFfff` (256 B)
**57.265625 -> 100.0**, `GetSpiderBallControllerMovement__10CMorphBallCFRC11CFinalInput` (324 B)
**97.40741 -> 100.0**, every other one of the unit's 158 unchanged (`report_diff.py` over all
units: +2, 0 worse, 0 changed). Unit `matched_functions` **116 -> 118**; `.text` fuzzy
**31.264025 -> 31.440900**; `matched_code` **15756 -> 16336** of 66600 (exactly +580, the two
functions' 256 + 324).

`sha1sum build/G2ME01/main.dol` = `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`, unchanged. All 86 RELs
unchanged (gate.sh green). `build/gate-probe.log`: `probe: 747 files, 0 failed, 0 errors; link:
LINKED (291 undefined, 0 duplicates)` - **291, the baseline exactly**
(`build/goal/judge/undef.base.count` = 291): this diff adds and removes no call, so it cannot move
that count. `check_symbol_names.py`: 525 units, 0 missing. `check_raw_offsets.py`: `ok: 166
raw-offset site(s) in 70 file(s)` - this diff adds none. `unit_fit.sh`: **51 functions / 5272
bytes** present in ours but not in the retail unit object, identical to the previous head's
measurement. `check_decl_order.py --unit main/MetroidPrime/Player/CMorphBall` still lists the unit
as permuted, as it was at HEAD and as `build/gate-order.log` already accounts for; this diff adds
no function declaration, so it adds no inversion.

**Pre-existing on this tip, not mine, and not filed as `NEW:`** because it is a documentation fix
and the brief routes those to the notes: `docs/research/raw_offsets.md`'s summary line reads "165
sites in 69 files" where the tool measures **166 in 70** (the enforced per-file headings agree, and
this diff adds no site). The eighth sync did fix the `port_link_gap` staleness runs 2 and 3 hit:
`python3 tools/link_gap.py` now reads `ok: 286 MISSING symbol(s), all accounted for in
port_link_gap_list.md` and `python3 tools/check_docs_claims.py` reads "docs claims agree with the
tree" on the clean tip, so `gate.sh` is green here for the first time in three runs.

## Not filed as `NEW:`

- **The flip's `undefined:` list is still the one name** runs 1, 2 and 3 left:
  `CAnimRes::kDefaultCharIdx` (declared `static const int` at
  `include/MetroidPrime/CAnimRes.hpp:37`, defined nowhere - there is no
  `src/MetroidPrime/CAnimRes.cpp` - not in `config/G2ME01/symbols.txt`, already in the port's
  tolerated baseline). It blocks the flip test's *link*, not any count, and the unit is at 118/158
  so the flip needs 40 more functions before the link is even reached.
- `ComputeMaxSpeed` is a measured near-miss, not a wall: it moved a whole instruction closer and the
  last one is named above. It is inside this item's own unit, which the item already covers.
- `__ct__10CMorphBallFR7CPlayerfb` (96.67%, 3956 B) and `fn_800D0584` (no body, 188 B) are the two
  remaining large unlisted targets, and I measured why neither is reachable in one item. The
  constructor is **not** a register-allocation problem: 989 retail insns, 979 ours, 62 equal
  regions, and the residue is (a) our `CMorphBall` member offsets are **+16** from retail's from
  `this+0xD58` onward (`addi r3,r31,3416` vs `3424`, 3440 vs 3456, ... 3504 vs 3520) and the frame
  is 368 vs 352, (b) ~16 `addi r5,r5,N` string-literal addresses differ (421 vs 442, 646 vs 630,
  666 vs 655, ...) - our `.rodata` string pool is not where retail's is, and (c) three structural
  blocks where we call `gpSimplePool->GetObj` with a literal null and `-1` where retail reloads a
  GOT word. (a) alone is a `CMorphBall` layout change that would move offsets in 117 already-matched
  functions of this unit, so it is not this item's work. `fn_800D0584` is the unit's static
  initialiser: it needs `lbl_803B84B8` (a 5-word table), two `.sdata2` words at
  `r13-27876`/`r13-27872`, a 16-byte type descriptor at `0x8040D558`, and
  `__register_global_object` (0x80344E20, a crt symbol no unit defines). Writing it is a
  `port`-shaped risk, and run 3 declined it for the same reason.

## Files

- `src/MetroidPrime/Player/CMorphBall.cpp:1932-1967` - `DampLinearAndAngularVelocities` and the comment
  above it, re-landed; the ladder is run 3's.
- `src/MetroidPrime/Player/CMorphBall.cpp:2257-2265` - the superseded paragraph above
  `GetSpiderBallControllerMovement`, corrected in place.
- `src/MetroidPrime/Player/CMorphBall.cpp:2285-2310` - the `||` body and its 8-row ladder.

No `configure.py`, no `config/`, no `splits.txt`, no `files.cmake`, no `.s`, no asm, nothing under
`tools/` or `build/goal/`; the throwaway spellings scripts lived in `.tmp/opencode/` and are
deleted. `docs/HANDOFF.md` shows as modified after `goal_check.sh`, which is the judge rewriting its
own derived counts, and it was reverted. Not committed, per the brief.
