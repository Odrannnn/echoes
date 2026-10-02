# carve-801ff720 - `MetroidPrime/ScriptObjects/Carve801FF720` is `Matching` and flipped

**Result: PASS.** `./tools/goal_check.sh build/goal/item.json` exits 0, last line
`goal_check: PASS carve-801ff720`:

```
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 12532 -> 12536   linked 5903 -> 5907
  ok    check_symbol_names.py
  ok    All:  35.35% fuzzy, 29.19% matched, 12.93% linked (12536 / 28465 functions)
  ok    flip_test MetroidPrime/ScriptObjects/Carve801FF720.cpp: PASS, Object(Matching) in configure.py
goal_check: PASS carve-801ff720
```

**+4 matched, +4 linked, `total_functions` still 28465.** `build/report.json` has
`main/MetroidPrime/ScriptObjects/Carve801FF720` at `fuzzy_match_percent 100.0`,
`matched_functions 4 / total_functions 4`, `matched_code 384 / 384`, `complete_units 1`.

## What the carve is

`.text 0x801FF720..0x801FF8A0`, `0x180` = 384 bytes, **4 functions**, out of dtk's
`auto_03_801FF4C4_text` (`build/G2ME01/asm/auto_03_801FF4C4_text.s:192-306`):

```
fn_801FF720  0x801FF720  0xAC   43 instructions
fn_801FF7CC  0x801FF7CC  0x20    8 instructions
fn_801FF7EC  0x801FF7EC  0x4C   19 instructions
fn_801FF838  0x801FF838  0x68   26 instructions
```

**A 36-byte-element block's `reserve` and the three helpers it calls.** `mulli r0,r0,0x24` fixes
the element stride at 36 bytes; `fn_801FF720` reads `+0x08` against the second argument
(`cmpw`/`ble`, so the guard is signed), `allocate(count * 36)`, moves the old elements into the new
buffer, destroys the old ones, `Free`s the old block, and stores the new pointer and capacity. Its
only caller is `fn_801FE97C` (0x801FE97C, `symbols.txt:8309`, 0xB0 bytes, also unclaimed), which
calls it as `reserve(self, capacity)` - `cmpw r0,r30 ; beq`, `cmpw r30,r0 ; ble`, `bl fn_801FF720`.

**All four are byte-shape twins of four functions `src/MetroidPrime/Player/CGameState.cpp` already
has at 100%**, and the twin table is not an inference from a byte count: disassembling both ranges
of each pair out of `build/G2ME01/main.elf` and comparing word by word leaves **no differing word at
all once the `bl`s are masked**, and only **two** words differ unmasked - both `bl`s, both to this
copy's own element callees.

| this copy | twin in `CGameState.cpp` | bytes | what it is |
| --- | --- | --- | --- |
| `fn_801FF720` | `fn_801466F4` | 0xAC / 0xAC | the block's `reserve` |
| `fn_801FF7CC` | `fn_801467A0` | 0x20 / 0x20 | forwards both arguments to `fn_801FF7EC` |
| `fn_801FF7EC` | `fn_801467C0` | 0x4C / 0x4C | element destroy loop, stride 36 |
| `fn_801FF838` | `fn_8014680C` | 0x68 / 0x68 | element move loop, stride 36, returns the new end |

The other four call sites encode their twin's exact word: `allocate__Q24rstl17rmemory_allocatorFi`
(0x802FDAB8) and `Free__7CMemoryFPCv` (0x802CE388) are the same absolute targets, and the move and
the destroy sit at the same displacement in both copies.

The block is `SGameStateBlock`'s 16-byte layout (`include/MetroidPrime/Player/CGameStateBlocks.hpp:47`
- `+0x04` count, `+0x08` capacity, `+0x0C` data) read at the same three offsets as the twin's. It is
spelled **locally** (`struct SCarve801FF720`) rather than included: retail's class here is not
`CGameState`'s, and `x00_unk` is read and written by nothing in the DOL, so nothing here pins it.

Run boundaries are the neighbours, not a choice: `fn_801FF6B8` (0x68 bytes) ends the run beneath and
`fn_801FF8A0` (0xAC = 172 bytes, 43 instructions) starts the one above.

## The one deviation from the item, and it is not cosmetic: **`.cpp`, not `.c`**

The item asked for `Carve801FF720.c` in plain C. That cannot reproduce `fn_801FF838`'s or
`fn_801FF720`'s bytes, and the reason is an ABI fact worth keeping:

* Retail's callee takes its two iterators **by address** - `lwz r31,0(r3)` at 0x801FF848 and
  `lwz r0,0(r29)` at 0x801FF874, re-read every iteration, with `r3`/`r4` formed by the caller as
  `addi r3,r1,20` (0x801FF760) and `addi r4,r1,12` (0x801FF76C) and four stores at
  8(r1)/12(r1)/16(r1)/20(r1) filling two copies of each.
* MWCC passes a **one-word class with a user-defined constructor by address**, which is what makes
  the caller materialise those pairs. mwcceppc's C front end has no class type to ask for this.
* The plain-C spelling of the same loop, `void* const* begin, void* const* end`, is already
  measured on the twin: 92.69% at 100 bytes instead of retail's 104, because it hoists the loop
  bound (`CGameState.cpp:88`).

So the unit is `src/MetroidPrime/ScriptObjects/Carve801FF720.cpp` with `extern "C"` on **every**
definition, which is what the `.c` was for anyway: `symbols.txt` carries the unmangled
`fn_<addr>` names and objdiff pairs by name, so a C++ definition without `extern "C"` would mangle
to `_Z<len>fn_<addr>v` and pair nothing. Precedent for the shape: `CGameState.cpp` itself, and the
carve `.cpp` units (`find src -name 'Carve*.cpp'` counts 23, this one included).

## Files (the carve is four, plus one port file the gate forced)

| file | change |
| --- | --- |
| `src/MetroidPrime/ScriptObjects/Carve801FF720.cpp` | new, 152 lines; definitions **descending by address** (`fn_801FF838`, `fn_801FF7EC`, `fn_801FF7CC`, `fn_801FF720`) |
| `configure.py:738` | `Object(Matching, "MetroidPrime/ScriptObjects/Carve801FF720.cpp"),` one line, in address order after `Carve801FF4A4.c` |
| `config/G2ME01/splits.txt:1159-1160` | `.text start:0x801FF720 end:0x801FF8A0`, immediately after `Carve801FF4A4.c` and before `ScriptLoader/SpacePirate.cpp` |
| `files.cmake:560` | `src/MetroidPrime/ScriptObjects/Carve801FF720.cpp`, next to the other carve2 entries |
| `src/MetroidPrime/PortLinkStubs.cpp:718-748` | `stub_182` for `fn_801FEA98`, `stub_183` for `fn_801FD8E0`, and the header's own counts |

**No `PortLinkStubs.cpp` duplicate existed**: `grep -rn "fn_801FF720\|fn_801FF7CC\|fn_801FF7EC\|
fn_801FF838" src/ include/` found nothing outside this change, so rule 3 of the carve vein needed no
deletion. The four names are the only ones the item listed, and no port unit defines them.

**The fifth file, and why it was not optional.** Listing the unit in `files.cmake` asks the *host*
link for the two element callees, `fn_801FEA98` (0x801FEA98, `symbols.txt:8311`, 0x20) and
`fn_801FD8E0` (0x801FD8E0, `symbols.txt:8279`, 0x20), which nothing in the tree defines, and both
calls are in the bytes - `fn_801FF838` is one `bl fn_801FEA98` and `fn_801FF7EC` one
`bl fn_801FD8E0`, so no spelling can drop them. The first judge run failed exactly there:

```
FAIL  gate.sh
      GATE FAIL: probe link-gap
      gap grew: fn_801FD8E0 is not in port_link_gap_list.md
      gap grew: fn_801FEA98 is not in port_link_gap_list.md
probe: 766 files, 0 failed, 0 errors; link: NOT LINKED (293 undefined, 0 duplicates)
```

Closed the way `stub_178` closed the same failure for `fn_801FDC88` (and `carve-800045a0.md` closed
it again for `fn_80008D68`): empty-body `extern "C" void stub_182() asm("fn_801FEA98")` and
`stub_183`. `PortLinkStubs.cpp` is not in `configure.py`, so the stubs cannot reach `main.dol`;
dtk's own `auto_03_801FDC88_text.o` and `auto_03_801FA3CC_text.o` supply the DOL's bytes. After:

```
probe: 766 files, 0 failed, 0 errors; link: LINKED (291 undefined, 0 duplicates)
link_check: unique undefined symbols 291
port link gap: ok: 286 MISSING symbol(s), all accounted for in port_link_gap_list.md
```

**Numbered 182/183, not 179/180**, because `build/goal/notes/` shows several lanes in flight have
already claimed `stub_179`, `stub_180` and `stub_181`, and a duplicate `stub_N` is a duplicate
definition the host link refuses. **Whoever merges several of these lanes must renumber.**

## Verification (every number measured in this run)

```
python3 tools/carve_diff.sh 801ff720 180 <our .o>
    retail: 96 instructions, 384 bytes / ours: 96 instructions, 384 bytes
    differing instructions: 7   <- the seven `bl`s (0x801FF750, 0x801FF788, 0x801FF79C,
                                   0x801FF7A4, 0x801FF7D8, 0x801FF810, 0x801FF868), all
                                   unlinked relocations; every other instruction identical
    (re-run with the `bl` words masked in a scratch script: "BYTE-EXACT with bl masked: True")
./tools/unit_fit.sh MetroidPrime/ScriptObjects/Carve801FF720.cpp
    .text claimed 384 ours 384 retail 384 fits
    no extra functions: our object defines only what the retail unit object does
python3 tools/check_decl_order.py --unit MetroidPrime/ScriptObjects/Carve801FF720.cpp
    ok (and nm on the object shows fn_801FF720 @0x0, fn_801FF7CC @0xac, fn_801FF7EC @0xcc,
    fn_801FF838 @0x118 - ascending, i.e. retail's order)
python3 tools/check_symbol_names.py
    0 missing names (goal_check step 3)
sha1sum build/G2ME01/main.dol
    6ef9b491d0cc08bc81a124fdedb8bfaec34d0010   (after decomp_build.sh and after flip_test)
./tools/flip_test.sh MetroidPrime/ScriptObjects/Carve801FF720.cpp
    PASS -> kept as Matching   (kept 1/1, failed 0, skipped 0)
tools/gate.sh per-function diff
    SPLIT  main/auto_03_801FF4C4_text: 30 function(s) moved into
           main/MetroidPrime/ScriptObjects/Carve801FF720, main/auto_03_801FF8A0_text
           (exact count match - a split, not a loss)
```

No spelling took iterations: the twin's source spelled each function, the object was byte-exact
against retail on the **first** compile, and nothing after that changed a byte.

`auto_03_801FF4C4_text` re-split around the claim, as carves do: the report now has
`auto_03_801FF4C4_text` (0x801FF4C4..0x801FF720, **5 functions, 604 bytes**, `matched_functions`
unset), the new Matching unit, and `auto_03_801FF8A0_text` (0x801FF8A0.., **26 functions, 5488
bytes**). `total_functions` is 28465 before and after.

## What is still there, for the next lane

* `auto_03_801FF8A0_text`, 0x801FF8A0 onward: 26 functions, 5488 bytes - the run above this claim.
  `fn_801FF8A0` (0xAC) is the first of them and starts with the same `stwu r1,-0x30`/`mflr` frame
  as `fn_801FF720`, so the twin-pairing that seeded this item should also pair something up there.
* `auto_03_801FF4C4_text`, 0x801FF4C4..0x801FF720: 5 functions, 604 bytes - the run below.
* The two element callees are `fn_801FEAB8` (0x28) and `fn_801FD924` (0x74), one chain deeper than
  the two stubs. Carving them would move the gap along rather than close it, which is the reason
  `stub_178`'s comment records.

## Caveats

- `docs/HANDOFF.md` and `docs/RUNNING_THE_DECOMP.md` show as modified in `git status`: that is
  **`goal_check.sh`/`gate.sh` rewriting their own derived counts** (matched 12532 -> 12536,
  linked 5903 -> 5907, DOL units 10964 -> 10968, probe 765 -> 766), not a lane edit. No `tools/`
  file and nothing under `build/goal/` was touched.
- The port stubs are stand-ins, not decompilation, and their comment says so; `fn_801FEA98` and
  `fn_801FD8E0` remain in `docs/research/port_link_gap.md`'s accounting as unclaimed DOL code.
- The two `SStateIter`-shaped structs in this file (`SCarve801FF720`, `SCarve801FF720_Iter`) name
  the offsets the bytes read and nothing more. `fn_801FE97C` is the one caller and it is itself
  unclaimed, so nothing more is pinned.
- No `WALL:` line: nothing here sat below 100%.
---

# Attempt 2 (lane 8, re-run on `08ac570c`) — **PASS, again**

The previous attempt did **not** fail. `build/goal/run.log` records it judge-PASS at
10:41:03Z on base `719bf8d7`, then four seconds later:

```
[2026-10-02 10:41:03Z] judge PASS carve-801ff720 - not reviewed (match items are outside MP_GOAL_REVIEW_KINDS='port progress')
[2026-10-02 10:41:03Z] goal/decomp moved (719bf8d -> 08ac570) during carve-801ff720 - carrying the judged change onto it
...
Applied patch to 'config/G2ME01/splits.txt' with conflicts.   (+ configure.py, files.cmake, PortLinkStubs.cpp)
carve-801ff720 does not apply on 08ac570 - releasing it for a fresh attempt
```

**So this is not a wall and not a stale item: it is a rebase that lost.** The judged patch
conflicted on all four files because the branch moved under it, and the driver released the
item rather than re-running the resolution. Everything below is re-measured on `08ac570c`;
nothing is carried forward from the notes above.

## Re-measured, and the conclusion the first run reached still holds - from a different twin

The first run paired these four against `CGameState.cpp`. On this tree there is a **closer**
twin available, and comparing the two ranges directly is a stronger measurement than
comparing either against a third copy:

```
$ ./build/binutils/powerpc-eabi-objdump -d --start-address=0x801FF5A0 --stop-address=0x801FF720 \
      build/G2ME01/main.elf > a;  ... 0x801FF720..0x801FF8A0 > b
instrs: 96 96
801ff5d0 e9e40f48 bl 802fdab8 <allocate__Q24rstl17rmemory_allocatorFi> | 801ff750 69e30f48 bl 802fdab8 <...>
801ff624 65ed0c48 bl 802ce388 <Free__7CMemoryFPCv>                  | 801ff7a4 e5eb0c48 bl 802ce388 <...>
801ff690 a9dfff4b bl 801fd638 <fn_801FD638>                        | 801ff810 d1e0ff4b bl 801fd8e0 <fn_801FD8E0>
801ff6e8 59f7ff4b bl 801fee40 <fn_801FEE40>                        | 801ff868 31f2ff4b bl 801fea98 <fn_801FEA98>
differing words: 4
```

`src/MetroidPrime/ScriptObjects/Carve801FF5A0.cpp` (0x801FF5A0..0x801FF720, `Matching`) is
**byte-for-byte this range with two `bl` targets swapped** - 96 instructions each, 4 differing
words, and 2 of those are the same absolute targets. So the body is that file's body with
`fn_801FEE40` -> `fn_801FEA98` and `fn_801FD638` -> `fn_801FD8E0`. No spelling search was needed
and none was done: the sibling's source *is* the answer, and the first attempt's account of why
the unit has to be `.cpp` re-measured the same way (below).

## Result

`./tools/goal_check.sh build/goal/item.json` exits 0:

```
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 12561 -> 12565   linked 5930 -> 5934
  ok    check_symbol_names.py
  ok    All:  35.38% fuzzy, 29.23% matched, 12.96% linked (12565 / 28465 functions)
  ok    flip_test MetroidPrime/ScriptObjects/Carve801FF720.cpp: PASS, Object(Matching) in configure.py
goal_check: PASS carve-801ff720
```

**+4 matched, +4 linked.** `build/report.json` has
`main/MetroidPrime/ScriptObjects/Carve801FF720` at `fuzzy_match_percent 100.0`,
`matched_functions 4 / total_functions 4`, `matched_code 384 / 384`, `complete True`.
`total_functions` is 28465 before and after; `complete_units` 774 -> 775.

## Files (the carve is four, plus the one port file the gate forces)

| file | change |
| --- | --- |
| `src/MetroidPrime/ScriptObjects/Carve801FF720.cpp` | new, 154 lines; definitions **descending by address** (`fn_801FF838`, `fn_801FF7EC`, `fn_801FF7CC`, `fn_801FF720`) |
| `configure.py:742` | `Object(Matching, "MetroidPrime/ScriptObjects/Carve801FF720.cpp"),` one line, address order after `Carve801FF5A0.cpp` |
| `config/G2ME01/splits.txt:1171-1172` | `.text start:0x801FF720 end:0x801FF8A0`, after `Carve801FF5A0.cpp`, before `ScriptLoader/SpacePirate.cpp` |
| `files.cmake:564` | `src/MetroidPrime/ScriptObjects/Carve801FF720.cpp`, next to the other carve2 entries |
| `src/MetroidPrime/PortLinkStubs.cpp` | `stub_182` / `stub_183` and the header's own counts (156 / 153+4) |

No `PortLinkStubs.cpp` duplicate existed: `grep -rn "fn_801FF720\|fn_801FF7CC\|fn_801FF7EC\|
fn_801FF838" src/ include/` returns nothing outside the new file.

### `.cpp`, not the `.c` the item asked for — re-measured, not inherited

The item says plain C. Re-measured on this tree, C mode is still wrong here, and the mechanism
is the same one `Carve801FF5A0.cpp` documents at length: the two iterators arrive **by address**
(`lwz r31,0(r3)` at 0x801FF848, `lwz r0,0(r29)` re-read every iteration at 0x801FF874), which
needs a one-word class with a converting constructor — MWCC passes such a class by address, and
mwcceppc's C front end has no class type to ask for it. Plain-C spellings of the same loop are
measured to hoist the bound (`CGameState.cpp:88`: 100 bytes against retail's 104, 92.69%).

`Carve801FDB5C.c` and `Carve801FF5A0.cpp` are `.c`/`.cpp` siblings of the same shape and the
second is `.cpp` for exactly this reason, so `.cpp` is the house answer, not a local deviation.
Every definition is `extern "C"`, which is what the `.c` rule was protecting:
`powerpc-eabi-nm` on the object shows exactly four `T fn_801FF<addr>` and nothing else.

### The fifth file, and the stub numbers moved

Same trade the first attempt hit, **now on different numbers because the tree moved**:

```
FAIL  gate.sh
      GATE FAIL: probe link-gap
      gap grew: fn_801FD8E0 is not in port_link_gap_list.md
      gap grew: fn_801FEA98 is not in port_link_gap_list.md
probe: 771 files, 0 failed, 0 errors; link: NOT LINKED (293 undefined, 0 duplicates)
```

Closed with `stub_182` (`fn_801FEA98`) and `stub_183` (`fn_801FD8E0`), both empty bodies,
numbered **182/183 and not 182/183-by-luck**: `stub_181` is the highest present here, and
`stub_179`/`stub_180`/`stub_181` are already taken by the `Carve801FF5A0.cpp` callees
(`allocate__Q24rstl17rmemory_allocatorFi`, `fn_801FEE40`, `fn_801FD638`) that the same rebase
brought in. **Whoever merges several carve lanes must renumber** — `stub_179..181` here and
`stub_179..183` on the other lane's base overlap. After:

```
probe: 771 files, 0 failed, 0 errors; link: LINKED (291 undefined, 0 duplicates)
port link gap   ok
```

Both new symbols are MWCC `destroy<T>(T*)` forwarders, 0x20 bytes each (`symbols.txt:8279`,
`:8311`), one `bl` to `fn_801FD900` / `fn_801FEAB8` — so carving them instead only moves the
gap one function along, which is why the stub is the right trade.

Header counts moved by the same +2 as the file: "154 of them" -> **"156 of them"** and
"151 functions, 4 data objects" -> **"153 functions, 4 data objects"**, measured
`grep -c 'stub_[0-9]*() {'` = **153** and 4 `asm("...")` data objects below the
`// Data objects` banner. Note the two lines were **already** one apart before this change
(151+4 = 155, not 154) — the same un-derived count the header's own third paragraph records
having been wrong before; I carried the existing relationship forward rather than inventing a
new base. `tools/check_docs_claims.py` does not derive this number (it greps neither
`PortLinkStubs` nor `stub_`), so the gate cannot catch it either way, and `gate.sh`'s
`port link dups` step is what protects the thing that matters: a duplicate definition the host
link refuses. It reports 0 duplicates.

## Verification (every number measured in this run)

```
./tools/decomp_build.sh
    All:  35.38% fuzzy, 29.23% matched, 12.96% linked (12565 / 28465 functions)
sha1sum build/G2ME01/main.dol
    6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
./tools/unit_fit.sh MetroidPrime/ScriptObjects/Carve801FF720.cpp
    .text claimed 384 ours 384 retail 384 fits
    no extra functions: our object defines only what the retail unit object does
python3 tools/carve_diff.sh 801ff720 180 build/G2ME01/obj/.../Carve801FF720.o
    retail: 96 instructions, 384 bytes / ours: 96 instructions, 384 bytes
    differing instructions: 7   <- the seven `bl`s (0x801FF750, 0x801FF788, 0x801FF79C,
                                    0x801FF7A4, 0x801FF7D8, 0x801FF810, 0x801FF868), all
                                    unlinked relocations
    (re-checked with a scratch script masking opcode+LK+AA of every branch:
     "differing after masking branch displacement+LK+AA: 0" - i.e. byte-exact once the
      relocations are filled in)
powerpc-eabi-nm --numeric-sort build/G2ME01/obj/.../Carve801FF720.o
    00000000 T fn_801FF720   000000ac T fn_801FF7CC
    000000cc T fn_801FF7EC   00000118 T fn_801FF838      <- ascending = retail's order
./tools/flip_test.sh MetroidPrime/ScriptObjects/Carve801FF720.cpp
    PASS  -> kept as Matching   (kept 1/1, failed 0, skipped 0)
tools/gate.sh per-function diff
    SPLIT  main/auto_03_801FF720_text: 30 function(s) accounted for across 2 new unit(s) in main
           (exact count match - a split, not a loss)
./tools/probe_sources.sh
    771 files, 0 failed, 0 errors; link: LINKED (291 undefined, 0 duplicates)
python3 tools/check_symbol_names.py
    0 missing names
```

`auto_03_801FF720_text` re-split around the claim, as carves do: the report now has
`main/MetroidPrime/ScriptObjects/Carve801FF720` (0x801FF720..0x801FF8A0, **4 functions,
384 bytes, 4/4 matched**) and `main/auto_03_801FF8A0_text` (0x801FF8A0..0x80200E10,
**26 functions, 5488 bytes**). The claim cuts at neither end of the old auto unit, so no
`@stringBase0` pool moved — consistent with `RUNNING_THE_DECOMP.md`'s "a split that moves many
functions costs fidelity", and measured: nothing else in `report.json` got worse.

## What is still there, for the next lane

* `auto_03_801FF8A0_text`, 0x801FF8A0..0x80200E10: **26 functions, 5488 bytes**. The
  twin-pairing that seeded this item should keep paying there: the 0xAC/0x20/0x4C/0x68 run
  repeats at 0x801FF8A0 and 0x801FFA20, and the helper triplet stands alone at
  0x801FF94C/0x801FF96C/0x801FF9B8 and 0x801FFACC/0x801FFAEC/0x801FFB38. **The next 0x180 is
  0x801FF8A0..0x801FFA20**, and its only new element callees will be a fresh pair of 0x20-byte
  forwarders, so budget two more stubs.
* `fn_801FE97C` (0x801FE97C, 0xB0 bytes, `symbols.txt:8309`) is `fn_801FF720`'s only caller
  and is still unclaimed; it calls it as `reserve(self, capacity)`.
* `auto_03_801FF4C4_text`, 0x801FF4C4..0x801FF5A0: 1 function, 220 bytes - the run below.

## Caveats

- `docs/HANDOFF.md` and `docs/RUNNING_THE_DECOMP.md` show as modified in `git status`: that is
  **`goal_check.sh`/`gate.sh` rewriting their own derived counts** (matched 12561 -> 12565,
  linked 5930 -> 5934, DOL units 10982 -> 10986), not a lane edit, and the driver discards it.
  No `tools/` file and nothing under `build/goal/` but this notes file was touched.
- The two port stubs are stand-ins, not decompilation, and their comment says so;
  `fn_801FEA98` and `fn_801FD8E0` stay in `docs/research/port_link_gap.md`'s accounting as
  unclaimed DOL code.
- The struct here is spelled locally (`SCarve801FF720Block`), not by including a header: retail's
  class at this address is not `CGameState`'s, and `x00` is read and written by nothing in the
  DOL, so nothing pins it. `SStateIter` is duplicated rather than shared for the same reason.
- No `WALL:` line: nothing here sat below 100%.

---

# Attempt 3 (lane 11, base `ddbfb010`) — **PASS, third time**

Judge, run by hand at the end of this attempt in this worktree:
`./tools/goal_check.sh build/goal/item.json` exits 0.

```
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 12571 -> 12575   linked 5934 -> 5938
  ok    check_symbol_names.py
  ok    All:  35.44% fuzzy, 29.25% matched, 12.97% linked (12575 / 28465 functions)
  ok    flip_test MetroidPrime/ScriptObjects/Carve801FF720.cpp: PASS, Object(Matching) in configure.py
goal_check: PASS carve-801ff720
```

**+4 matched, +4 linked, `total_functions` still 28465, `complete_units` 775 -> 776.**
`build/report.json`: `main/MetroidPrime/ScriptObjects/Carve801FF720`
`fuzzy_match_percent 100.0`, `matched_functions 4 / total_functions 4`,
`matched_code 384 / 384`, `complete True`.

## Re-measured first, on THIS tree (`ddbfb010`), and the item is not stale

`grep -rn "Carve801FF720\|fn_801FF720\|fn_801FF838" src/ include/ configure.py
config/G2ME01/splits.txt files.cmake` returned **nothing** before this attempt, and
`build/report.json` had `main/auto_03_801FF720_text` still whole: **30 functions, 5872 bytes**,
`.text` virtual address 2149578528 = 0x801FF720. So nothing of the item was on the branch; the
two earlier PASSes were both lost to the driver's rebase, not to a failing judge (attempt 2's
section above records the exact rebase lines). This attempt is a fresh `git reset --hard`-clean
start, not an increment.

## The measurement that produced the body: the sibling, re-run here

Attempt 2's shortcut is still the shortest path and it was re-measured, not inherited:

```
$ ./build/binutils/powerpc-eabi-objdump -d --start-address=0x801FF5A0 --stop-address=0x801FF720 \
      build/G2ME01/main.elf > a
$ ./build/binutils/powerpc-eabi-objdump -d --start-address=0x801FF720 --stop-address=0x801FF8A0 \
      build/G2ME01/main.elf > b
96 instructions each (109 lines of objdump each); **4 differing words, all `bl`**:
  0x801ff690 bl 801fd638 <fn_801FD638> | 0x801ff810 bl 801fd8e0 <fn_801FD8E0>
  0x801ff6e8 bl 801fee40 <fn_801FEE40> | 0x801ff868 bl 801fea98 <fn_801FEA98>
  0x801ff5d0 bl 802fdab8 <allocate__Q24rstl17rmemory_allocatorFi>   (same word in b)
  0x801ff624 bl 802ce388 <Free__7CMemoryFPCv>                        (same word in b)
```

A second scratch check masked opcode+LK+AA of every branch in both word streams:
**"differing after masking branch disp+LK+AA: 0"**, i.e. the two ranges are the same code with
two call targets swapped. So the source is
`src/MetroidPrime/ScriptObjects/Carve801FF5A0.cpp` (Matching, 0x801FF5A0..0x801FF720) with
`fn_801FEE40` -> `fn_801FEA98` and `fn_801FD638` -> `fn_801FD8E0` and the four names of the
functions themselves changed. **No spelling search was needed and none was done**: the unit was
byte-exact on the first compile (`carve_diff` below), which is what "twins" is worth here.

The `.cpp`-not-`.c` conclusion is the sibling's, and it is measured on the sibling, not here -
C-mode CSE of the two `x0c` loads and its register allocation for the destroy loop are what the
sibling's own header records (42 instructions instead of 43; 6 of 19 words off). Every definition
here is `extern "C"`, so the four symbols stay unmangled and `nm` shows four `T fn_801FF<addr>`.

## Files (the carve is four, plus the one port file the gate forces)

| file | change |
| --- | --- |
| `src/MetroidPrime/ScriptObjects/Carve801FF720.cpp` | **new**, 159 lines; definitions descending by address (`fn_801FF838`, `fn_801FF7EC`, `fn_801FF7CC`, `fn_801FF720`) |
| `configure.py:743` | `Object(Matching, "MetroidPrime/ScriptObjects/Carve801FF720.cpp"),` one line, address order directly after `Carve801FF5A0.cpp` |
| `config/G2ME01/splits.txt:1174-1175` | `.text start:0x801FF720 end:0x801FF8A0`, between `Carve801FF5A0.cpp` and `ScriptLoader/SpacePirate.cpp` |
| `files.cmake:565` | `src/MetroidPrime/ScriptObjects/Carve801FF720.cpp`, next to the other carve entries |
| `src/MetroidPrime/PortLinkStubs.cpp:767-787` | `stub_183` for `fn_801FEA98`, `stub_184` for `fn_801FD8E0`, and the header's own counts (line 8 `156` -> `158`, line 12 `152` -> `154`) |

No duplicate existed: before the stubs were added, `grep -rn "fn_801FEA98\|fn_801FD8E0"
src/ include/` found nothing at all, and after the only non-stub hits are this new file's own
comments and prototypes. `stub_183`/`stub_184` do not collide with
`PortReachStubs.cpp`'s `reachstub_183` (different prefix); `gate.sh`'s `port link dups` reports
0 duplicates.

### The fifth file, and the stub numbers on THIS base

`fn_801FEA98` (0x801FEA98, 0x20, `symbols.txt:8311`) and `fn_801FD8E0` (0x801FD8E0, 0x20,
`symbols.txt:8279`) are two of the seven `bl`s in the claimed bytes, so no spelling can drop
them, and nothing in the tree defines them. Measured before the stubs existed:

```
build/probe-logs/link_check.log:  NEW  fn_801FD8E0   /   NEW  fn_801FEA98
./tools/probe_sources.sh:  probe: 772 files, 0 failed, 0 errors; link: NOT LINKED (293 undefined, 0 duplicates)
```

`stub_182` on this base is **`fn_80008D68`**, landed by `carve-800045a0` (`ddbfb010`), so the two
new ones are `stub_183` / `stub_184` - not 182/183. Each is the MWCC `destroy<T>(T*)`/copy
forwarder shape, one `bl` to `fn_801FD900` / `fn_801FEAB8`, so carving them instead only moves the
gap one function along: the stub is the right trade and the comment on each says so. After:

```
probe: 772 files, 0 failed, 0 errors; link: LINKED (291 undefined, 0 duplicates)
link_check: unique undefined symbols 291 / duplicate definitions 0 / unchanged from baseline
```

**Whoever merges several carve lanes must still renumber** - the header's own history line shows
this file has carried four hand-added stub numbers across two days.

## Verification (every number measured in this run)

```
./tools/decomp_build.sh
    All:  35.44% fuzzy, 29.25% matched, 12.97% linked (12575 / 28465 functions)
sha1sum build/G2ME01/main.dol
    6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
python3 tools/check_decl_order.py --unit MetroidPrime/ScriptObjects/Carve801FF720
    ok: 1 unit(s) checked, none emits its functions out of retail order
    (the argument takes no `.cpp` suffix: with it the tool matches 0 units and prints
     "ok: 0 unit(s) checked", which looks like a pass and checks nothing)
./tools/unit_fit.sh MetroidPrime/ScriptObjects/Carve801FF720.cpp
    .text claimed 384 ours 384 retail 384 fits
    no extra functions: our object defines only what the retail unit object does
python3 tools/carve_diff.sh 801ff720 180 build/G2ME01/obj/MetroidPrime/ScriptObjects/Carve801FF720.o
    retail: 96 instructions, 384 bytes / ours: 96 instructions, 384 bytes
    differing instructions: 7   <- exactly the seven `bl`s (0x801FF750, 0x801FF788, 0x801FF79C,
                                    0x801FF7A4, 0x801FF7D8, 0x801FF810, 0x801FF868), all unlinked
    (scratch re-check masking every branch's disp+LK+AA: "differing: 0")
powerpc-eabi-nm --numeric-sort build/G2ME01/obj/MetroidPrime/ScriptObjects/Carve801FF720.o
    00000000 T fn_801FF720   000000ac T fn_801FF7CC
    000000cc T fn_801FF7EC   00000118 T fn_801FF838     <- ascending = retail's order
    and only these four T; the four callees are U
./tools/flip_test.sh MetroidPrime/ScriptObjects/Carve801FF720.cpp
    PASS -> kept as Matching   (kept 1/1, failed 0, skipped 0)
python3 tools/check_symbol_names.py
    checked 528 units; 0 declared names are missing from their object
build/gate-diff.log
    SPLIT   main/auto_03_801FF720_text: 30 function(s) accounted for across 2 new unit(s) in main
            (exact count match - a split, not a loss)
    matched  12571 -> 12575   linked 5934 -> 5938   (+4 functions at 100%, 1 units newly linked)
    LINKED  main/MetroidPrime/ScriptObjects/Carve801FF720
    +100%   ... :: fn_801FF720 / fn_801FF7CC / fn_801FF7EC / fn_801FF838
    no regression
```

`auto_03_801FF720_text` re-split around the claim: the report now has the new Matching unit
(0x801FF720..0x801FF8A0, 4 functions, 384 bytes, 4/4) and `main/auto_03_801FF8A0_text`
(0x801FF8A0.., **26 functions, 5488 bytes**). The claim cuts neither end of the old auto unit's
`@stringBase0` pool, and `gate.sh`'s per-function diff prints `no regression`.

## What is still there, for the next lane

* `auto_03_801FF8A0_text`, 0x801FF8A0..: **26 functions, 5488 bytes**. The twin-pairing keeps
  paying: the 0xAC/0x20/0x4C/0x68 run repeats at 0x801FF8A0 and 0x801FFA20, and the helper
  triplet stands alone at 0x801FF94C/0x801FF96C/0x801FF9B8 and 0x801FFACC/0x801FFAEC/0x801FFB38.
  **The next 0x180 is 0x801FF8A0..0x801FFA20** and its only new element callees will be a fresh
  pair of 0x20-byte forwarders, so budget two more `stub_18x` numbers - and re-derive the highest
  one, because `stub_182` was taken in between two attempts of this item.
* `fn_801FE97C` (0x801FE97C, 0xB0 bytes, `symbols.txt:8309`) is `fn_801FF720`'s only caller and
  is still unclaimed; it calls it as `reserve(self, capacity)`.
* `auto_03_801FF4C4_text`, 0x801FF4C4..0x801FF5A0: 1 function, 220 bytes - the run below.

## Caveats

- **The failure mode of this item is the rebase, not the code.** Two attempts passed the judge and
  were released when `goal/decomp` moved under them (`configure.py`, `splits.txt`, `files.cmake`
  and `PortLinkStubs.cpp` all conflicted - the last one because every carve lane appends stub
  numbers at the same two places in the file). If this attempt is released the same way, the next
  run should re-apply the same five edits, not search for a different spelling: the object has
  been byte-exact on the first compile all three times.
- `docs/HANDOFF.md` and `docs/RUNNING_THE_DECOMP.md` show as modified in `git status`: that is
  **`goal_check.sh`/`gate.sh` rewriting their own derived counts** (matched 12571 -> 12575,
  linked 5934 -> 5938), not a lane edit. No `tools/` file and nothing under `build/goal/` but this
  notes file was touched.
- The two port stubs are stand-ins, not decompilation, and their comment says so;
  `fn_801FEA98` and `fn_801FD8E0` stay in `docs/research/port_link_gap.md`'s accounting as
  unclaimed DOL code.
- The block struct is spelled locally (`SCarve801FF720Block`) rather than by including a header:
  retail's class at this address is not `CGameState`'s, and `x00` is read and written by nothing
  in the DOL, so nothing pins it. `SStateIter` is duplicated from the sibling rather than shared
  for the same reason.
- No `WALL:` line: nothing here sat below 100%, in this run or in either earlier one.
