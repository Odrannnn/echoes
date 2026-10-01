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
