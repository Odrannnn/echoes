# port-tevcombiners-init

`kind: port`, target `fn_802BE51C` = retail `CTevCombiners::Init` (0x802BE51C, 0x6C bytes).
**PASS** on `./tools/goal_check.sh build/goal/item.json` in `../wt-mp2-goal-L1` (lane 1).

## What the item's `reason` said, and what was actually true

The reason claims "every callee is now defined in `src/Kyoto/Graphics/CGraphicsSetTevOp.cpp`, so
this file plus a one-line rename is the whole change". **That file does not exist in this tree**
(`ls src/Kyoto/Graphics/ | grep -i tev` → nothing), and neither callee is defined anywhere:

    $ grep -rn "fn_802BE4D8\|fn_802BE588" src include configure.py
    include/Kyoto/Graphics/CTevCombiners.hpp:189:  static bool sValidPasses[2];
    include/Kyoto/Graphics/CTevCombiners.hpp:190:  static uint sNumEnabledPasses;
    (only the .hpp lines matched; both fn_ names appear nowhere in src/)

`fn_802BE4D8` (`DeletePass`) and `fn_802BE588` (`RecomputePasses`) are in `config/G2ME01/symbols.txt`
(0x802BE4D8 size 0x44, 0x802BE588 size 0x40) and neither is defined. So the change is a new file
with three functions, not a rename. `CTevCombiners::sValidPasses` / `sNumEnabledPasses` are
declared in the header and **defined nowhere**, which is why the new file defines them.

## The change

- **new** `src/Kyoto/Graphics/CTevCombinersHostInit.cpp` - `fn_802BE51C`, `fn_802BE4D8`,
  `fn_802BE588` plus a file-local `ExecutePass` (retail's `CTevPass::Execute`, 0x802BE398, 0xB4),
  and the two class statics. Port-only: listed in `files.cmake`, **not** in `configure.py`, so
  mwcceppc never sees it and the DOL objects are unchanged (`gate.sh`'s DOL sha1 and 86-REL steps
  pass).
- `files.cmake` - the one listing line, after `CGraphicsHostWorkspace.cpp`.
- `include/Kyoto/Graphics/CTevCombiners.hpp` - `extern "C"` declarations of the three names above
  the class and three `friend` lines in the private section. Needed because `sValidPasses` and
  `sNumEnabledPasses` are private and the port's bodies are free functions under retail's names
  (measured: 9 compile errors, all "is private within this context", without them).
- `src/MetroidPrime/PortReachStubs.cpp` - `reachstub_472` retired with a dated comment. Required:
  it is `asm("fn_802BE51C")`, so it collides under `-DMP_BOOT_STUBS=ON`. Re-running
  `tools/restub_reach.py` puts it back.
- `src/Kyoto/Graphics/CGraphicsHostStartup.cpp` - two comments that claimed no body existed; the
  `extern void fn_802BE51C();` declaration and the call at `InitGraphicsDefaults` are unchanged.
- `docs/research/port_link_gap_list.md`, `docs/research/port_link_gap.md`, `PORT_NOTES.md` - the
  gate fails on a listed symbol that is no longer missing, and the per-group count had to follow.

## Measured

- `tools/link_check.sh`: **289 -> 288** unique undefined, 0 duplicates; `fn_802BE51C` gone, and
  `diff <(cut -f1 build/goal/judge/undef.base.txt|sort) <(cut -f1 <new>|sort)` shows exactly one
  line removed and nothing added.
- `tools/probe_sources.sh`: 751 files, 0 failed, 0 errors; link LINKED (288 undefined, 0 dups).
- `tools/goal_check.sh`: PASS. gate.sh green (DOL sha1, 86 RELs, report diff, wiring, docs claims,
  port probe); matched 12203 -> 12203, linked 5860 -> 5860; `All: 34.45% fuzzy, 27.73% matched,
  12.89% linked (12203 / 28465)`.
- 4 paths changed under `src/` or `include/`.

## What the bodies are (all read from `build/G2ME01/main.elf` with `tools/dis.sh`)

`Init` (0x802BE51C): `sNumEnabledPasses = 2`; `sValidPasses[0] = sValidPasses[1] = 1`;
`for (i = 0; i < 2; ++i) DeletePass(i);` (the `cmpwi/blt` pair at 0x802BE54C is the loop, not an
unrolled pair of calls); both bytes cleared; `RecomputePasses()`.

`DeletePass` (0x802BE4D8): `Execute(&lbl_80416B48, passIndex)` - the address is a **constant**, so
both loop iterations in `Init` submit the same record to two different GX tev stages;
`sValidPasses[passIndex] = 0`; `RecomputePasses()`.

`RecomputePasses` (0x802BE588): `lbz r3, sValidPasses[1]` through `neg/or/srwi 31/addi 1`, i.e.
**1 or 2, reading only index 1**, stored as a `uchar` into `sNumEnabledPasses`, then
`CGX::SetNumTevStages`. So `Init`'s `sNumEnabledPasses = 2` is visible only inside its own loop;
what it leaves behind is one tev stage. Worth recording: the naive reading of "both passes, then
count the valid ones" would compute 2 at the end and is wrong.

The two `.sdata` objects are `tools/sda.py` outputs: `_SDA_BASE_` 0x8041FD80 - 29352 = 0x80418AD8
(`lbl_80418AD8`) and - 29348 = 0x80418ADC (`lbl_80418ADC`).

`Execute`'s 11 loads give the record's shape without guessing (0x4C, the size `symbols.txt` records
for `lbl_80416B48`); the table is in the file's header. `include/.../CTevCombiners.hpp`'s own
`CTevPass` is a **different** shape (its ctor at 0x8025AA1C copies `ColorPass`/`AlphaPass` as flat
4-word block moves), so the file uses its own struct rather than the header's type.

## The one thing the port cannot have, and why it is stated rather than smoothed over

`lbl_80416B48` is `CTevCombiners::kEnvPassthru`. `grep -n "27464"` over an `objdump -d` of
`main.elf` finds its address in exactly four places in the whole DOL, all in this function cluster
(0x802BE334 `ResetStates`, 0x802BE48C, 0x802BE4E8, 0x802BE6B8), and the only writer is
`fn_802BE5D8`, which takes no arguments and is registered in `.ctors`
(`objdump -s -j .ctors main.elf` lists `802be5d8` between `802be2e8` and `802c3478`). Static
constructors do not run in this port, so the record is **zeroed** where retail has kEnvPassthru's
real values: the GX tev stage registers `DeletePass` writes during `Init` are zeros here. That is
stated in the file header and in this note rather than papered over. Nothing in the boot path reads
them back - `CGX::STevState` is what `SetNumTevStages` and `ResetGXStates` use, and
`RecomputePasses` narrows the hardware to one stage at the end of the same sequence.

## NEW:

- `port-tev-combiners-setuppass | port | fn_802BE47C | 0x802BE47C, 0x5C bytes, CTevCombiners::SetPassCombiners(int, const CTevPass&): compares the pass against &kEnvPassthru and calls DeletePass, else Execute + set the valid byte + RecomputePasses. Its Execute/RecomputePasses callees and the CGX functions it reaches now all exist, and the store it makes is the same kEnvPassthru record CTevCombinersHostInit.cpp already models.`
---

# Run 2 (2026-10-02, lane 1, `../wt-mp2-goal-L1`) - PASS, and a much smaller diff

`kind: port`, target `fn_802BE51C`. `./tools/goal_check.sh build/goal/item.json` → **PASS**
(this run's full verdict is at the end).

## Run 1's central premise is superseded, and that is the whole difference

Run 1 reported "`ls src/Kyoto/Graphics/ | grep -i tev` → nothing" and built a new file
`src/Kyoto/Graphics/CTevCombinersHostInit.cpp` with three functions plus a private-data workaround
(`friend` lines, `extern "C"` declarations in the header). **All of that is no longer needed**:
`src/Kyoto/Graphics/CGraphicsSetTevOp.cpp` exists now, it defines `CTevCombiners::DeletePass`,
`CTevCombiners::RecomputePasses`, `CTevCombiners::SetupPass`, `CTevCombiners::SetPassCombiners`,
`CTevCombiners::CTevPass::Execute`, and it already defines the two private statics
`sValidPasses` / `sNumEnabledPasses` as class members. That is item `port-tevop-cgraphics-setevop`'s
work (see `docs/goal-notes/port-tevop-cgraphics-setevop.md`), which landed between the runs.

So the item's `reason` - "this file plus a one-line rename is the whole change" - is now **true**,
where run 1 measured it as false. Recorded so the next run does not re-derive it.

## The change (5 files, +107/-17)

- **`src/Kyoto/Graphics/CGraphicsSetTevOp.cpp`** - `void CTevCombiners::Init()` (18 lines) plus a
  header section transcribing retail's 0x6C bytes. The header's old sentence saying `Init` "is still
  undefined ... so it is not here" is corrected in place.
- **`src/Kyoto/Graphics/CGraphicsHostStartup.cpp`** - the call in `InitGraphicsDefaults` is now
  `CTevCombiners::Init()`; the `extern void fn_802BE51C();` declaration and the two comments that
  said no body existed are gone. `CTevCombiners.hpp` is already reachable here
  (`CGraphics.hpp:9` includes it), so no new include.
- **`src/MetroidPrime/PortReachStubs.cpp`** - `reachstub_472` retired with a dated comment.
- **`docs/research/port_link_gap_list.md`** - the `fn_802BE51C` entry deleted (1 line).
- **`docs/research/port_link_gap.md`** - the `unmangled: fn_/lbl_/globals` row 56 → 55, plus a dated
  283 → 282 entry.

## Measured

- `tools/link_check.sh`: **288 → 287** unique undefined, 0 duplicates, 0 compile errors.
- `diff <(cut -f1 build/goal/judge/undef.base.txt|sort) <(cut -f1 <new list>|sort)`: exactly one
  line removed (`fn_802BE51C`), nothing added.
- `python3 tools/link_gap.py --rebuild`: `282 MISSING` over `744` objects (was 283), 0 duplicate.
- `tools/probe_sources.sh`: 751 files, 0 failed, 0 errors; link LINKED (287 undefined, 0 dups).
- `goal_check.sh`: gate green (DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe);
  matched **12205 → 12205**, linked **5860 → 5860**; `All: 34.46% fuzzy, 27.74% matched,
  12.89% linked (12205 / 28465)`. 3 paths under `src/`+`include/`.
- The DOL is untouched: both edited sources are **port-only** (`files.cmake`, never `configure.py`;
  `grep -n "Kyoto/Graphics" configure.py` lists 12 entries, none of them these two), which is why
  matched/linked are identical rather than merely non-falling.

## `Init`'s body, and the two stores a careless reading drops

From `build/G2ME01/main.elf`, `objdump -d --start-address=0x802BE51C --stop-address=0x802BE588`:

```
li r3,1 ; li r0,2 ; addi r31,r13,-29352 (&sValidPasses) ; li r30,0
stw r0,-29348(r13)      ; sNumEnabledPasses = 2
stb r3,-29352(r13)      ; sValidPasses[0] = 1
stb r3,1(r31)           ; sValidPasses[1] = 1
loop: mr r3,r30 ; bl 802be4d8 ; addi r30,r30,1 ; cmpwi r30,2 ; blt loop
li r0,0 ; stb r0,0(r31) ; stb r0,1(r31) ; bl 802be588
```

The three stores before the loop are **clobbered by the loop**: `DeletePass(i)` writes
`sValidPasses[i] = false` itself, and each call runs `RecomputePasses`. `sNumEnabledPasses = 2` is
dead on arrival - after the last `DeletePass` it is 1, and the explicit `RecomputePasses()` at the
end leaves it 1. They are kept: the "never delete an initialisation to gain percent" rule, and the
store/call sequence is the observable behaviour (`CGX::SetNumTevStages` is called 3 times, not 1).
`cmpwi r30,2` + `blt` is a counted loop, so `for (int i = 0; i < 2; ++i) DeletePass(i);` - not an
unrolled pair and not a range-for.

## The thing that cost this run two extra judge cycles, and will cost the next one the same

**A `port` item that closes a listed gap symbol fails `gate.sh` twice, and neither failure is in
`src/`.** Measured, in order:

1. `python3 tools/link_gap.py --rebuild` →
   `stale: fn_802BE51C is listed but no longer missing - delete the entry` → `GATE FAIL: link-gap`.
   Fix: delete the line from `docs/research/port_link_gap_list.md`.
2. `python3 tools/check_docs_claims.py` →
   `stale: the gap table says unmangled: fn_/lbl_/globals is 56, the generated list has 55` and
   `stale: the gap table's rows sum to 283, the generated list holds 282` → `GATE FAIL: docs`.
   Fix: edit the per-group row in `docs/research/port_link_gap.md` and add a dated entry.

`MP_GATE_DOCS_WRITE=1` (which `goal_check.sh` sets) does **not** write either: it rewrites the
state block, module list and probe count, but the gap table is check-only. So a lane closing a gap
symbol must edit both research files by hand or the item cannot pass. Worth a line in
`docs/goal-unit-prompt.md`; not filed as `NEW:` because it is documentation, not work that raises a
count.

## Correction to run 1's reach-stub claim

Run 1 wrote that retiring `reachstub_472` was **required** because it is `asm("fn_802BE51C")` and
"collides under `-DMP_BOOT_STUBS=ON`". That was true for run 1 because run 1 defined `Init` as a
bare `extern "C"` function under retail's linker name. **This run defines the C++ member**
(`_ZN13CTevCombiners4InitEv`), which is a different symbol, so nothing was ever going to collide -
`link_check.sh` reports 0 duplicates and the boot probe is unaffected either way. The stub is still
retired, because a stub that logs `fn_802BE51C` is a claim about what the boot path demands and it
is no longer one, and because the file's own convention is to retire stubs whose symbol the tree no
longer needs. Re-running `tools/restub_reach.py` will put it back (run 1 saw that too).

## Left alone, on purpose

`PortReachStubs.cpp`'s header claims "**318 stubs** - 258 Itanium, 3 `REL_Load*`, 57 unmangled -
plus 6 `reachdata_`". Measured on this tree before this change: **367** function stubs
(`grep -cE '^extern "C" void reachstub_[0-9]+\(\) asm\('`), 306 of them `asm("_Z...`)`, and **9**
`reachdata_`. So the header was already wrong by 49 before I touched it; this change takes it to 366.
Fixing it is unrelated to the item, so it is reported here rather than changed. **NEW:** not filed -
it is a stale count, not work that raises a count.

## NEW:

(none - the `port-tev-combiners-setuppass` line above is still the open one from run 1. `Init` is
now written, so that item's premise is unchanged: 0x802BE47C's callees all exist. Re-check
`src/Kyoto/Graphics/CGraphicsSetTevOp.cpp` before starting it - `SetupPass` may already be there.)

## One more doc this change makes false, and the final diff

`PORT_NOTES.md:272` said "One retail call stays a reach stub because it has no decompiled body in
this tree: `CTevCombiners::Init` (`fn_802BE51C`)". That is now false, so it is corrected in place
and marked superseded (AGENTS.md: update in the same change). **`docs/HANDOFF.md:324` also names
`fn_802BE51C`** among 11 pre-renderer boot stubs - left alone, because that paragraph is a dated
measurement of a past `boot_probe.sh` run ("Measured after ... 300 frames, exit 0"), the driver
discards edits to `docs/HANDOFF.md` anyway, and rewriting a historical measurement would be the
worse error. It is stale in the same way every other boot-run figure there is.

Final diff, 6 files:

    PORT_NOTES.md                                |  9 ++++++--
    docs/research/port_link_gap.md               | 14 ++++++-
    docs/research/port_link_gap_list.md          |  1 -
    src/Kyoto/Graphics/CGraphicsHostStartup.cpp  | 24 ++++++----
    src/Kyoto/Graphics/CGraphicsSetTevOp.cpp     | 72 ++++++++++++++++++++-
    src/MetroidPrime/PortReachStubs.cpp          | 13 +++++--

Final `goal_check.sh` verdict: **PASS** (gate green, matched 12205 -> 12205, linked 5860 -> 5860,
`fn_802BE51C` gone from the undefined list, port undefined 288 -> 287, probe 751 files 0 failed).
