# carve-801ff8a0

**Kind:** `match` **Target:** `MetroidPrime/ScriptObjects/Carve801FF8A0` **Result: PASS.**

`./tools/goal_check.sh build/goal/item.json`, verbatim:

```
goal_check: item carve-801ff8a0 (match) target=MetroidPrime/ScriptObjects/Carve801FF8A0
goal_check: baseline .../wt-mp2-goal-L13/build/goal/judge/report.base.json
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 12610 -> 12614   linked 5971 -> 5975
  ok    check_symbol_names.py
  ok    All:  35.48% fuzzy, 29.30% matched, 13.01% linked (12614 / 28465 functions)
  ok    flip_test MetroidPrime/ScriptObjects/Carve801FF8A0.cpp: PASS, Object(Matching) in configure.py
goal_check: PASS carve-801ff8a0
```

`sha1sum build/G2ME01/main.dol` = `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010` (retail's).
`total_functions` is still 28465 after the `splits.txt` edit.

## What I did

The carve is four files, each entry placed in address order between the two sibling claims
(`Carve801FF720.cpp` ends at 0x801FF8A0, `Carve801FFA20.cpp` starts at 0x801FFA20):

- **`src/MetroidPrime/ScriptObjects/Carve801FF8A0.cpp`** (new, `extern "C"`, 4 functions,
  descending source order: `fn_801FF9B8`, `fn_801FF96C`, `fn_801FF94C`, `fn_801FF8A0`).
- **`configure.py`** - `Object(Matching, "MetroidPrime/ScriptObjects/Carve801FF8A0.cpp"),`
  immediately after `Carve801FF720.cpp` and before `Carve801FFA20.cpp`, one line.
- **`config/G2ME01/splits.txt`** - the unit header plus
  `.text start:0x801FF8A0 end:0x801FFA20`, in the same position. Contiguous, retail's own range,
  spans no unclaimed gap.
- **`files.cmake`** - the source path in the same position.

Plus the two stand-ins the port link needs (see below):

- **`src/MetroidPrime/PortLinkStubs.cpp`** - `stub_189() asm("fn_801FEC64")` and
  `stub_190() asm("fn_801FDAA4")`, immediately after `stub_188`, plus the header counts
  162 -> 164 / 158 -> 160 / 25 -> 27 unmangled `fn_/lbl_` and their history clause.

`docs/HANDOFF.md` and `docs/RUNNING_THE_DECOMP.md` show as modified in `git status` but I did not
touch them: `tools/check_docs_claims.py` rewrites the derived state block from the tree on every
run (the diff is exactly 12610 -> 12614 matched, 5971 -> 5975 linked, 11027 -> 11031 DOL units).
`goal_check.sh` reported `no judge-owned path touched`.

## What the four functions are, measured

`.text 0x801FF8A0..0x801FFA20`, 0x180 = 384 bytes, 96 instructions, 4 functions
(`config/G2ME01/symbols.txt:8350-8353` agrees with `objdump` here):

```
fn_801FF8A0  0x801FF8A0  0xAC  43 instructions  the block's reserve
fn_801FF94C  0x801FF94C  0x20   8 instructions  forwarder to fn_801FF96C
fn_801FF96C  0x801FF96C  0x4C  19 instructions  destroy_impl(begin, end)
fn_801FF9B8  0x801FF9B8  0x68  26 instructions  uninitialized_copy(begin, end, dst)
```

Read out of `build/G2ME01/asm/auto_03_801FF8A0_text.s` before the claim existed (that file was
`.text 0x801FF8A0..0x801FFA20 | size: 0x180` with exactly these four `.fn` blocks); still readable
with `build/binutils/powerpc-eabi-objdump -d --start-address=0x801FF8A0 --stop-address=0x801FFA20
build/G2ME01/main.elf`.

This is the same template the neighbouring carves hold, at element stride **0x2C = 44**. Compared
against the immediately preceding claimed range (`Carve801FF720.cpp`, stride 0x24) **at relative
offsets**, measured this run with a script over both `objdump` outputs:

```
96 instructions each, 384 bytes each, every instruction at the same relative offset
(so both ranges have the same four function starts: +0x000, +0x0AC, +0x0CC, +0x118)
10 differing words:
  +0x02c  mulli r3,r30,36  -> mulli r3,r30,44     the allocation's size
  +0x044  mulli r0,r0,36   -> mulli r0,r0,44      the loop bound, first copy call
  +0x074  mulli r0,r0,36   -> mulli r0,r0,44      the loop bound, destroy call
  +0x0f0  bl     801fd8e0  -> bl     801fdaa4     the element destructor
  +0x0f4  addi   r31,r31,36 -> addi r31,r31,44
  +0x148  bl     801fea98  -> bl     801fec64     the element copy
  +0x14c  addi   r31,r31,36 -> addi r31,r31,44
  +0x150  addi   r30,r30,36 -> addi r30,r30,44
  +0x030  bl     802fdab8  -> bl     802fdab8     allocate: same target, displacement only
  +0x084  bl     802ce388  -> bl     802ce388     Free:     same target, displacement only
```

So the body is `Carve801FF720.cpp`'s with the stride 36 -> 44 and the two element callees swapped;
nothing else differs, and the two `bl`s that look different encode the same absolute address (this
range sits 0x180 bytes higher). The internal call edges line up one for one:
`fn_801FF8A0` -> `fn_801FF9B8` at 0x801FF908 and -> `fn_801FF94C` at 0x801FF91C,
`fn_801FF94C` -> `fn_801FF96C` at 0x801FF958.

The two callees are the MWCC element-copy / `destroy<T>(T*)` forwarder shape, 0x20 bytes each
(`symbols.txt:8316` / `:8285`), each one `bl` to the body it forwards to (`fn_801FEC84` /
`fn_801FDAC4`). The `.cpp` and the by-value `SStateIter` argument struct are inherited from the
measured sibling `Carve801FF5A0.cpp`, not chosen - that file records why plain C with a one-word
struct is not byte-exact for this template.

## Verification

- `./tools/decomp_build.sh MetroidPrime/ScriptObjects/Carve801FF8A0` ->
  `main/MetroidPrime/ScriptObjects/Carve801FF8A0: 100.00% fuzzy, 100.00% matched (4 / 4 functions)`
  and `All: ... (12614 / 28465 functions)`.
- `build/report.json` for the unit: `matched_functions 4/4`, `total_code`/`matched_code` 384/384,
  `matched_code_percent 100.0`, `complete_units 1`, `metadata.complete true`,
  `source_path src/MetroidPrime/ScriptObjects/Carve801FF8A0.cpp`.
- `./tools/carve_diff.sh 0x801FF8A0 0x180 build/G2ME01/src/MetroidPrime/ScriptObjects/Carve801FF8A0.o`
  -> `retail: 96 instructions, 384 bytes / ours: 96 instructions, 384 bytes / differing
  instructions: 7`, and all 7 are `bl` relocations resolved by the linker
  (`allocate__Q24rstl17rmemory_allocatorFi`, `fn_801FF9B8`, `fn_801FF94C`, `Free__7CMemoryFPCv`,
  `fn_801FF96C`, `fn_801FDAA4`, `fn_801FEC64`). No opcode, offset or frame displacement differs.
- `build/binutils/powerpc-eabi-nm` on the object: `T fn_801FF8A0` / `_801FF94C` / `_801FF96C` /
  `_801FF9B8` at 0/0xac/0xcc/0x118 and four `U` externs - nothing else defined, all unmangled.
- `python3 tools/check_decl_order.py --unit MetroidPrime/ScriptObjects/Carve801FF8A0` ->
  `ok: 1 unit(s) checked, none emits its functions out of retail order`.
- `./tools/unit_fit.sh MetroidPrime/ScriptObjects/Carve801FF8A0.cpp` -> `.text claimed 384 ours 384
  retail 384 fits`, `no extra functions`.
- `./tools/flip_test.sh MetroidPrime/ScriptObjects/Carve801FF8A0.cpp` -> **PASS, kept as Matching**
  (the full check, both directions).
- `grep -rn "fn_801FF8A0\|fn_801FF94C\|fn_801FF96C\|fn_801FF9B8" src/ include/` finds the four
  names only in the new file (the only other hits are comment prose in `PortLinkStubs.cpp`), so
  there is no duplicate definition.

## The two stubs, and why they are stubs rather than carves

Neither element callee is claimed anywhere in the tree (`grep -rn "fn_801FEC64\|fn_801FDAA4"
src/ include/` is empty before the stub block), and the carve's bytes *are* those `bl`s, so the
calls cannot be dropped without losing the match. Measured both ways in this tree:

- without the two blocks: `python3 tools/link_gap.py --rebuild` prints `288 MISSING` and
  `gap grew: fn_801FDAA4 is not in port_link_gap_list.md` /
  `gap grew: fn_801FEC64 is not in port_link_gap_list.md`;
- with them: the same command prints `286 MISSING` and
  `ok: 286 MISSING symbol(s), all accounted for in port_link_gap_list.md`.

Both are forwarders (`stwu/mflr/stw` / `bl fn_801FEC84` or `fn_801FDAC4` / `mtlr/addi/blr`), so
carving either would only move the gap one function along - the trade `stub_178` and its siblings
already record. Neither name is added to `docs/research/port_link_gap_list.md`: the stub defines
it, so it never reaches the gap list.

For the DOL nothing is needed, and the stub cannot reach it: `PortLinkStubs.cpp` is not in
`configure.py`. dtk's own auto object for the unclaimed range holding each function defines it in
the DOL (this run `auto_03_801FDC88_text.o` for `fn_801FEC64`, `auto_03_801FA3CC_text.o` for
`fn_801FDAA4`).

## Lessons (not filed as `NEW:`)

- **The item's stub numbers were stale, again.** The `NEW:` that queued this item said "one
  stub_186 plus one stub_187"; on this tip `stub_188` is `fn_801FE7E8`, so this run's blocks are
  **`stub_189`** and **`stub_190`** (read out of the file, not from the note). `stub_186`/`_187` are
  `fn_8000408C`/`fn_801B9C68`. The item also expected *one* stub; there are **two** unclaimed
  callees, because this round's element copy and destructor are both unclaimed (the sibling round
  had one claimed destructor, `fn_801FDC68`, so it needed only one).
- The relative-offset comparison against the immediately preceding claimed range is again the
  check that decides: same offsets, same function starts, and every differing word is a stride
  constant or a `bl` - 10 here, the same count and shape the `Carve801FFA20` run measured for
  0x24 -> 0x30. Comparing against the first carve of the family would have been a weaker check.
- `PortLinkStubs.cpp`'s header has **two** `Breakdown:` paragraphs; the second (the one still
  reading `22 unmangled fn_/lbl_`) is a stale duplicate that no prior run fixed. I updated the
  maintained first paragraph (25 -> 27, +2 for the two `fn_` stubs, the same increment convention
  the previous runs used) and left the duplicate alone as pre-existing, unrelated churn. Nothing
  derives these numbers (`gate.sh` cannot catch a miss), so the next run should not trust either
  paragraph without counting the file.
- **That header series is one behind a direct count, measured.** Counting the `asm` labels in this
  file gives 164 = 160 functions + 4 data (which matches), and 28 of the 164 start with `fn_`/`lbl_`
  (19 `fn_` + 9 `lbl_`; 26 before this item's two). The maintained paragraph said 25 before this
  change and now says 27, and its classes must sum to 164, so I kept the increment convention rather
  than rewrite an unrelated bucket (`86 REL loader` / `46 game method`) I cannot re-derive - the
  original generated header read `87 REL loader, 72 game method, 18 unmangled fn_/lbl_, 4
  vtable/typeinfo` for 181, and the generator's `by_class` is its input tsv, which hand-added stubs
  never enter. Recorded, not fixed: a generator run should collapse the duplicate paragraph and
  re-derive all five numbers.
- The next round at 0x801FFBA0 is already recorded by the `carve-801ffa20` run as **not** a
  straight copy (its `symbols.txt` sizes give a 0x360-byte run whose `bl`s leave the range), so no
  `NEW:` is filed for it here.
- Minor, measured, not fixed: the sibling comments' "this address is 0x6B50 bytes into
  `CUnknown90.cpp`" (`Carve801FF5A0.cpp` for 0x801FF5A0, `Carve801FFA20.cpp` for 0x801FFA20) does
  not match the arithmetic - 0x801FF5A0 - 0x801F9050 = 0x6550, 0x801FFA20 - 0x801F9050 = 0x69D0.
  Mine reads 0x6850, which is what `splits.txt`'s `start:0x801F9050` gives. Left in place: it is
  prose in files this item does not own.

No `NEW:` items: this item is complete, and nothing new was found that can raise a count.
