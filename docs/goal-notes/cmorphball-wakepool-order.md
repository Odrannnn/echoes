# cmorphball-wakepool-order (match, `MetroidPrime/Player/CMorphBall`)

Lane 6, 2026-10-01. **Result: `goal_check` PARTIAL** - the unit's matched count rose
**105 -> 107 of 158**, every other check green. The flip still fails, on the unit's
**unwritten** bodies, unchanged by this diff.

## The item as filed is mostly stale, and re-measuring it is what found the work

The `NEW:` line that queued this said the unit's `.rodata` string pool is out of order and
pins four functions at 99.73-99.99%. **At HEAD that is no longer true**, and the fix is
already in the tree: `src/MetroidPrime/Player/CMorphBall.cpp:61-68` hoists
`kNoModelName` / `kMultiplayerBallModelName` to the top of the TU precisely to get the pool
offsets right. Measured, per function, by disassembling both objects and extracting every
`lis rX,0` / `addi rX,rX,0` / `addi rY,rX,<off>` string materialisation (throwaway script,
`tools/dol_fd.py`-style, deleted; the command is in "Reproduce" below):

| pool offset | string | referenced by (retail) | referenced by (ours) |
|---|---|---|---|
| 0x0C2 | `""` | `fn_800D0584` (188 B, **no body in ours**) | - |
| 0x18F | `Locomotion` | `UpdateScrewAttackRecovery` (1328 B, scaffold in ours) | - |

**That is the whole list.** Retail's 158 functions contain exactly two string references
between them, both in functions this tree has not written, and our object contains **zero**
string references. So the pool order is now correct for every function that exists in our
object, and `CreateBallShadow`, `UpdateIceBreakEffect` and
`UpdateMorphBallTransitionFlash` (three of the four the item named) are already at 100% at
HEAD. `BallLight` at 0x19A and `TXTR_BallFade` at 0x181 are in the pool but referenced by no
function in this unit at all, so they are not this unit's to account for.

What was left was two functions a hair under 100% for reasons that had nothing to do with the
pool. Both are fixed below.

## 1. `GetMorphBallModel` 99.9375% -> 100.00% - and it was returning null for every real name

`src/MetroidPrime/Player/CMorphBall.cpp:1002-1014`. The function was spelled
`if (!rstl_string_eq_c(name, "")) { return nullptr; }`. Retail's `beq` at 0x800C12E0 is taken
when `rstl_string_eq_c` returns **false**, i.e. retail branches *into* the body and the
**empty** name is what returns null. Ours had `bne` - the one differing instruction out of 80 -
and the inverted semantics with it: the constructor asks for `kNoModelName` when it wants no
model (`mSpiderBallGlassModel`, 0x800C0758) and passes "SamusBallCMDL",
"SamusBallLowPolyCMDL", "SamusBallFrozenCMDL" and the eleven table entries when it wants a
model, so as written the function returned null for all of those and built a model from `""`.

The spelling is the same codegen rule three other bodies in this file already established
(`fn_800C084C`, `GetGravityAcceleration`, `IsMovementAllowed`): **mwcceppc branches to the
continuation when the `if` condition is false**, so to get retail's `beq` over the
`return nullptr` the source condition has to be the equality, not its negation. Writing the
negation back measures 99.9375% with a `bne`; the equality measures 100.00% with a `beq`,
every other instruction and the frame unchanged.

**This also corrects a wrong claim in the tree.** The comment above `rstl_string_eq_c` said
the last 0.0625% was "one relocation out of 160" - the call site's `R_PPC_REL24` naming
`rstl_string_eq_c` where retail names `__eq__4rstlF...` - and that it was unreachable from
C++. It was the branch: **objdiff normalises a `R_PPC_REL24` target**, so the renamed symbol
costs nothing and the function is 100.00% with the reloc still named `rstl_string_eq_c`. The
comment is corrected in place (`:120-138`) so nobody spends a run trying to rename the symbol.
The rename is still not expressible (MWCC rejects both `asm("...")` labels on a function and
namespace-scope `__asm__`, measured), it is just not needed.

## 2. `InitializeWakeEffects` 99.7368% -> 100.00% - one register swap

`src/MetroidPrime/Player/CMorphBall.cpp:849-861`. The whole 532-byte function differed from
retail in **seven instructions, all the same r18/r19 swap**, and nothing else: the frame, both
local arrays, all six pool-relative loads and every `bl` matched.

```
retail   addi r18,r5,0  ; li r19,0 ; stw r19,32(r18) ; stw r20,36(r18) ; ...
ours     addi r19,r5,0  ; li r18,0 ; stw r18,32(r19) ; stw r20,36(r19) ; ...
```

Retail copies the vector's address into its own register once and puts the value 0 in a
second one. Four separate `operator[]` calls give the constant the lower register instead.
Hoisting one pointer - `EWakeEffectIndex* const indices = sWakeEffectForMaterial.data();` and
four `indices[kMT_*] = ...` - reverses the assignment and the function becomes **0 differing
lines** (`tools/dol_fd.py MetroidPrime/Player/CMorphBall InitializeWakeEffects`: "133 retail
insns, 133 ours, 0 differing lines"). One spelling, no search: the two candidates were
`operator[]` and a hoisted `data()`, and the hoisted one is the one that reproduces the bytes.

## Measured

```
$ ./tools/goal_check.sh build/goal/item.json
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 11456 -> 11458   linked 5587 -> 5587
  ok    check_symbol_names.py
  ok    All:  32.85% fuzzy, 25.72% matched, 12.17% linked (11458 / 28465 functions)
  flip  flip_test MetroidPrime/Player/CMorphBall.cpp: FAIL - judged below as partial progress
            build failed: mwldeppc undefined: 'fn_800CD4B8'
                                         'CAnimRes::kDefaultCharIdx'
  ok    target rose: main/MetroidPrime/Player/CMorphBall: 105 -> 107 / 158 functions
  ok    no asm added
goal_check: PARTIAL cmorphball-wakepool-order - flip_test ...: FAIL, but the target rose;
commit it and keep the item

$ python3 tools/report_diff.py <report at HEAD> build/report.json
  matched 11456 -> 11458  linked 5587 -> 5587  (+2 functions at 100%, 0 units newly linked)
    +100%  main/MetroidPrime/Player/CMorphBall :: GetMorphBallModel__10CMorphBallFRCQ24rstl...
    +100%  main/MetroidPrime/Player/CMorphBall :: InitializeWakeEffects__10CMorphBallFv
  no regression
```

Unit `matched_code` **13436 -> 14288** of 66600, `.text` fuzzy **28.954535 -> 28.956938**.
Per function, `build/report.json`: `GetMorphBallModel` (320 B) **99.9375 -> 100.0**,
`InitializeWakeEffects` (532 B) **99.73684 -> 100.0**, every other function in the unit and
every other unit unchanged (`report_diff.py` over all 2066 units: +2, 0 worse, 0 changed).

`sha1sum build/G2ME01/main.dol` = `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`; all 86 RELs
(`hashes vs config.yml ok`); `build/gate-probe.log`: `probe: 752 files, 0 failed, 0 errors;
link: LINKED (250 undefined, 0 duplicates)` - 250, unchanged from the judge baseline, so
this diff adds no undefined symbol. `check_symbol_names.py`: 514 units, 0 missing.
`unit_fit.sh`: **51** functions present in ours but not in the retail unit object, 5272
bytes - identical before and after this diff, since both hunks touch existing functions and
add no symbol. `check_docs_claims.py`: ok (via gate.sh). `docs/HANDOFF.md` shows as modified
after `goal_check.sh`; that is `MP_GATE_DOCS_WRITE=1` rewriting the derived counts, and it
was reverted.

### Reproduce

```sh
export MP_TOOLCHAIN_DIR=/run/media/odran/Leo/projects/Restored-projects/Chatgpt/MetroidPrimePort
python3 tools/dol_fd.py MetroidPrime/Player/CMorphBall GetMorphBallModel InitializeWakeEffects
python3 tools/dol_read.py 0x803A86F0 0x2F0        # retail's pool, pool offset 0x18F = Locomotion
./tools/decomp_build.sh >/dev/null && python3 tools/report_diff.py <report at HEAD> build/report.json
```

## Still open in this unit, measured not guessed

- **The flip is stopped by unwritten bodies, not by anything in this diff.** `flip_test.sh`
  fails to link with `undefined: 'fn_800CD4B8'` and `'CAnimRes::kDefaultCharIdx'`. Note the
  pair is **not** the one the previous run recorded (`CElementGen::GetEmitterTime() const` and
  `fn_800CD4B8`): `GetEmitterTime` is defined at `:118` now, so `CAnimRes::kDefaultCharIdx` -
  a static data member, not a function - is the new one in the list.
- **10 of the unit's 158 functions still have no body** (`fuzzy_match_percent: None`):
  `fn_800D0584` (188), `fn_800CD4B8` (152), `fn_800CD460` (88), `fn_800CD35C` (260),
  `fn_800CD244` (280), `fn_800C5420` (444), `fn_800C9380` (48), `fn_800C93B0` (72),
  `fn_800C88C0` (92), `fn_800C33DC` (92). A carve is the only route and that is four files.
- `check_decl_order.py --unit main/MetroidPrime/Player/CMorphBall` still reports the unit as
  out of retail order, identical at HEAD and already listed in
  `docs/research/decl_order.md:101`. That is the next wall after the unwritten bytes.
- Walls carried forward from earlier runs, **not retried here** (they belong to other items
  and are recorded there): `ComputeMaxSpeed` 96.84%, `GetRenderBounds` 96.48%,
  `fn_800C8CE0` 74.05%, `GetSpiderBallControllerMovement` 94.51%,
  `DampLinearAndAngularVelocities` 57.27%.

No `NEW:` line from this run: the pool order it was queued for is already correct, and what
is left in this unit (the unwritten bodies, then the declaration order) is already covered by
the queued `cmorphball-wakeeffects-carve-74-unwritten` and `docs/research/decl_order.md`.
Filing a third item for the same target would cost a lane an hour to re-derive this.

## Files

- `src/MetroidPrime/Player/CMorphBall.cpp`
  - `:120-138` - the `rstl_string_eq_c` comment: the residual 0.0625% was the branch, not the
    relocation, and objdiff normalises a `R_PPC_REL24` target
  - `:849-861` - `InitializeWakeEffects`' four writes through one hoisted `data()`
  - `:1002-1014` - `GetMorphBallModel`'s empty-name test, and the measurement behind it

Not committed, per the brief.

---

# Run 2 (lane 6, 2026-10-01) - the two light-copy functions

## Result: `goal_check` PARTIAL - the unit's matched count rose **109 -> 111 of 158**

Every other check green, no asm, no judge-owned path touched. Run 1 above is still accurate: the
`.rodata` pool order this item was filed for **is** correct at HEAD (three green checks had already
been spent on it), and the flip still fails on unwritten bodies. So this run ignored the pool,
re-measured the unit, and wrote two of the eight functions that still had **no body in our object**.

Re-measured on the clean tree first: **109/158** matched, 14760/66600 bytes, `.text` fuzzy
29.343304, **eight** functions with `fuzzy_match_percent: None`. Four of them are already recorded
elsewhere and were not retried (`fn_800C88C0`/`fn_800C33DC`, the vtable destructors - three
spellings at 53.91% each in `cmorphball-wakeeffects-carve-74-unwritten`; `fn_800CD460`/`fn_800CD4B8`,
landed by `cmorphball-unclaimed-vtables-and-8033d2f4`). The other four were left for the reasons
measured under "Not done" below.

## What I changed

Three files. No `configure.py`, no `config/`, no `splits.txt`, no `files.cmake`, no `.s`, no asm,
nothing under `tools/` or `build/goal/`. `docs/HANDOFF.md` shows as modified after `goal_check.sh`;
that is the judge rewriting its own derived counts, and it was reverted.

1. **`src/MetroidPrime/Player/CMorphBall.cpp:492-531`** - `fn_800C93B0` (0x800C93B0, 72 B) and
   `fn_800C9380` (0x800C9380, 48 B) written out under their retail names, with the two
   unclaimed-range callees they call declared by name. **Both 0% -> 100.00% on the first
   spelling.** No search was needed.
2. **`src/MetroidPrime/PortGlobals.cpp:103, 1086-1122`** - host definitions of
   `fn_80045E18` and `__as__6CLightFRC6CLight`, the two functions `fn_800C93B0` calls, beside the
   `fn_8033D2F4` block that the previous `CMorphBall` head added for exactly the same reason, plus
   `#include <string.h>` for `memcpy`.
3. **`docs/research/raw_offsets.md`** - `CMorphBall.cpp`'s heading `3 sites` -> `4 sites` with the
   `+0x50` entry (`tools/check_raw_offsets.py` fails a source edit that adds a raw offset without
   it), and the summary total `164` -> `165`, quoted from the tool.

## The measurement that says what these two functions *are*

They are not `CMorphBall` members: both take the destination in `r3` and the source in `r4`, and
every call site agrees - `CMorphBall::UpdateBallLight` passes `r3 = r1+484` (a stack local it has
just built) and `r4` = the light it read out of the `CActorLights` in the state manager (two
`bl fn_800C9380` at 0x800C4CEC and 0x800C4D38), and the four sites in
`auto_03_8021FABC_text.o` (`fn_8021FBCC`, unclaimed range) all pass a stack local as `r3` and a
`lis`-materialised address as `r4`. Found with `objdump -r` over every object in
`build/G2ME01/obj`: **six call sites total, and `fn_800C93B0` has exactly one - the wrapper.**

`fn_800C9380` is the wrapper: `mr r31,r3`, `bl fn_800C93B0` **without touching `r4`** (the source
stays in the register the caller put it in), then `mr r3,r31` from one exit. That last `mr` is why
the return type is `void*`: written `void`, the store back drops out of the epilogue - the same
"one exit returning the object" rule the four `fn_800CEFxx` links above already record.

`fn_800C93B0` branches on a **byte at +0x50** and the two branches are **two different copies**,
which is the only reason it is written out rather than inlined:

| branch | callee | what retail's callee does |
| --- | --- | --- |
| flag clear | `fn_80045E18` (0x80045E18, 21 insns) | ten `lfd`/`stfd` pairs - **all 0x50 bytes**, flag word included; then `li r0,1` / `stb r0,80(r31)` sets the flag |
| flag set | `__as__6CLightFRC6CLight` (0x80046384, retail's **weak** `CLight` copy-assign, 41 insns) | 0x4C bytes plus the byte at +0x4C = **0x4D bytes**, stopping short of +0x50, so it leaves the flag alone |

The source is the ordinary `if/else` and it reproduces retail's branch shape as written - `cmplwi`
then **`bne` into the `else`** - with no inversion trick:

```cpp
unsigned char* inScene = reinterpret_cast< unsigned char* >(self) + 0x50;
if (*inScene == 0) { fn_80045E18(self, src); *inScene = 1; } else { __as__6CLightFRC6CLight(self, src); }
```

## Both callees are real DOL functions, and both needed a host definition to keep the gate green

`nm --defined-only` over `build/G2ME01/obj`: **one** object defines either of them,
`auto_03_80045CDC_text.o`, and `build.ninja` links it into `main.elf` - so the **DOL needs nothing
from us** and `main.dol`'s sha1 is unchanged. The **host** link does need them, and only because a
body now calls them: without definitions `tools/link_gap.py` measures 247 MISSING where
`docs/research/port_link_gap_list.md` documents 245 and the gate fails. `PortGlobals.cpp` is the
repo's existing home for exactly this case (it is deliberately not a `configure.py` unit, so it
cannot perturb a Matching unit's small-data offsets - see its own header).

The definitions are the bulk copies retail performs and nothing else: `memcpy(dst, src, 0x50)` and
`memcpy(dst, src, 0x4D)`. No arithmetic, no branches, no allocation, no invented behaviour. They
are reachable only from `fn_800C93B0` <- `fn_800C9380` <- `UpdateBallLight`, which is still a
scaffold, so the port's boot does not run them; they exist so the linker has a definition for the
reference the new DOL code makes. **Measured effect on the link gap: 249 undefined before and after**
(`build/gate-probe.log`: `LINKED (249 undefined, 0 duplicates)`), i.e. the two references are closed
by the two definitions exactly.

## Measured

```
$ ./tools/goal_check.sh build/goal/item.json
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 11542 -> 11544   linked 5625 -> 5625
  ok    check_symbol_names.py
  ok    All:  33.11% fuzzy, 25.95% matched, 12.24% linked (11544 / 28465 functions)
  flip  flip_test MetroidPrime/Player/CMorphBall.cpp: FAIL - judged below as partial progress
            build failed: mwldeppc undefined: 'CAnimRes::kDefaultCharIdx', 'fn_800C88C0'
                                       (the log lists all five; the summary prints the first two)
  ok    target rose: main/MetroidPrime/Player/CMorphBall: 109 -> 111 / 158 functions
  ok    no asm added
goal_check: PARTIAL cmorphball-wakepool-order - flip_test ...: FAIL, but the target rose; commit it
and keep the item

$ python3 tools/report_diff.py build/goal/judge/report.base.json build/report.json
  matched  11542 -> 11544   linked 5625 -> 5625   (+2 functions at 100%, 0 units newly linked)
    +100%    main/MetroidPrime/Player/CMorphBall :: fn_800C9380
    +100%    main/MetroidPrime/Player/CMorphBall :: fn_800C93B0
  no regression
```

Per function (`build/report.json`): `fn_800C9380` (48 B) and `fn_800C93B0` (72 B) **`None` ->
100.0**; **every other one of the 158 unchanged** (diffed the whole list against
`build/goal/judge/report.base.json`: 2 changed, both to 100.0, 0 worse). Unit `matched_code`
**14760 -> 14880** of 66600 (exactly 48 + 72), `.text` fuzzy **29.343304 -> 29.523483**,
`matched_functions` **109 -> 111**. Functions with no body in our object: **eight -> six**.

`sha1sum build/G2ME01/main.dol` = `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`, unchanged. All 86
RELs unchanged (`hashes vs config.yml ok`). `build/gate-probe.log`: `probe: 754 files, 0 failed, 0
errors; link: LINKED (249 undefined, 0 duplicates)` - **249, unchanged**. `check_symbol_names.py`:
515 units, 0 missing. `python3 tools/check_raw_offsets.py`: `ok: 165 raw-offset site(s) in 69
file(s)`. `config/G2ME01/splits.txt` untouched, so its 804 blocks and 28465 `total_functions` are
as they were. `tools/unit_fit.sh`: the "present in ours but not in the retail unit object" list is
**51 functions / 5272 bytes**, identical to the previous head's measurement - both new functions
are symbols the retail object has, so nothing was added.

**The bytes, not just the score.** Ours and retail agree instruction for instruction; the only
differences are the `bl` displacements, which are the two relocations, and they name retail's
symbols exactly:

```
$ build/binutils/powerpc-eabi-objdump -r build/G2ME01/src/MetroidPrime/Player/CMorphBall.o
000054a8 R_PPC_REL24       fn_80045E18
000054b8 R_PPC_REL24       __as__6CLightFRC6CLight
```

**The flip's `undefined:` list is 5 names, down from 6** - `fn_800C9380` is gone, and it was the
one the two vtable destructors and `fn_800CD244`/`fn_800CD35C` reach through `UpdateBallLight`:
`CAnimRes::kDefaultCharIdx`, `fn_800C33DC`, `fn_800C88C0`, `fn_800CD244`, `fn_800CD35C`.

## Not done, and why - the six bodies still missing, all measured

- **`fn_800CD244` (0x800CD244, 280 B = 70 insns)** and **`fn_800CD35C` (0x800CD35C, 260 B = 65
  insns)** - the two largest remaining and both blocked on **unclaimed-range callees**:
  `fn_80258790` (0x80258790, 0x14C = 83 insns) and `fn_802588DC` (0x802588DC, 0x94 = 37 insns),
  which `nm` finds **only** in `auto_03_80257AF8_text.o`. Writing either body adds two more host
  references, so both callees would need host transcriptions too - 120 instructions of
  unknown-semantics allocator/vector-buffer code before the first byte of the target function can
  be scored. That is why it is filed as a `NEW:` item below and not attempted here.
- **`fn_800D0584` (0x800D0584, 188 B)** - a global static initialiser, not logic: it writes byte
  patterns (`255,255,192,255` and `170,84,255,255`) into `.sdata2` words at `_SDA2_BASE_`-relative
  offsets, copies four `.data` words, and ends in `stwu r0,-10904(r3)` / `bl
  __register_global_object`. Reproducing it means reproducing an unclaimed `.sbss` global's layout
  and a runtime registration call, both outside this unit.
- **`fn_800C5420` (0x800C5420, 444 B)** and the two vtable destructors `fn_800C88C0`/`fn_800C33DC`
  (92 B each) - carried forward from `cmorphball-wakeeffects-carve-74-unwritten` and
  `cmorphball-unclaimed-vtables-and-8033d2f4`, not re-measured here. The destructors are still
  worth **zero** to the judge whatever they score, because a sub-100% function adds nothing to
  `matched_functions` or `matched_code` (both count only exactly-100.0 functions).
- `check_decl_order.py --unit main/MetroidPrime/Player/CMorphBall` still reports the unit permuted,
  as it did at HEAD and as `docs/research/decl_order.md:101` already lists. Unchanged.

No `WALL:` line from this run: nothing I tried failed - both functions reached 100.00% on the
first spelling, so a wall line would be a claim about spellings I did not measure. The walls from
earlier runs (`ComputeMaxSpeed` 96.84%, `GetRenderBounds` 96.48%, `fn_800C8CE0` 74.05%,
`GetSpiderBallControllerMovement` 94.51%, `DampLinearAndAngularVelocities` 57.27%) are **not
retried**, and their spellings stay in the other items' notes.

## Codegen rules this run adds

- **A `void*` wrapper whose only job is to call another function of the same pair wants its
  argument in `r31` and its own argument back in `r3`.** `return self;` is what emits retail's
  closing `mr r3,r31`; `void` drops it and the function scores less than 100.
- **Two calls to two different functions of the same name family, in an `if/else`, come out in
  retail's order with the ordinary spelling** - no inverted condition needed, unlike
  `GetMorphBallModel` in run 1 above. The difference is that here the `else` is the *short* branch:
  retail's `bne` jumps forward over the registering copy into the two-instruction one, which is
  exactly what MWCC emits for `if (flag == 0) { A; flag = 1; } else { B; }`.

NEW: cmorphball-unclaimed-80258790-802588dc | match | MetroidPrime/Player/CMorphBall |
`fn_800CD244` (0x800CD244, 280 B = 70 insns) and `fn_800CD35C` (0x800CD35C, 260 B = 65 insns) are the
two largest of the unit's six remaining unwritten bodies and both call two functions that live in
an **unclaimed range**: `fn_80258790` (0x80258790, 0x14C) and `fn_802588DC` (0x802588DC, 0x94),
which `nm` finds only in `auto_03_80257AF8_text.o`. With host definitions of those two added to
`PortGlobals.cpp` beside `fn_8033D2F4` - the arrangement this run and the previous head used to
close the link gap for `fn_80045E18` - both bodies become writable, they are two of the five names
in the flip test's `undefined:` list, and they are worth +2 to `matched_functions`. Both
disassemblies are in `build/flip-ninja.log`'s symbol set and `./tools/dis.sh 0x800CD244 0x118`;
`fn_800CD244`'s body is a stack-popping loop over a `CPlane` (`__ct__6CPlaneFRC9CVector3fRC9CVector3f`
is already in the tree, retail 0x802F7868) with a flag byte at +0x18 and three counters at
+0x20/+0x24/+0x28/+0x2C, so it is transcription work rather than investigation.

## Files

- `src/MetroidPrime/Player/CMorphBall.cpp:492-531` - `fn_800C9380`, `fn_800C93B0`, and the two
  declarations for the unclaimed-range copies they call.
- `src/MetroidPrime/PortGlobals.cpp:103` (`#include <string.h>`), `:1086-1122` - host
  `fn_80045E18` / `__as__6CLightFRC6CLight`.
- `docs/research/raw_offsets.md` - `CMorphBall.cpp` heading `3 sites` -> `4 sites` with the
  `+0x50` entry, and the summary total 164 -> 165.

Not committed, per the brief.

---

# Run 3 (lane 5, 2026-10-01) - the allocator flag byte is a bit-field struct

## Result: `goal_check` PARTIAL - the unit's matched count rose **114 -> 115 of 158**

Every other check green, no asm, no judge-owned path touched. Run 1's and run 2's conclusions both
still hold: the `.rodata` pool order this item was filed for is correct at HEAD, and the flip still
fails on bodies this unit has not written. This run ignored the pool, re-measured, and fixed the
one function whose remaining diff was a **declared-type** question rather than an algorithm one.

Re-measured on the clean tree first: **114/158** matched, 15344/66600 bytes, `.text` fuzzy
30.406366, **two** functions with no body (`fn_800C5420` 444 B, `fn_800D0584` 188 B - both carried
forward from other items and not retried). 42 sub-100%, of which 36 are scaffolds under 3% and
cannot be reached this way. Of the six that were real code, four are recorded walls in other
items' notes and were not retried; the two left were `fn_800CD4B8` (96.32%) and `fn_800C8CE0`
(74.05%). **This run fixed `fn_800CD4B8` and did not fix `fn_800C8CE0` - see "Not done".**

## 1. `fn_800CD4B8` 96.32% -> 100.00% - it is a bit-field struct, and the tree said it could not be

The one file changed. `src/MetroidPrime/Player/CMorphBall.cpp:460-520`.

The existing comment above this body stated the opposite, and stated it with a number:

> `MWCC` reaches this shape from arithmetic on a `uchar` member rather than from a C++ bit-field -
> a bit-field declaration emits a `stw`-and-mask pair instead of the single-byte rotates retail
> has, measured at 91.58% against this body's 93.03%

**That is wrong, and it is what kept this function at 96% through two previous runs.** Declaring
the byte as a struct of bit-fields reproduces retail's bytes exactly. I found it by compiling
candidate declarations with the unit's own `mwcceppc` 2.7 and comparing emitted words against
retail's, which is the same method `tools/probe_cntlzw_versions.py` uses; a throwaway harness
(flags read out of `build.ninja` so the comparison is against the compiler the build actually
uses) was deleted afterwards.

The whole diff was **three instructions**, all of them the three flag operations:

| retail | ours (the `uchar` spelling that was in the tree) |
|---|---|
| `rlwinm. r0,r0,27,31,31` | `rlwinm. r0,r0,0,27,27` |
| `rlwimi r4,r0,2,28,29` | `rlwimi r4,r0,4,26,27` |
| `rlwimi r3,r3,1,26,26` + `stb r3,0(r30)` | `ori r3,r0,64` + `stb r0,0(r30)` |

The `& 0x10` test is the difference between SH=27 (rotate into the sign bit) and SH=0 (a masked
`andi.`); the count's 2-bit field sits at SH=30/MB=30/ME=31 rather than SH=30/MB=26/ME=27; and
the final set is a `rlwimi` from the register itself rather than an `ori`.

### The byte layout, each bit measured rather than assumed

Sweeping the field offsets and reading off the emitted SH/MB/ME triple:

- **bit 2, 1-bit - the `if` test.** A 1-bit field at byte bit 2 emits `5400dfff`, **retail's exact
  word**. Measured in the same sweep: bit 0 -> `5400cfff`, bit 1 -> `5400d7ff`, bit 2 -> `5400dfff`,
  bit 3 -> `5400e7ff`, bit 6 -> `5400ffff`, bit 7 -> `540007ff`. So this is a positive
  identification, not a guess.
- **bits 4..5, 2-bit - the count.** A 2-bit field emits `rlwinm SH=26+off MB=30 ME=31` to read and
  `rlwimi SH=(6-off) MB=24+off ME=25+off` to write. Sweeping off=0..6, **off=4 is the only one
  that produces retail's `SH=30,MB=30,ME=31` and `SH=2,MB=28,ME=29`** (off=0 gives 26/24, off=2
  gives 28/26, off=6 gives 0/30). `f->mCount = (f->mCount - 1) & 3;` is the whole expression; the
  `& 3` is redundant and makes no difference to the bytes.
- **bit 3, 1-bit - the source of the last `rlwimi`.** Retail's is `rlwimi r3,r3,1,26,26` with
  **src == dest**, which is the part that rules out the obvious spelling. Measured: `f->b2 = 1`
  emits SH=5 **with a separate `li r0,1`**; copying from the byte's own bits gives SH=30 (from
  bit 0), 31 (bit 1), **1 (bit 3)**, 4 (bit 6), 5 (bit 7). Only `f->b2 = f->b3;` gives SH=1 with
  rA == rS, which is retail's word. So bit 2 is refreshed from bit 3, not set to a literal.

Bits 0, 1, 6 and 7 are declared as unnamed padding so the three real fields land where retail puts
them. The comment records that the field *names* describe what the code does, not what retail
called the byte - nothing outside this function establishes what it means.

**After this the function is 38/38 instructions byte-identical to retail except the three `bl`
displacements**, which are the three `R_PPC_REL24` relocations (`CMemory::Free` twice,
`fn_8033D2F4` once) and which objdiff normalises. That was verified in the probe object before the
edit, so the 100.00% is the compiler's word and not an objdiff rounding artefact.

`fn_800CD460`, which calls into this one, stayed at **0 differing lines** (`python3 tools/dol_fd.py
MetroidPrime/Player/CMorphBall fn_800CD460`: "22 retail insns, 22 ours, 0 differing lines") and
remains 100.0%.

## Measured

```
$ ./tools/goal_check.sh build/goal/item.json
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 11946 -> 11947   linked 5728 -> 5728
  ok    check_symbol_names.py
  ok    All:  33.76% fuzzy, 26.94% matched, 12.64% linked (11947 / 28465 functions)
  flip  flip_test MetroidPrime/Player/CMorphBall.cpp: FAIL - judged below as partial progress
            build failed:
              ### mwldeppc.exe Linker Error:
              #   undefined: 'CAnimRes::kDefaultCharIdx'
  ok    target rose: main/MetroidPrime/Player/CMorphBall: 114 -> 115 / 158 functions
  ok    no asm added
goal_check: PARTIAL cmorphball-wakepool-order - flip_test ...: FAIL, but the target rose; commit it and keep the item

$ python3 tools/report_diff.py build/goal/judge/report.base.json build/report.json
  matched  11946 -> 11947   linked 5728 -> 5728   (+1 functions at 100%, 0 units newly linked)
    +100%    main/MetroidPrime/Player/CMorphBall :: fn_800CD4B8
  no regression
```

Per function (`build/report.json`): `fn_800CD4B8` (152 B) **96.31579 -> 100.0**; **every other one
of the 158 unchanged** (`report_diff.py` over the whole report: +1 at 100%, 0 worse, 0 changed).
Unit `matched_code` **15344 -> 15496**, `.text` fuzzy **30.406366 -> 30.414774**,
`matched_functions` **114 -> 115**.

`sha1sum build/G2ME01/main.dol` = `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`, unchanged. All 86
RELs unchanged (`hashes vs config.yml ok`). `python3 tools/check_raw_offsets.py`: `ok: 166
raw-offset site(s) in 70 file(s)` - unchanged by this diff, which adds no raw offset.
`tools/unit_fit.sh`: the "present in ours but not in the retail unit object" list is **51 functions
/ 5272 bytes**, identical to the previous head. `config/G2ME01/splits.txt` untouched.
`docs/HANDOFF.md` shows as modified after `goal_check.sh`; that is the judge rewriting its own
derived counts, and it was reverted - the only file this diff changes is `CMorphBall.cpp`.

## Not done, and why

- **`fn_800C8CE0` (0x800C8CE0, 76 B = 19 insns), 74.05% -> still 74.05%.** Retail needs a **32-byte
  frame with `r31` saved** (`stwu r1,-0x20(r1)` / `stw r31,0x1c(r1)` / `mr r31,r3` / `lwz r31,0x1c(r1)`)
  and **three** stack slots at 0x8, 0xc and 0x10 holding `last`, a second copy of `last`, and
  `first`. The tree's spelling gives a 16-byte frame with no `r31` and only two slots, so it can
  never match. What forces `r31` in retail is that `out` (in `r3`) is **live across the call**
  while retail emits **no work after the call** - the epilogue is `bl; lwz r0; lwz r31; mtlr;
  addi; blr`. Nine spellings were compiled and measured (named locals in three orders, an
  address-of indirection, a dead slot kept alive by a `(void)`, a dead post-call store, a
  conditional post-call use, and volatile-sink variants); the best reached **19/19 instructions
  with the right frame and the right `r31`, 7 of 19 words differing, all of them scheduling** -
  the same three loads and the same three stores in a different order, e.g. retail
  `addi r6,r6,8` before `lwz r0,0(r5)` where the probe emits them the other way round, and retail
  storing 0x8/0xc/0x10 in that order where the probe stores 0xc/0x8. No spelling found produces
  retail's *order* while keeping its *shape*. This is register-scheduling residue, so per the
  brief's wall rule it is recorded as a wall rather than iterated further.
- **The flip is still blocked by bodies this unit does not have.** `flip_test.sh` now reports a
  **single** undefined name, `CAnimRes::kDefaultCharIdx` - **down from the five** run 2 recorded
  (`CAnimRes::kDefaultCharIdx`, `fn_800C33DC`, `fn_800C88C0`, `fn_800CD244`, `fn_800CD35C`). The
  four function names are gone because the previous item
  (`cmorphball-unclaimed-80258790-802588dc`, commit 9210ad93) wrote those bodies. What is left is
  a **static data member, not a function** - the same class of thing as `fn_8033D2F4` was.
- **Two bodies still unwritten** (`fuzzy_match_percent: None`): `fn_800C5420` (444 B) and
  `fn_800D0584` (188 B, a global static initialiser). Both are carried forward from other items and
  were not retried here.
- **`check_decl_order.py --unit main/MetroidPrime/Player/CMorphBall`** still reports the unit
  permuted, unchanged at HEAD and already listed at `docs/research/decl_order.md:101`.

No `NEW:` line from this run: everything left in this unit is either covered by a queued item or
is `fn_800C8CE0`'s scheduling residue, which is a wall and not work whose success raises a count
by a new route.

## Codegen rules this run adds

- **The three flag idioms of this file are bit-field idioms, and `uchar` arithmetic cannot reach
  them.** A 1-bit field's *test* is `rlwinm. rX,rX,SH,MB=31,ME=31` with `SH` selecting the bit, so
  the test word identifies the bit position exactly - and a mask on the byte does **not** produce
  that word (`(x & 0x10)` emits SH=28, `(x & 0x40)` emits SH=26; the field at bit 2 emits SH=27).
  This is the same rule `SMorphBallPathFlags` below already records for its own fields, now
  measured on a second struct in the same unit.
- **A 2-bit field's position is identified by two numbers at once** - the read's
  `SH=26+off,MB=30,ME=31` and the write's `MB=24+off,ME=25+off` - and exactly one offset matches a
  given retail pair. Sweeping the offset is cheap and decisive; guessing it from the byte's
  arithmetic is what the previous two runs did.
- **`rlwimi` with `src == dest` is a field copy, never a literal store.** A literal `1` emits a
  separate `li` plus SH=5; a copy from a neighbouring bit emits SH set by the distance between the
  two fields, and SH=1 with rA == rS means "copy from the immediately adjacent field".
- **When a probe disagrees with a comment that quotes a percentage, believe the probe.** The
  comment's claim that a bit-field "emits a `stw`-and-mask pair" is not what mwcceppc 2.7 does for
  this shape; compiling the candidate and comparing words settles it in one run, and the number in
  the comment was what stopped anyone from trying.

## Files

- `src/MetroidPrime/Player/CMorphBall.cpp`
  - `:445-479` - the comment above `fn_800CD4B8`: the `uchar`-arithmetic claim is corrected and
    replaced by the measured bit layout, with the emitted words for each of the three fields.
  - `:481-489` - new `SMorphBallAllocFlags`, the flag byte as a bit-field struct.
  - `:491-517` - `fn_800CD4B8`'s body in terms of those fields.

Not committed, per the brief.

WALL: fn_800C8CE0 74.05% - retail needs a 32-byte frame with r31 and three spills (0x8/0xc/0x10);
9 spellings measured, best is 19/19 instructions with the right frame and r31 but 7/19 words
differing, all instruction scheduling (same loads and stores, different order).
