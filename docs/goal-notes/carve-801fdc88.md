# carve-801fdc88 — `match`, `MetroidPrime/ScriptObjects/Carve801FDC88`

**Result: the item is done. `flip_test` PASS, kept as `Matching`, and
`./tools/goal_check.sh build/goal/item.json` printed `goal_check: PASS carve-801fdc88`.**

## What I did

Carved the one function the item names out of dtk's `main/auto_03_801FDC88_text`, as a new
`Matching` unit `src/MetroidPrime/ScriptObjects/Carve801FDC88.c`, claiming exactly
`.text 0x801FDC88..0x801FDCAC` (0x24 = 36 bytes, 1 function, `fn_801FDC88`). The body is
`void fn_801FDC88(void* self) { fn_801FDCAC(self, -1); }`, plain C so the `fn_` symbol does not
mangle, with `fn_801FDCAC` declared `extern` and never defined here.

Four files, all in one change, each entry in address order:

| file | change |
| --- | --- |
| `config/G2ME01/splits.txt:1509-1510` | new entry, `.text start:0x801FDC88 end:0x801FDCAC`, between `Carve801FDB5C.c` (ends 0x801FDC88) and `Carve801FEA98.c` |
| `configure.py:853` | `Object(Matching, "MetroidPrime/ScriptObjects/Carve801FDC88.c"),` on one line, next to the preceding range |
| `files.cmake:870` | `src/MetroidPrime/ScriptObjects/Carve801FDC88.c` |
| `src/MetroidPrime/ScriptObjects/Carve801FDC88.c` | new, the claim itself |

Plus `src/MetroidPrime/PortLinkStubs.cpp:858-884`: `stub_178` (`fn_801FDC88`) **deleted** and
replaced by `stub_carve801fdc88_0` (`fn_801FDCAC`), and four live comment references to `stub_178`
repointed at the new name so they stay true.

## What I measured

**The twin is byte-for-byte, 9 of 9 words, the `bl` word included.** Both functions disassembled
from `build/G2ME01/main.elf` *before* the claim existed (`objdump -d --start-address=0x801FDC88
--stop-address=0x801FDCAC`, and the same at `0x80004458`):

```
fn_801FDC88                 fn_80004458  (MetroidPrime/Carve80004438.c, Matching 2/2)
94 21 ff f0 stwu r1,-16(r1) 94 21 ff f0 stwu r1,-16(r1)
7c 08 02 a6 mflr r0         7c 08 02 a6 mflr r0
38 80 ff ff li r4,-1        38 80 ff ff li r4,-1
90 01 00 14 stw r0,20(r1)   90 01 00 14 stw r0,20(r1)
48 00 00 15 bl fn_801FDCAC  48 00 00 15 bl __dt__11CWorldStateFv
80 01 00 14 lwz r0,20(r1)   80 01 00 14 lwz r0,20(r1)
7c 08 03 a6 mtlr r0         7c 08 03 a6 mtlr r0
38 21 00 10 addi r1,r1,16   38 21 00 10 addi r1,r1,16
4e 80 00 20 blr             4e 80 00 20 blr
```

Zero differing words, because in each case the callee sits 0x14 bytes past its own `bl`
(0x801FDC98 + 0x14 = 0x801FDCAC, 0x80004468 + 0x14 = 0x8000447C) — the same coincidence
`Carve801FDAA4.c` records for its own pair. So the identification does not rest on the twin alone:
**`fn_801FDC68`** (0x801FDC68, 0x20, in the already-`Matching` `Carve801FDB5C.c`, 3/3) is a frame
and one unconditional `bl fn_801FDC88` and nothing else, which is `rstl::destroy`
(`include/rstl/construct.hpp:92-95`), and **`fn_801FDC18`** walks elements with
`addi r31,r31,0x30`, which fixes the element at 0x30 bytes. That makes this function
`destroy_impl` — the `li r4,-1` being MWCC's "do not free me afterwards" flag.

The bytes are retail's, not ours: read from the disc at file offset `0x640 + (0x801FDC88 -
0x80003840)` -> `9421fff07c0802a63880ffff9001001448000015800100147c0803a6382100104e800020`,
identical to what the pre-change `main.elf` held.

## The port-link trade, and why it was forced

`fn_801FDCAC` (0x801FDCAC, `symbols.txt:8293`, 0x94 = 148 bytes) is above the claim and unclaimed,
so it stays dtk's in the DOL and needs a stand-in in the port link — the same trade `stub_178`
made. `stub_178`'s own comment predicted exactly this outcome ("carving 0x801FDC88..0x801FDCAC
only moves the same gap one function along: that body calls fn_801FDCAC, and the chain
continues"). **Two rules in the brief collide here and the stub trade is the only resolution:**
(a) `fn_801FDC88` must not be defined twice, so `stub_178` had to go, and `tools/gate.sh`'s
`port link dups` step is what would have failed if it had stayed; (b) `tools/link_check.sh
--strict` fails on **any growth** of the linker's undefined count against the recorded baseline
of 287, and the carve's `bl fn_801FDCAC` adds exactly one new name unless something defines it.
One stub out, one stub in, and the numbers below confirm the trade is net-neutral.

## Verification (all measured this run)

- `./tools/goal_check.sh build/goal/item.json` -> **`goal_check: PASS carve-801fdc88`**, every
  check `ok`: gate.sh, counts, `check_symbol_names.py`, the `All:` line, `flip_test ... PASS`.
- `./tools/flip_test.sh MetroidPrime/ScriptObjects/Carve801FDC88.c` -> `PASS -> kept as Matching`;
  `kept: 1 / 1   failed: 0   skipped: 0`. One `Object(...)` on one line, no multi-line form.
- `sha1sum build/G2ME01/main.dol` -> `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`; the gate's
  `hashes vs config.yml` step (DOL + all 86 RELs) `ok`.
- `matched 13629 -> 13630`, `linked 6677 -> 6678`; `total_functions` still **28465** after the
  `splits.txt` edit; the per-function diff says `SPLIT main/auto_03_801FDC88_text: 19 function(s)
  accounted for across 2 new unit(s) ... exact count match - a split, not a loss`, so no function
  anywhere got worse.
- `./tools/unit_fit.sh` -> `.text claimed 36 ours 36 retail 36 fits`, "no extra functions".
- `tools/carve_diff.sh 0x801FDC88 0x24 build/G2ME01/obj/MetroidPrime/ScriptObjects/Carve801FDC88.o`
  reports 9 vs 9 instructions, 36 vs 36 bytes, with one "differing" line — `bl 10 <fn_801FDC88+0x10>`
  against `bl 801fdcac`. That is the **unrelocated** displacement in the standalone `.o`, not a
  byte difference; the linked DOL's sha1 above and the unit's own generated listing
  (`build/G2ME01/asm/MetroidPrime/ScriptObjects/Carve801FDC88.s`, `48 00 00 15 bl fn_801FDCAC`)
  are the real evidence. **Read `carve_diff.sh`'s object as the linked one or ignore its verdict
  on `bl` targets.**
- `python3 tools/check_decl_order.py` -> `1299 unit(s) checked, 37 permuted, all 37 accounted for`
  (unchanged; one function, so descending order is trivially satisfied).
- `python3 tools/check_files_cmake.py` -> "every configured DOL object is either in files.cmake or
  excluded with a reason".
- Port: `probe: 1030 files, 0 failed, 0 errors; link: LINKED (287 undefined, 0 duplicates)` —
  the undefined count is **exactly the recorded 287**, and `tools/link_gap.py --rebuild` reports
  `279 MISSING symbol(s), all accounted for in port_link_gap_list.md`, so no gap-list edit is owed.
- `check_symbol_names.py` -> `611 units; 0 declared names are missing from their object`.
- `docs claims` gate step `ok`. I did **not** edit `docs/HANDOFF.md`,
  `docs/RUNNING_THE_DECOMP.md` or `docs/LANE_BRIEFING.md` (the driver discards those anyway).

## Notes for whoever reads this next

- **The chain does continue.** `fn_801FDCAC` is a 0x94-byte deleting destructor: vtable
  `lbl_803B7BD8` into +0x0, `__dt__6CTokenFv` at +0x24 behind `lbz 0x2c(r30)`, `fn_801FD6F0` at
  +0x14 with the same `li r4,-1`, a `rstl::basic_string` at +0x4 through
  `internal_dereference__Q24rstl66basic_string<...>`, and `Free__7CMemoryFPCv(self)` only when the
  caller's flag is positive (`extsh. r0,r31 / ble` — which is why the flag is `int`, not `short`).
  Matching it needs that `basic_string` release, `fn_801FD6F0` and the `.data` vtable as well, so
  it is **not** a same-shape twin and is not in this item. If it is ever carved, delete
  `stub_carve801fdc88_0` in the same change or `port link dups` fails.
- **The `bl`-word coincidence is a family fact worth reusing.** `destroy_impl` forwarders whose
  callee is exactly 0x14 bytes past the `bl` emit an identical `48000015`, so "the twin differs in
  the `bl` word" is *not* a reason to doubt the twin. `Carve801FDAA4.c` says this for its pair;
  this is a second instance (`0x801FDAB0`/`0x801FDAC4` and `0x801FDC98`/`0x801FDCAC`).
- **`PortLinkStubs.cpp`'s own header count needs no edit here**: one stub out, one in, and both
  are `asm("fn_...")`, so the file's function total and its "40 unmangled `fn_`/`lbl_`" breakdown
  are unchanged by this change. I did update the two lines that read "after `fn_801FDC88` was
  added by hand" — they are history and still true as history.
- **Found, not fixed, and deliberately out of scope: that same header breakdown is already wrong
  before my change.** `src/MetroidPrime/PortLinkStubs.cpp:142-145` claims "200 total, 40 unmangled
  is `grep -cE 'asm\("(fn_|lbl_)'` over the file, 9 is its `stub_data_*` count". Measured on this
  tree: `grep -cE 'asm\("(fn_|lbl_)'` is **51** and `grep -c stub_data_` is **33**. So the file's
  self-description drifted and nobody re-derives it; nothing checks it (`check_docs_claims.py`
  reads `docs/HANDOFF.md`, `docs/RUNNING_THE_DECOMP.md`, `docs/LANE_BRIEFING.md` and
  `docs/research/`, not this header), and the gate stayed green with it wrong. It is an unrelated
  fix, so I left it alone rather than widen this diff — it is recorded here instead.
- No `NEW:` lines filed: nothing here is a blocker. The item's own claim, its target function and
  its unit all landed.