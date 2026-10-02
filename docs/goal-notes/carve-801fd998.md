# carve-801fd998 - `MetroidPrime/ScriptObjects/Carve801FD998` -> `Matching`

**Kind:** `match` **Target:** `MetroidPrime/ScriptObjects/Carve801FD998` **Result: PASS**
(`goal_check: PASS carve-801fd998`, verbatim in "Verification" below).

Five files. Nothing else in the tree changed; `git status --porcelain --untracked-files=all` at the
end lists exactly those five.

## What I did

- **`src/MetroidPrime/ScriptObjects/Carve801FD998.c`** (new, plain C, 1 function): `fn_801FD998`,
  the teardown of a block that owns an array of 0x2C-byte script-object elements - walk the array
  through `fn_801FDA1C`, `Free__7CMemoryFPCv(x0c_buffer)`, then `Free__7CMemoryFPCv(self)` behind
  `flag > 0`, returning `self`. The spelling is `Carve801FBC58.c`'s `fn_801FBCAC` (a `Matching`
  unit) verbatim - same struct shape, same `volatile firstCopy`/`lastCopy`, same `end` temporary
  that **both** copies are assigned from, same `&first`/`&last` passed - with the `mulli` immediate
  44 and the callee `fn_801FDA1C`.
- **`configure.py:775`** - `Object(Matching, "MetroidPrime/ScriptObjects/Carve801FD998.c"),`
  between `Carve801FD924.cpp` (774) and `Carve801FDAA4.c` (now 776).
- **`config/G2ME01/splits.txt:1374-1375`** - `MetroidPrime/ScriptObjects/Carve801FD998.c:` /
  `.text start:0x801FD998 end:0x801FDA1C`, between the `Carve801FD924.cpp` entry (1371-1372, ends
  0x801FD998) and the `Carve801FDAA4.c` entry (now 1377-1378). No gap is spanned: both boundaries
  are function edges, below `fn_801FD924` (`symbols.txt:8281`) and above `fn_801FDA1C`
  (`symbols.txt:8283`).
- **`files.cmake:582`** - after `Carve801FD924.cpp` (581), before `Carve801FDAA4.c` (now 583),
  keeping the carve list in address order.
- **`src/MetroidPrime/PortLinkStubs.cpp:1423-1446`** - one stand-in for the carve's only new
  callee, `stub_801fd998_0() asm("fn_801FDA1C")` with an empty body. This is the port cost the
  item's `reason` predicted and nothing else retires. It is named after the unit rather than
  numbered (`stub_NNN`), which is the convention the `stub_801e515c_0` block's own paragraph
  gives: a numbered stub can be taken by another lane between the judge and the rebase, and that
  has already thrown this file's carry away once (`docs/goal-notes/carve-8023289c.md` records it).
  That block's instruction to leave the file's **header numerals** alone was followed too - see
  the last section.

## Re-measured, this tree (the `reason` was a starting point, not a measurement)

- `config/G2ME01/symbols.txt:8282` `fn_801FD998 = .text:0x801FD998; // type:function size:0x84`;
  `:8283` `fn_801FDA1C ... size:0x38`; `:8284` `fn_801FDA54 ... size:0x50`.
- The 132 bytes, read this run out of the disc and not out of the built ELF:
  `python3 tools/dol_read.py 0x801FD998 0x84` -> `.text @ 0x801fd998 (file 0x1fa798, 132 bytes)`,
  33 instruction words. The same 33 are dtk's at
  `build/G2ME01/asm/auto_03_801FD998_text.s:8` (`# .text:0x0 | 0x801FD998 | size: 0x84`).
- **The twin claim, checked rather than repeated.** `fn_801FD6F0` (0x801FD6F0, 0x84, unclaimed,
  dtk's `auto_03_801FD67C_text.s:43`) and `fn_801FD998` differ in exactly **three** of the 33
  words: `mulli r0,r0,0x14` against `mulli r0,r0,0x2c` at +0x30, and the two
  `bl Free__7CMemoryFPCv` displacements at +0x54 and +0x64. The displacement difference is exactly
  0x2A8, the distance between the two functions' addresses - position, not semantics.
- **The shape is a matched unit's, not a guess.** `Carve801FBC58.c`'s `fn_801FBCAC` is those 33
  instructions with `0xc` for `0x2c`, and *our own compile* of it
  (`build/G2ME01/asm/MetroidPrime/ScriptObjects/Carve801FBC58.s`, `.fn fn_801FBCAC`) is
  instruction-for-instruction the listing of `fn_801FD998` with `0x2c` for `0xc` and the three
  `bl` targets differing. That is why the two load-bearing spellings
  (`volatile firstCopy`/`lastCopy`; `end` before both copy assignments; `&first`/`&last` passed)
  were copied instead of re-derived - `Carve801FBC58.c:56-74` measures what each of them is worth.
- **The stride is 44, measured independently of the `mulli` it fixes.** The walk this function
  forwards to, `fn_801FDA54` (0x801FDA54, 0x50, dtk's `auto_03_801FD998_text.s:64`), steps
  `addi r31,r31,0x2c` and calls `fn_801FDAA4` on each element; and `Carve801FDAA4.c` - a `Matching`
  unit - fixes 0x2C as the element size from that walk plus `fn_801FF96C` in the `Matching`
  `Carve801FF8A0.cpp`, and identifies `fn_801FDAA4`/`fn_801FDAC4` as `rstl::destroy<T>` /
  `destroy_impl<T>` for a 0x2C-byte element whose deleting destructor is `fn_801FDAE8`, claimed
  by the `Matching` `Carve801FDAE8.cpp`.
- **The caller.** The only `bl fn_801FD998` in the DOL is at 0x801FD4E4
  (`build/binutils/powerpc-eabi-objdump -d build/G2ME01/main.elf | grep 'bl.*801fd998'` -> one
  hit), inside `fn_801FD4B0` (0x801FD4B0, 0x7C, unclaimed, dtk's `auto_03_801FBD68_text.s:1750`):
  `addi r3,r30,0x20` / `li r4,-1` / `bl fn_801FD998`, the middle of four member teardowns at
  +0x10/+0x20/+0x30. So the flag arrives as MWCC's "destroy, do not free me afterwards", which is
  what `Carve801FD924.cpp:154-162` records for the twin, and the receiver is a member at +0x20 of
  an unnamed class.
- **The split behaved as it does.** dtk's `auto_03_801FD998_text.s` held
  `# 0x801FD998..0x801FDAA4 | size: 0x10C` with three `.fn` blocks (`0x84`, `0x38`, `0x50`).
  After the carve, `build/report.json` has `main/MetroidPrime/ScriptObjects/Carve801FD998`
  (`100.0`, 132/132 bytes, 1/1 functions) and `main/auto_03_801FDA1C_text` (136 bytes, **2**
  functions); `main/auto_03_801FD998_text` is gone. `fn_801FDA1C` and `fn_801FDA54` stay retail's,
  which is why the DOL still reproduces and why `fn_801FDA1C` needs a port stand-in.
- **`total_functions` is still 28465** after the `splits.txt` edit.

## Verification

`./tools/goal_check.sh build/goal/item.json`, verbatim:

```
goal_check: item carve-801fd998 (match) target=MetroidPrime/ScriptObjects/Carve801FD998
goal_check: baseline .../wt-mp2-goal-L6/build/goal/judge/report.base.json
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 13043 -> 13044   linked 6146 -> 6147
  ok    check_symbol_names.py
  ok    All:  36.98% fuzzy, 30.42% matched, 13.40% linked (13044 / 28465 functions)
  ok    flip_test MetroidPrime/ScriptObjects/Carve801FD998.c: PASS, Object(Matching) in configure.py
goal_check: PASS carve-801fd998
```

Measured directly as well:

- `./tools/flip_test.sh MetroidPrime/ScriptObjects/Carve801FD998.c` - `PASS -> kept as Matching`,
  `kept: 1 / 1  failed: 0  skipped: 0`.
- `./tools/unit_fit.sh MetroidPrime/ScriptObjects/Carve801FD998.c` -
  `.text claimed 132 ours 132 retail 132 fits`, `no extra functions`.
- `sha1sum build/G2ME01/main.dol` -> `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`; build printed
  `87 files OK`. `gate.sh`'s independent re-hash against `config/G2ME01/config.yml` passed inside
  the judge, so all 86 RELs are unchanged.
- `build/report.json` `measures`: `matched_functions` 13043 -> **13044**, `complete_units`
  819 -> **820**, `total_units` 2147 -> **2148**, `fuzzy_match_percent` 36.974903 ->
  36.976925, `total_functions` **28465** unmoved.
- **Neighbours did not move**: `Carve801FD924` still 100.0 / 116/116 bytes / 1/1 functions,
  `Carve801FDAA4` 100.0 / 68/68 / 2/2, `Carve801FDAE8` 100.0 / 116/116 / 1/1, `Carve801FDB5C`
  100.0 / 168/168 / 3/3, `Carve801FD8E0` and `Carve801FD638` and `Carve801FD5E8` unchanged.
- **Byte comparison against the disc, per word.** Comparing the 33 `dol_read.py` words with the 33
  words of `build/G2ME01/obj/MetroidPrime/ScriptObjects/Carve801FD998.o`:
  **30 of 33 identical**; the three that differ are `0x801fd9e4`, `0x801fd9ec`, `0x801fd9fc`, all
  of them `bl` words reading `48000001` where retail has the target - which is what
  `powerpc-eabi-objdump -r -j .text` shows them to be: exactly three relocations and no others,
  `R_PPC_REL24 fn_801FDA1C`, `R_PPC_REL24 Free__7CMemoryFPCv` twice. `powerpc-eabi-nm -S` shows
  `T fn_801FD998` size `00000084` and the two undefined names unmangled.
  (`tools/carve_diff.sh 801FD998 132 <o>` prints `NOT byte-exact` for the same reason and adds 43
  phantom "ours: <none>" lines: it disassembles `main.elf`, whose `--stop-address` overrun past
  0x801FDA1C prints the next function. Not a defect of the carve - `unit_fit.sh`, objdiff's
  132/132 and the DOL sha1 are the three instruments that decide this.)
- `python3 tools/check_symbol_names.py` - `checked 577 units; 0 declared names are missing from
  their object`.
- `python3 tools/check_decl_order.py --unit MetroidPrime/ScriptObjects/Carve801FD998.c` -
  `ok: 0 unit(s) checked` (it matches `.cpp` names only; moot for one function, and `flip_test.sh`
  is the check that matters, and it passed).
- **The port**: `gate.sh`'s `port probe` and `port link gap` both passed.
  `build/gate-probe.log`: `821 files, 0 failed, 0 errors; link: LINKED (286 undefined, 0
  duplicates)`. `build/gate-link.log`: `280 MISSING` and `ok: 280 MISSING symbol(s), all accounted
  for in port_link_gap_list.md`, and `build/gate-linkcheck.log`: `duplicate definitions 0`.
  The stand-in is load-bearing: without it `link_gap.py` would report
  `gap grew: fn_801FDA1C is not in port_link_gap_list.md` and exit 1, because the carve's
  `bl fn_801FDA1C` is in retail's bytes and cannot be dropped. `docs/research/port_link_baseline.txt:4`
  still reads `undefined 287` while this tree measures **286** - that is the branch head's state,
  not this change's (the driver's own judge baseline, `build/goal/judge/undef.base.count`, is 286),
  and that file is one this item may not edit, so it is left for the driver to restate.
- `gate.sh` rewrote `docs/HANDOFF.md` and `docs/RUNNING_THE_DECOMP.md` as a side effect, as it
  does for every lane; `git checkout --` on exactly those two files left the tree at the five
  files of this change.
- **The judge's run predates two comment-only edits** to the new file's header (two wrong
  `build/G2ME01/asm/*.s` line numbers, and one sentence that named the pre-split listing
  `auto_03_801FD998_text.s` without saying this carve is what retires it). Re-verified after them,
  on the final tree: `flip_test` `PASS` again, build `87 files OK`, `main.dol` sha1 unchanged at
  `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`, `All: 36.98% fuzzy, 30.42% matched, 13.40% linked
  (13044 / 28465 functions)`. The judge was not re-run end to end - that is another full port
  build - so a reader who wants the judge on the exact final bytes should run
  `./tools/goal_check.sh build/goal/item.json` once more; nothing but comments changed.

## Left alone on purpose

`src/MetroidPrime/PortLinkStubs.cpp`'s header paragraph carries the file's function/data/total
counts (188 / 8 / 196 as of the `Carve801FDAE8.cpp` carve). This change adds one function stand-in
without retiring one, so those numerals are each one low now. That paragraph's own instruction -
twice - is that whoever next edits it does so on an item's tree where a rebase cannot break the
carry, and it is not this item's business: `docs/goal-notes/carve-8023289c.md` records the carry
dying on exactly those lines. **The numbers to derive, measured on this tree after this change:**
`grep -cE '^extern "C" void stub[^ ]*\(\) asm\("'` is **189** function stubs and
`grep -cE '^extern "C" char stub_data_[0-9]+\[[0-9]+\] asm\("'` is **8** data stubs, 197 in all
(`grep -cE 'asm\("'` = 197, of which `grep -cE 'asm\("(fn_|lbl_)'` = 37 are unmangled names). So the
paragraph's 188 / 8 / 196 wants 189 / 8 / 197 - the function term and the total, nothing else.

## NEW:

NEW: carve-801fda1c | match | MetroidPrime/ScriptObjects/Carve801FDA1C | carve `fn_801FDA1C` and `fn_801FDA54` together (`.text` 0x801FDA1C..0x801FDAA4, 0x88 = 136 bytes, `symbols.txt:8283-8284`, the two `.fn` blocks of dtk's `auto_03_801FDA1C_text.s` - which this item's split created). `fn_801FDA1C` (0x38) is `fn_801FBD30`'s twin instruction for instruction (`lwz r5,0(r4)` / `addi r4,r1,8` / `lwz r0,0(r3)` / `addi r3,r1,0xc` / `bl`), and `fn_801FDA54` (0x50) is the element walk the `Matching` `Carve801FBC58.c` documents for its own pair; both are the shapes that unit matches. Its one callee, `fn_801FDAA4`, is already claimed and `Matching` (`Carve801FDAA4.c`), so this carve needs **no new stand-in** and retires `stub_801fd998_0` when it lands. Contiguous on both sides: `Carve801FD998.c` ends at 0x801FDA1C and `Carve801FDAA4.c` starts at 0x801FDAA4.

NEW: carve-801fd4b0 | match | MetroidPrime/ScriptObjects/Carve801FD4B0 | carve `fn_801FD4B0` (`.text` 0x801FD4B0..0x801FD52C, 0x7C = 124 bytes, `symbols.txt:8267`, inside dtk's `auto_03_801FBD68_text.s:1750`) - the deleting destructor that is the only caller of this item's `fn_801FD998`: four member teardowns at `addi r3,r30,+0x10/+0x20/+0x30` and `mr r3,r30`, each preceded by `li r4,-1`, then `extsh. r0,r31 / ble` and `Free__7CMemoryFPCv(self)`, receiver returned. That is the shape `Carve801FD924.cpp` already matches, so the body is nearly written; **its cost is two stand-ins**, for the unclaimed callees `fn_801FD7D4` (0x801FD7D4, 0x84, `symbols.txt:8276`) and `fn_801FD52C` (0x801FD52C, 0x84, `symbols.txt:8268`) - `fn_801FDB5C` is already ours (`Carve801FDB5C.c`, `Matching`) and `fn_801FD998` is ours as of this item. Both boundaries of that claim are unclaimed function edges (`fn_801FD420`, 0x801FD420+0x90, below; `fn_801FD52C`, above), so it is a four-file carve on its own; claiming it and `fn_801FD52C` together would need only one stand-in.

---

# carve-801fd998 - SECOND run (lane 6, 2026-10-02) - `MetroidPrime/ScriptObjects/Carve801FD998` -> `Matching`

**Kind:** `match` **Target:** `MetroidPrime/ScriptObjects/Carve801FD998` **Result: PASS**
(`goal_check: PASS carve-801fd998`, verbatim in "Verification" below).

The first run's change was **not on this tree** when this run started: `git status
--porcelain` was empty at HEAD `0fdabf38`, `build/report.json` had no
`main/MetroidPrime/ScriptObjects/Carve801FD998` unit, and `main/auto_03_801FD998_text` still
held all three functions unmatched (`total_functions 3`, no `fuzzy_match_percent`). So this is
not STALE; the work was redone. Four files changed, not five.

## What this run did differently, and why

The first run carved **`fn_801FD998` alone** (0x801FD998..0x801FDA1C, 132 bytes) and paid for it
with a new stand-in `stub_801fd998_0` in `src/MetroidPrime/PortLinkStubs.cpp` for the
then-unclaimed callee `fn_801FDA1C`. Its own `NEW: carve-801fda1c` line filed the obvious fix:
**claim `fn_801FDA1C` and `fn_801FDA54` too.** That is what this run does - and it costs nothing,
because the two extra functions' only callee (`fn_801FDAA4`) is already ours. So:

- **`src/MetroidPrime/ScriptObjects/Carve801FD998.c`** (new, plain C, 3 functions), claiming
  **0x801FD998..0x801FDAA4** = **0x10C = 268 bytes**, three functions instead of one:
  `fn_801FD998` (0x84, the block's clear), `fn_801FDA1C` (0x38, `destroy(first,last)` forwarding
  on), `fn_801FDA54` (0x50, the 0x2C-strided element walk).
- **`src/MetroidPrime/PortLinkStubs.cpp` is NOT touched at all.** No stand-in is needed: the
  only callees of the three are `fn_801FDAA4` (already claimed and `Matching` in
  `Carve801FDAA4.c`), `Free__7CMemoryFPCv` (already ours) and each other. Measured: the port
  link's undefined count is **286, the same as the judge's baseline**
  (`build/goal/judge/undef.base.count` = 286), so `PortLinkStubs.cpp`'s header numerals and
  every count in it are untouched and stay correct - which also removes the carry-die risk
  `docs/goal-notes/carve-8023289c.md` records.
- **`configure.py:777`**, **`config/G2ME01/splits.txt:1380-1381`**, **`files.cmake:584`** - the
  same three-file carve, with `Carve801FD998.c` between `Carve801FD924.cpp` and
  `Carve801FDAA4.c` in all three. The source's own claim is the header's
  `.text 0x801FD998..0x801FDAA4, 0x10C = 268 bytes, 3 functions`.

Result: **+3 matched functions** (13046 -> 13049) rather than +1, +1 complete unit, and no new
port stand-in. The first run's `NEW: carve-801fda1c` is **absorbed by this change** and is now
stale - do not queue it.

## Re-measured on this tree (the `reason` was a starting point, not a measurement)

- `config/G2ME01/symbols.txt:8282-8284`: `fn_801FD998` `.text:0x801FD998 size:0x84`,
  `fn_801FDA1C` `.text:0x801FDA1C size:0x38`, `fn_801FDA54` `.text:0x801FDA54 size:0x50`;
  `:8285` `fn_801FDAA4 size:0x20`, `:8288` `fn_801FDB5C size:0x84`, `:8289` `fn_801FDBE0
  size:0x38`.
- The 268 bytes read out of the disc, **not** out of the built ELF:
  `python3 tools/dol_read.py 0x801FD998 0x84` -> `.text @ 0x801fd998 (file 0x1fa798, 132 bytes)`.
  The same bytes are dtk's at `build/G2ME01/asm/auto_03_801FD998_text.s`, whose three `.fn`
  blocks are at lines 8-9 (`fn_801FD998`, 0x84), 46-47 (`fn_801FDA1C`, 0x38) and 64-65
  (`fn_801FDA54`, 0x50) under `# 0x801FD998..0x801FDAA4 | size: 0x10C` (line 4). That unit is
  gone from `build/report.json` after this carve; nothing replaces it, because the claim now
  covers all three.
- **All three twins re-verified word by word this run with
  `build/binutils/powerpc-eabi-objdump -d` against `main.elf`** (not recalled, not copied from
  the run above):
  - `fn_801FD998` == `fn_801FBCAC` (0x801FBCAC, 0x84, `Carve801FBC58.c`, `Matching`): 33
    instructions each, differing in exactly three words - `mulli r0,r0,0x0c` against
    `mulli r0,r0,0x2c` at +0x30, and the two `bl Free__7CMemoryFPCv` displacements at +0x54 /
    +0x64 (the gap between the two functions' addresses - position, not semantics).
  - `fn_801FDA1C` == `fn_801FBD30` (0x801FBD30, 0x38, same `Matching` unit): 14 instructions
    each, differing only in the `bl` at +0x24. This is what fixes the `const void*` parameters
    and the dereference-inside-the-argument-list spelling.
  - `fn_801FDA54` == `fn_801FF96C` (0x801FF96C, 0x4C, `Carve801FF8A0.cpp`, `Matching`) for 19 of
    its 20 instructions, and the **one word of difference is the interesting one**: retail here
    takes two by-value iterators and **re-reads the bound** (`lwz r31,0(r3)` / `mr r30,r4` /
    ... / `lwz r0,0(r30)` / `cmplw r31,r0`), while `fn_801FF96C` takes two bare `char*` and holds
    the bound in r30 (`mr r30,r4` / `cmplw r31,r30`). That reload is MWCC's by-reference pass of
    a struct that fits in a register, which is why `fn_801FDA1C` has to be claimed with it.
  - `fn_801FDA54` **also has a byte-identical twin elsewhere in the DOL**, found this run by
    searching the built `main.dol` for its 80 bytes: `fn_802559BC` (0x802559BC, 0x50,
    `symbols.txt:10486`, **unclaimed** - `splits.txt`'s nearest entry is
    `WorldFormat/Carve80255A0C` at 0x80255A0C). Left where it is; filed below.
- **The stride is 44, measured independently of the `mulli` it fixes.** `fn_801FDA54` steps
  `addi r31,r31,0x2c` at 0x801FDA7C and calls `fn_801FDAA4` on each element; and
  `src/MetroidPrime/ScriptObjects/Carve801FDAA4.c` (a `Matching` unit) already fixes 0x2C from
  that walk plus `fn_801FF96C` in the `Matching` `Carve801FF8A0.cpp`, identifying
  `fn_801FDAA4`/`fn_801FDAC4` as `rstl::destroy<T>`/`destroy_impl<T>` for a 0x2C element.
- **The callers**, from `objdump -d build/G2ME01/main.elf | grep 'bl.*<fn_801FDA1C>'` and
  `...<fn_801FD998>`: `fn_801FD998` has exactly one caller in the DOL, at 0x801FD4E4 inside
  `fn_801FD4B0` (0x801FD4B0, 0x7C, `symbols.txt:8267`, unclaimed) -
  `addi r3,r30,0x20` / `li r4,-1` / `bl fn_801FD998`, the second of four member teardowns at
  +0x10/+0x20/+0x30 and `mr r3,r30`, then `extsh. r0,r31 / ble` and `Free__7CMemoryFPCv(self)`.
  `fn_801FDA1C` has two: that one at 0x801FD9E4 and 0x801FEBD4 inside `fn_801FEB48`.
- **The split behaves as it does**: no gap is spanned. Below, `Carve801FD924.cpp` ends exactly at
  0x801FD998 (`splits.txt:1377-1378`, `symbols.txt:8281` `fn_801FD924` 0x801FD924+0x74); above,
  `Carve801FDAA4.c` starts exactly at 0x801FDAA4 (`splits.txt:1383-1384`, `symbols.txt:8285`).
- `total_functions` is still **28465** after the `splits.txt` edit.

## Verification

`./tools/goal_check.sh build/goal/item.json`, verbatim:

```
goal_check: item carve-801fd998 (match) target=MetroidPrime/ScriptObjects/Carve801FD998
goal_check: baseline .../wt-mp2-goal-L6/build/goal/judge/report.base.json
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 13046 -> 13049   linked 6149 -> 6152
  ok    check_symbol_names.py
  ok    All:  36.98% fuzzy, 30.42% matched, 13.41% linked (13049 / 28465 functions)
  ok    flip_test MetroidPrime/ScriptObjects/Carve801FD998.c: PASS, Object(Matching) in configure.py
goal_check: PASS carve-801fd998
```

Measured directly as well:

- `./tools/flip_test.sh MetroidPrime/ScriptObjects/Carve801FD998.c` - `PASS -> kept as Matching`,
  `kept: 1 / 1  failed: 0  skipped: 0`.
- `./tools/unit_fit.sh MetroidPrime/ScriptObjects/Carve801FD998.c` -
  `.text claimed 268 ours 268 retail 268 fits`, `no extra functions: our object defines only what
  the retail unit object does`.
- `sha1sum build/G2ME01/main.dol` -> `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`; the build printed
  `All: 36.98% fuzzy, 30.42% matched, 13.41% linked (13049 / 28465 functions)`. `gate.sh`'s
  independent re-hash of all 86 RELs against `config/G2ME01/config.yml` passed inside the judge.
- `build/report.json`: `matched_functions` 13046 -> **13049**, `complete_units` 822 -> **823**,
  `total_units` 2151 -> 2151 (dtk's `auto_03_801FD998_text` is replaced by our unit, so the
  total does not move), `fuzzy_match_percent` 36.978450 -> 36.982555, `total_functions`
  **28465** unmoved. New unit: `main/MetroidPrime/ScriptObjects/Carve801FD998`, `100.0`
  fuzzy, `268/268` bytes, **3/3** functions.
- **Nothing anywhere got worse** - compared unit by unit against
  `build/goal/judge/report.base.json` on `matched_functions` and `complete_code`: **none**. The
  neighbours are all still 100.0 and unmoved: `Carve801FD924` 116/116 1/1,
  `Carve801FDAA4` 68/68 2/2, `Carve801FDAE8` 116/116 1/1, `Carve801FDB5C` 168/168 3/3,
  `Carve801FD8E0` 68/68 2/2, `Carve801FBC58` 272/272 3/3, `Carve801FF8A0` 384/384 4/4,
  `WorldFormat/Carve80255A0C` 284/284 4/4. `main/auto_03_801FD998_text` and
  `main/auto_03_801FDA1C_text` are ABSENT from the new report, as expected.
- **Byte comparison against the disc, per word.** Comparing the 268 disc bytes with the 268
  bytes of `build/G2ME01/obj/MetroidPrime/ScriptObjects/Carve801FD998.o`'s `.text`
  (file offset 0x40, size 0x10c): **64 of 68 words identical**; the 5 differing words are the
  5 `bl` instructions reading `48000001` where retail has the target, and
  `powerpc-eabi-objdump -r -j .text` shows exactly those five relocations and no others -
  `R_PPC_REL24 fn_801FDA1C`, `R_PPC_REL24 Free__7CMemoryFPCv` twice, `R_PPC_REL24
  fn_801FDA54`, `R_PPC_REL24 fn_801FDAA4`. `powerpc-eabi-nm -S` shows `T fn_801FD998`
  0x84, `T fn_801FDA1C` 0x38, `T fn_801FDA54` 0x50, in that order, and `U` only for
  `fn_801FDAA4` and `Free__7CMemoryFPCv`.
- `python3 tools/check_symbol_names.py` - `checked 578 units; 0 declared names are missing from
  their object`.
- `./tools/probe_sources.sh` - `probe: 824 files, 0 failed, 0 errors; link: LINKED (286 undefined,
  0 duplicates)`. **286 is the judge's baseline count**, so this carve adds nothing to the port's
  undefined list - the measurement that says the stand-in is genuinely unnecessary.
  (`docs/research/port_link_baseline.txt:4` still reads `undefined 287`, which is the branch
  head's state and not this change's; that file is one this item may not edit.)
- `python3 tools/check_decl_order.py --unit MetroidPrime/ScriptObjects/Carve801FD998.c` - it
  matches `.cpp` names only, so it says nothing here; `flip_test.sh` passing with the DOL sha1
  unchanged is the check that decides source order, and the file is written descending by
  address (`fn_801FDA54`, then `fn_801FDA1C`, then `fn_801FD998`).
- `gate.sh` rewrote `docs/HANDOFF.md` and `docs/RUNNING_THE_DECOMP.md` as a side effect, as it
  does for every lane; `git checkout --` on exactly those two files left the tree at the four
  files of this change (`git status --porcelain --untracked-files=all`: ` M config/G2ME01/
  splits.txt`, ` M configure.py`, ` M files.cmake`, `?? src/MetroidPrime/ScriptObjects/
  Carve801FD998.c`).

## Supersedes the run above

- `NEW: carve-801fda1c` (the run above) is **done by this change**. Do not queue it; the
  functions are `main/MetroidPrime/ScriptObjects/Carve801FD998` at 100.0, 3/3.
- The run above's "`stub_801fd998_0`" and its "Left alone on purpose" section about
  `PortLinkStubs.cpp`'s header numerals (188/8/196 wanting 189/8/197) are **moot**: this carve
  adds no stub. That file is untouched; measured on this tree it holds **187** function stubs,
  **9** data stubs, **196** in all, of which **36** are unmangled (`fn_`/`lbl_`) names.
- **Correction to the run above's second `NEW:` line.** It says `fn_801FDB5C` "is already ours
  (`Carve801FDB5C.c`, `Matching`)". It is not: `fn_801FDB5C` is at 0x801FDB5C..0x801FDBE0
  (`symbols.txt:8288`, 0x84) and `splits.txt:1390` claims `Carve801FDB5C.c` from **0x801FDBE0**,
  not from 0x801FDB5C. So carving `fn_801FD4B0` costs **three** stand-ins (`fn_801FDB5C`,
  `fn_801FD7D4`, `fn_801FD52C`), not two - or none, if `fn_801FDB5C` is carved first, which the
  item below makes a one-function job.

## NEW:

NEW: carve-801fdb5c | match | MetroidPrime/ScriptObjects/Carve801FDB5C | carve `fn_801FDB5C` (`.text` 0x801FDB5C..0x801FDBE0, 0x84 = 132 bytes, 33 instructions, `symbols.txt:8288`, unclaimed) - the **third** twin of the shape this item just matched: `fn_801FD998` with the stride changed to 0x30 (`mulli r0,r0,48` at +0x30 against 44), everything else the same 31 words, and the walk handed to `bl fn_801FDBE0`. Its body is written: copy this item's `fn_801FD998` with `44` -> `48` and the callee -> `fn_801FDBE0`, which is **already claimed and `Matching`** (`Carve801FDB5C.c`, `splits.txt:1390`, 0x801FDBE0..0x801FDC88), so this carve needs **no new stand-in** at all. Both boundaries are claimed-unit edges: `Carve801FDAE8.cpp` ends exactly at 0x801FDB5C and `Carve801FDB5C.c` starts exactly at 0x801FDBE0, so it is a clean four-file carve with no gap. This is also what removes one of the three stand-ins the re-filed `carve-801fd4b0` needs.

NEW: carve-801fd4b0 | match | MetroidPrime/ScriptObjects/Carve801FD4B0 | **re-filed with the corrected cost** (the run above filed it at two stand-ins; it is three, or none once `carve-801fdb5c` lands). Carve `fn_801FD4B0` (`.text` 0x801FD4B0..0x801FD52C, 0x7C = 124 bytes, `symbols.txt:8267`, unclaimed) - the deleting destructor that is the only caller of this item's `fn_801FD998`: `addi r3,r30,+0x10/+0x20/+0x30` and `mr r3,r30`, each preceded by `li r4,-1`, then `extsh. r0,r31 / ble` and `Free__7CMemoryFPCv(self)`, receiver returned. That is the shape `Carve801FD924.cpp` already matches, so the body is nearly written. Callees: `fn_801FD998` (ours as of this item), `fn_801FDB5C` (0x801FDB5C, unclaimed - `carve-801fdb5c` above), `fn_801FD7D4` (0x801FD7D4, 0x84, `symbols.txt:8276`, unclaimed) and `fn_801FD52C` (0x801FD52C, 0x84, `symbols.txt:8268`, unclaimed). Both boundaries of the claim are unclaimed function edges (`fn_801FD420`, 0x801FD420+0x90, below; `fn_801FD52C`, above), so it is a four-file carve on its own.

---

# carve-801fd998 - THIRD run (lane 11, 2026-10-02) - `MetroidPrime/ScriptObjects/Carve801FD998` -> `Matching`

**Kind:** `match` **Target:** `MetroidPrime/ScriptObjects/Carve801FD998` **Result: PASS**
(`goal_check: PASS carve-801fd998`, verbatim under "Verification"). Five files changed: the four
of the carve plus `src/MetroidPrime/PortLinkStubs.cpp`, whose `stub_229` this change **retires**.

## Not STALE - the item was re-done because neither earlier run's change is on this tree

Measured at the start: `git status --porcelain` showed only the two doc files `gate.sh` rewrites
for every lane, and `build/report.json` had **no** `main/MetroidPrime/ScriptObjects/Carve801FD998`
unit while dtk's `main/auto_03_801FD998_text` still held `fn_801FD998` (1 function, 132 bytes,
no `fuzzy_match_percent` = unmatched). What the tree *does* have is a **hybrid the two earlier
notes do not describe**, and it changes the shape of the job:

- `src/MetroidPrime/ScriptObjects/Carve801FDA1C.c` (0x801FDA1C..0x801FDAA4, 2 functions,
  `Matching`) has landed - run 2's `NEW: carve-801fda1c`, done as its own item.
- `src/MetroidPrime/ScriptObjects/Carve801FD4B0.cpp` (0x801FD4B0..0x801FD52C, `Matching`) has
  landed, and with it `stub_228`..`stub_231` in `src/MetroidPrime/PortLinkStubs.cpp`: the port
  **already** stands in for `fn_801FD998` (`stub_229`), because that unit's `bl fn_801FD998` is in
  retail's bytes.
- `config/G2ME01/splits.txt:1383-1384` claims `Carve801FDA1C.c` from 0x801FDA1C, so the only
  unclaimed range here is exactly **0x801FD998..0x801FDA1C = 0x84 = 132 bytes, 1 function**.

So run 1's five-file version is wrong twice on this tree (no new stand-in is needed - the symbol is
already stubbed, and this change removes it) and run 2's three-function version is impossible (it
would overlap `Carve801FDA1C.c`). This run is **one function, the four carve files, plus the stub
retirement**.

## What this run did

- **`src/MetroidPrime/ScriptObjects/Carve801FD998.c`** (new, plain C, 1 function): `fn_801FD998`,
  `.text 0x801FD998..0x801FDA1C`, 0x84 = 132 bytes, 33 instructions, declared descending by address
  (trivially - one function). The body is `ScriptObjects/Carve801FBC58.c`'s `Matching`
  `fn_801FBCAC` (0x801FBCAC, 0x84) with the stride changed from 0xC to 0x2C and the callee from
  `fn_801FBD30` to `fn_801FDA1C`, and the callee declared with **pointer** parameters
  (`void fn_801FDA1C(const void* first, const void* last)`), which is what makes the call site pass
  `&first`/`&last` in r3/r4 instead of building struct copies. That declaration is the load-bearing
  choice, and it is the one `Carve801FBC58.c` measured for its own twin.
- `configure.py:778`, `config/G2ME01/splits.txt:1383-1384`, `files.cmake:585` - the other three
  files of the carve, `Carve801FD998.c` between `Carve801FD924.cpp` and `Carve801FDA1C.c` in all
  three. `total_functions` is still **28465** after the `splits.txt` edit. No gap is spanned:
  below, `Carve801FD924.cpp` ends exactly at 0x801FD998; above, `Carve801FDA1C.c` starts exactly
  at 0x801FDA1C.
- **`src/MetroidPrime/PortLinkStubs.cpp`**: `stub_229` (the `fn_801FD998` stand-in added by the
  `Carve801FD4B0.cpp` carve) is gone, the block comment above `stub_228`/`stub_230`/`stub_231` is
  corrected (it claimed `fn_801FDBE0` "is *not* claimed" and listed `fn_801FDA1C` among the
  unclaimed inner walks - both are claimed and `Matching` now), and the header numerals move
  **191 functions / 9 data / 200 total -> 190 / 9 / 199**.

## Measured this run (never recalled)

- `config/G2ME01/symbols.txt:8282` `fn_801FD998 = .text:0x801FD998; size:0x84`; `:8283`
  `fn_801FDA1C size:0x38`; `:8284` `fn_801FDA54 size:0x50`; `:8285` `fn_801FDAA4 size:0x20`.
- **The twin, word by word, read out of the disc** (`orig/G2ME01/sys/main.dol`, file offset
  0x1fa798 for 0x801FD998; both ranges are retail's bytes): 33 words each, **30 identical**, three
  differing -
  `+0x30` `mulli r0,r0,0x2c` vs `mulli r0,r0,0x0c`; `+0x54` and `+0x64` the two
  `bl Free__7CMemoryFPCv` words (same instruction, displacements 0x1CEC apart - the distance
  between the two functions). The `bl` to the inner walk is the **same word** in both (`48000039`),
  because each walk sits 0x84 past its function's start (`fn_801FBD30` for `fn_801FBCAC`,
  `fn_801FDA1C` for `fn_801FD998`). Nothing else differs.
- **Our object against the disc**: `build/G2ME01/obj/MetroidPrime/ScriptObjects/Carve801FD998.o`
  `.text` (file 0x40, size 0x84) - **30 of 33 words identical**; the three differing words are the
  three `bl`s holding relocation placeholders, and `objdump -r -j .text` shows exactly three
  relocations and no others (`R_PPC_REL24 fn_801FDA1C` at 0x4c, `R_PPC_REL24 Free__7CMemoryFPCv`
  at 0x54 and 0x64). `nm -S`: `T fn_801FD998` size 0x84, undefined only `fn_801FDA1C` and
  `Free__7CMemoryFPCv`.
- **The stride 44 is measured independently of the `mulli` it fixes**: `Carve801FDA1C.c`'s
  `fn_801FDA54` (that unit is `Matching`, so its bytes are retail's) steps `addi r31,r31,0x2c` per
  element, and `Carve801FD4B0.cpp:70` tabulates the same 0x2C for this address.
- The only `bl fn_801FD998` in the DOL is at **0x801FD4E4**, inside `fn_801FD4B0`
  (`addi r3,r30,0x20` / `li r4,-1`), which is `Matching` too. So the flag arrives as MWCC's
  "destroy, do not free me afterwards".
- **The stub had to go, measured rather than assumed.** With `stub_229` put back on this tree,
  `./tools/link_check.sh` printed `unique undefined symbols 285` **and `duplicate definitions 1` /
  `DUP fn_801FD998`**; with it retired the same command prints 285 / **0 duplicates**. The
  five-file version (carve + stub kept) would have failed the gate's `link-dups` step.

## Verification

`./tools/goal_check.sh build/goal/item.json`, verbatim (final tree, i.e. after the scratch
experiment below was reverted and the tree rebuilt):

```
goal_check: item carve-801fd998 (match) target=MetroidPrime/ScriptObjects/Carve801FD998
goal_check: baseline .../wt-mp2-goal-L11/build/goal/judge/report.base.json
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 13053 -> 13054   linked 6163 -> 6164
  ok    check_symbol_names.py
  ok    All:  37.00% fuzzy, 30.44% matched, 13.43% linked (13054 / 28465 functions)
  ok    flip_test MetroidPrime/ScriptObjects/Carve801FD998.c: PASS, Object(Matching) in configure.py
goal_check: PASS carve-801fd998
```

Measured directly as well:

- `./tools/flip_test.sh MetroidPrime/ScriptObjects/Carve801FD998.c` - `PASS -> kept as Matching`,
  `kept: 1 / 1  failed: 0  skipped: 0`.
- `./tools/unit_fit.sh MetroidPrime/ScriptObjects/Carve801FD998.c` - `.text claimed 132 ours 132
  retail 132 fits`, `no extra functions: our object defines only what the retail unit object does`.
- `sha1sum build/G2ME01/main.dol` -> `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`; the build printed
  `87 files OK` and `All: 37.00% fuzzy, 30.44% matched, 13.43% linked (13054 / 28465 functions)`.
- `build/report.json`: `matched_functions` 13053 -> **13054**, `complete_units` 826 -> **827**,
  `total_units` **2155 unchanged** (dtk's `auto_03_801FD998_text` is replaced by our unit, not added
  to), `total_functions` **28465 unchanged**, `fuzzy_match_percent` 36.995853 -> 36.997868. New
  unit `main/MetroidPrime/ScriptObjects/Carve801FD998`: 100.0 fuzzy, 132/132 bytes, **1/1**
  functions. `main/auto_03_801FD998_text` is gone from the new report.
- **Nothing anywhere got worse**: unit by unit against `build/goal/judge/report.base.json`, no unit
  has a lower `matched_functions` or `matched_code` (the only unit that disappeared is the dtk auto
  unit this claim replaces). Neighbours unmoved and 100.0: `Carve801FD924` 116/116 1/1,
  `Carve801FDA1C` 136/136 2/2, `Carve801FDAA4` 68/68 2/2, `Carve801FDAE8` 116/116 1/1,
  `Carve801FD4B0` 124/124 1/1, `Carve801FDB5C` 168/168 3/3.
- `python3 tools/check_symbol_names.py` - `checked 579 units; 0 declared names are missing from
  their object`.
- Port: `build/gate-probe.log` -> `probe: 827 files, 0 failed, 0 errors; link: LINKED (285
  undefined, 0 duplicates)`. 285 is this item's judge baseline
  (`build/goal/judge/undef.base.count` = 285), so the undefined count did not rise.
  `build/gate-link.log` -> `ok: 279 MISSING symbol(s), all accounted for in
  port_link_gap_list.md`, and the committed list holds 279 entries
  (`grep -cE '^- \`' docs/research/port_link_gap_list.md`), i.e. the gap set is unchanged and **no
  `port_link_gap*.md` edit was needed**. `build/gate-dups.log` -> `unique undefined symbols 285`,
  `duplicate definitions 0`.
- `PortLinkStubs.cpp` counts after the retirement, by the file's own terms: `grep -cE 'asm\("'` =
  **199**; `^extern "C" void stub[^ ]*\(\) asm\("` = **190**; `^extern "C" char stub_data_` = **9**.
- `python3 tools/check_decl_order.py --unit MetroidPrime/ScriptObjects/Carve801FD998.c` -
  `0 unit(s) checked` (it matches `.cpp` names only), so it says nothing here; the file holds one
  function, so the descending-order rule cannot be violated, and `flip_test.sh` passing with the
  DOL sha1 unmoved is the check that decides it.
- `gate.sh` rewrote `docs/HANDOFF.md` and `docs/RUNNING_THE_DECOMP.md` as a side effect, as for
  every lane; `git checkout --` on exactly those two left the tree at the five files of this change
  (`git status --porcelain --untracked-files=all`: ` M config/G2ME01/splits.txt`, ` M configure.py`,
  ` M files.cmake`, ` M src/MetroidPrime/PortLinkStubs.cpp`,
  `?? src/MetroidPrime/ScriptObjects/Carve801FD998.c`).

## The finding for the neighbours - a scratch unit, reverted, **not** part of this diff

`docs/goal-notes/carve-801fdb5c.md` lists seven spellings for `fn_801FDB5C` (0x801FDB5C, 0x84,
`mulli 48`, inner walk `fn_801FDBE0`) and records that the only byte-exact one needs compound
literals, which allocate a `.sbss2` and cannot link. **That list does not include this item's
spelling, and this item's spelling matches.** Measured on a scratch unit `Carve801FDB5Cb.c` (this
item's body with `44 -> 48` and `fn_801FDA1C -> fn_801FDBE0`, the callee declared `const void*`),
wired into the three carve files and then **reverted** - the scratch file deleted, the three files
restored to their pre-experiment md5s (`91d64969...`, `0bf383ce...`, `ce23aeee...`) and the tree
rebuilt and re-judged:

```
tools/fast_try.sh  -> main/MetroidPrime/ScriptObjects/Carve801FDB5Cb: 100.00% fuzzy, 100.00% matched code, 1/1 functions
tools/unit_fit.sh  -> .text claimed 132 ours 132 retail 132 fits / no extra functions
tools/flip_test.sh -> PASS -> kept as Matching   kept: 1 / 1  failed: 0  skipped: 0
```

So the lesson is not "this family of destructors is hard", it is the one `Carve801FBC58.c` already
records and this run re-measured: **declare the callee with pointer parameters at the call site,
not by-value struct ones.** With a by-value declaration the caller must build the two iterator
copies (that is the "named struct It locals / 31 instructions" row of the table), with
`const void*` it passes `&first`/`&last` and the bytes fall out. A definition written with the
pointer spelling and dereferences inside the argument list is what `Carve801FBC58.c`'s
`fn_801FBD30`/`fn_801FBD68` pair does, so the trick works inside one translation unit too.

## Supersedes, and what is moot

- Run 1's whole "left alone on purpose" section about the stub file's numerals predates the
  `stub_228`..`stub_231` block; the numerals now measure 190/9/199, not 188/8/196 or 189/8/197.
- Run 2's three-function version of this file (claiming to 0x801FDAA4) is impossible on this tree:
  `Carve801FDA1C.c` owns 0x801FDA1C..0x801FDAA4. Run 1's version (one function plus a new stand-in)
  is superseded the other way: the stand-in already exists and this change deletes it.
- Run 2's "286 is the judge's baseline" is stale here: this item's judge baseline
  (`build/goal/judge/undef.base.count`) is **285**, and this tree measures 285.
- `NEW: carve-801fdb5c` and `NEW: carve-801fd4b0` from the run above: `carve-801fd4b0` is done
  (`Carve801FD4B0.cpp`, `Matching`). `carve-801fdb5c` was never queued and is still open - re-filed
  below with the measured spelling. `NEW: carve-801fda1c` is done (`Carve801FDA1C.c`).
- `carve-801fd52c` **is already in the queue** (`build/goal/queue.json`), so it is not re-filed;
  what this run adds to it is the callee-declaration trick and the fact that claiming its walk
  `fn_801FD5B0` (0x801FD5B0, 0x38, contiguous above it: 0x801FD52C + 0x84 = 0x801FD5B0, and
  0x801FD5B0 + 0x38 = 0x801FD5E8 = the `Carve801FD5E8.c` claim) makes the carve free of new
  stand-ins and retires `stub_231`.

## NEW:

NEW: carve-801fdb5c | match | MetroidPrime/ScriptObjects/Carve801FDB5C | carve `fn_801FDB5C` (`.text` 0x801FDB5C..0x801FDBE0, 0x84 = 132 bytes, 33 instructions, `symbols.txt:8288`, unclaimed) - **measured winnable on this tree, not a spelling guess**: during this item's run the body below was compiled into a scratch unit and passed `fast_try.sh` (100.00% fuzzy / 100.00% matched code, 1/1), `unit_fit.sh` (132/132 fits, no extra functions) and `flip_test.sh` (PASS -> kept as Matching), then reverted. The body is this item's `fn_801FD998` with `44 -> 48` and `fn_801FDA1C -> fn_801FDBE0`, and `fn_801FDBE0` must be declared **at the call site** as `void fn_801FDBE0(const void* first, const void* last)` - that declaration is the whole trick, and none of the seven spellings `docs/goal-notes/carve-801fdb5c.md` lists uses it. Costs **no new stand-in** (`fn_801FDBE0` is defined by the `Matching` `Carve801FDB5C.c`, 0x801FDBE0..0x801FDC88) and **retires `stub_228`**; both boundaries are claimed-unit edges (`Carve801FDAE8.cpp` ends at 0x801FDB5C, `Carve801FDB5C.c` starts at 0x801FDBE0), so it is a clean four-file carve with no gap. The scratch file was named `Carve801FDB5Cb.c` - use any name the 0x801FDBE0 unit does not already have.

NEW: carve-801fd7d4 | match | MetroidPrime/ScriptObjects/Carve801FD7D4 | carve `fn_801FD7D4` (0x801FD7D4, 0x84, `symbols.txt:8276`, unclaimed, `mulli 36`, walk `fn_801FD858`) **and the walk pair above it**, 0x801FD7D4..0x801FD8E0 = 0x84 + 0x38 + 0x50 = 0x10C = 268 bytes, 3 functions, in **two units**: the destructor alone (0x801FD7D4..0x801FD858) because its `bl fn_801FD858` needs the pointer-parameter declaration this item measured, and `fn_801FD858` + `fn_801FD890` (0x801FD858..0x801FD8E0, the by-value iterator shapes `Carve801FDA1C.c` already matches - forwarding, then a walk with `addi r31,r31,0x24`), because one TU cannot hold both declarations of `fn_801FD858`. No gap is spanned: below, `fn_801FD774` (0x801FD774, 0x60, unclaimed) ends exactly at 0x801FD7D4; above, 0x801FD8E0 is where the `Matching` `Carve801FD8E0.c` claim starts. The destructor's walk target `fn_801FD8E0` is defined by that unit, so this costs **no new stand-in** and **retires `stub_230`**. The three bodies are the shapes this item and `Carve801FDA1C.c` measured (`fn_801FD7D4` = this item's `fn_801FD998` with the stride 36), but the three-function compile was **not** run this time - only the `fn_801FDB5C` sibling above was.
