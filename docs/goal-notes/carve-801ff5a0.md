# carve-801ff5a0 — `MetroidPrime/ScriptObjects/Carve801FF5A0`, `Matching`, 4/4 functions

**Result: the unit is `Matching` and `flip_test.sh` PASSes. `tools/goal_check.sh build/goal/item.json` PASSes.**
`matched` 12532 -> 12536, `linked` 5903 -> 5907, `total_functions` still 28465,
`build/G2ME01/main.dol` = `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`, all 86 RELs unchanged,
port link gap still 291 (see "the port link" below).

## What I did

Carved `.text 0x801FF5A0..0x801FF720` (0x180 = 384 bytes, 4 functions) out of dtk's
`auto_03_801FF4C4_text` as its own unit. Four files, all in address order, source descending:

- `src/MetroidPrime/ScriptObjects/Carve801FF5A0.cpp` (new)
- `config/G2ME01/splits.txt` — `Carve801FF5A0.cpp: .text start:0x801FF5A0 end:0x801FF720`
- `configure.py:738` — `Object(Matching, "MetroidPrime/ScriptObjects/Carve801FF5A0.cpp")`, one line
- `files.cmake:560` — `src/MetroidPrime/ScriptObjects/Carve801FF5A0.cpp`
- `src/MetroidPrime/PortLinkStubs.cpp` — three stubs, required by the gate (below)

| function | addr | size | twin it is byte-for-byte |
| --- | --- | --- | --- |
| `fn_801FF5A0` | 0x801FF5A0 | 0xAC | `fn_801466F4` (CGameState.cpp, Matching) |
| `fn_801FF64C` | 0x801FF64C | 0x20 | `fn_801467A0` / `__sys_free` (main.cpp, Matching) |
| `fn_801FF66C` | 0x801FF66C | 0x4C | `fn_801467C0` (CGameState.cpp, Matching) |
| `fn_801FF6B8` | 0x801FF6B8 | 0x68 | `fn_8014680C` (CGameState.cpp, Matching) |

All four are the same `rstl`-shaped block's `reserve`, `destroy` forwarder, `destroy_impl` and
`uninitialized_copy` for a 0x24 = 36-byte element. The same four repeat over this range at
0x801FF720, 0x801FF8A0, 0x801FFA20, with the triplet alone at 0x801FF7CC/0x801FF7EC/0x801FF838,
0x801FF94C/0x801FF96C/0x801FF9B8 and 0x801FFACC/0x801FFAEC/0x801FFB38 — one template per
script-object type, which is what fixes the shapes. Only the first round is claimed.

## This unit is C++, not C, and that was measured

`docs/RUNNING_THE_DECOMP.md`'s carve recipe says plain C so the `fn_` names do not mangle. I wrote
it in C first and it is **not** byte-exact. Against dtk's own bytes for the range
(`build/G2ME01/asm/auto_03_801FF4C4_text.s`, `.text:0xDC`..`.text:0x1F4`), word by word:

- `fn_801FF5A0`: **27 of 43 words differ.** mwcceppc's C mode common-subexpressions the two
  `x0c` reads, so it never emits the `lwz r0,0xc(r29)` reload at 0x801FF5F8 and produces **42**
  instructions to retail's 43; and the two by-value argument slots land interleaved (begin at
  0xc/0x14, end at 0x8/0x10) instead of in two contiguous 8-byte slots (end 0x8/0xc, begin
  0x10/0x14).
- `fn_801FF66C`: **6 of 19 words differ**, all one thing — C mode puts the walked cursor in r30
  and the bound in r31 where retail has them the other way round.
- `fn_801FF64C` and `fn_801FF6B8` **are** byte-exact in C, so it is specifically C-mode register
  allocation and CSE, not the struct-by-value ABI, that is the difference.

Spellings tried in C and measured, none of which changed either function: assigning `end` before
`begin`; the `for (; p != end; p += 36)` spelling of the destroy loop instead of `while`; both
together; and casting `self` on the second read to try to defeat the CSE. Scores were identical
(80 differing words over the range) for all four.

Rewritten as the twins' C++ — same `SStateIter` with its converting constructor, same
`static_cast` spellings — **all four are byte-exact**, and the object's `.text` is 0x180 with the
functions at offsets 0x0 / 0xAC / 0xCC / 0x118, i.e. the same relative offsets as retail.
`powerpc-eabi-nm` on the object shows exactly four `T fn_801FF<addr>` and four `U`, nothing else,
so the symbols stay unmangled, which is the thing the plain-C rule protects. Thirteen other
`Carve*.cpp` units in this tree are `extern "C"` for the same reason. The unit's file comment
records all of this.

Two smaller things worth knowing:

- **mwcceppc's C front-end rejects a declaration that does not come first in a block**
  (`char* const buffer = ...;` after an `if` is `expression syntax error`, and it cascades into
  every line after it). C++ does not have that restriction, so the C++ spelling needs no
  hoisting.
- **The `.c` route is not dead in general** — `Carve801FDB5C.c` in this same directory carries
  struct-by-value parameters in C and is `Matching`. It is this function pair that needs C++.

## The port link needed three stubs

The carve's bytes *are* its `bl`s, so `allocate__Q24rstl17rmemory_allocatorFi` (0x801FF5D0),
`fn_801FEE40` (0x801FF6E8) and `fn_801FD638` (0x801FF690) cannot be dropped. The DOL gets them
from dtk's `auto_*` objects; the port link does not, and `tools/gate.sh`'s `link gap` step failed
on it: **294 undefined against a baseline of 291**. `stub_179`/`stub_180`/`stub_181` in
`PortLinkStubs.cpp` take it back to 291. Same trade, same shape of reasoning, as the existing
`stub_178` for `fn_801FDC88`, which `Carve801FDB5C.c` put in the gap in the same way — empty
bodies, no claim that any of the three is decompiled, and carving them instead only moves the gap
one function along because each is itself a one-`bl` forwarder. The header counts were updated
from the measured 151/148 to 154/151; the "Breakdown:" line was already not summing to the total
before this change and I did not try to re-derive it, only to add the one new category.

## How it was verified

```
./tools/decomp_build.sh
All:  35.35% fuzzy, 29.19% matched, 12.93% linked (12536 / 28465 functions)
sha1sum build/G2ME01/main.dol -> 6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
./tools/flip_test.sh MetroidPrime/ScriptObjects/Carve801FF5A0.cpp
  PASS  -> kept as Matching        kept: 1 / 1   failed: 0   skipped: 0
./tools/unit_fit.sh MetroidPrime/ScriptObjects/Carve801FF5A0.cpp
  .text  claimed 384  ours 384  retail 384  fits
  no extra functions: our object defines only what the retail unit object does
python3 tools/check_symbol_names.py      526 units, 0 missing names
python3 tools/check_decl_order.py --all 993 checked, 28 permuted, all accounted for
./tools/probe_sources.sh                 766 files, 0 failed
./tools/goal_check.sh build/goal/item.json
  ok  gate.sh (DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok  counts: matched 12532 -> 12536   linked 5903 -> 5907
  ok  flip_test MetroidPrime/ScriptObjects/Carve801FF5A0.cpp: PASS
goal_check: PASS carve-801ff5a0
```

`docs/HANDOFF.md` and `docs/RUNNING_THE_DECOMP.md` were rewritten by `gate.sh`'s own derived-count
writer while it ran; I reverted them, since the driver discards edits to those two anyway and
`gate.sh` reproduces them from the tree.

## A warning about `tools/carve_diff.sh`

**`tools/carve_diff.sh` compares your object against `build/G2ME01/main.elf`, which is *your own
link*, not retail.** On a tree whose DOL does not reproduce retail it therefore reports big
differences that have nothing to do with your bytes, and on a carve whose bytes *are* right it
cannot fail. I hit exactly that: it reported 8 differing instructions and "NOT byte-exact" for a
compilation that is byte-identical to retail. `orig/G2ME01/main.dol` does not exist in a lane
worktree either (`orig/G2ME01` symlinks to the main tree, which has only `files/` and `sys/`), so
there is no retail DOL to point it at.

What I used instead, and would use again: dtk's own pre-claim disassembly
`build/G2ME01/asm/auto_03_801FF4C4_text.s`, whose `/* addr vaddr XX XX XX XX */` comments carry
retail's bytes verbatim. **`dtk dol split` overwrites that file on the next split** — after my
claim landed, it had been rewritten to cover only 0x801FF4C4..0x801FF5A0 — so copy it aside
before the first build of the session. (I had already read the range out of it, and
`../wt-mp2-goal-L9/build/G2ME01/asm/auto_03_801FF4C4_text.s`, written 11:14 and still pre-claim,
gave the same bytes.) Compare word by word, skipping the branch words — their displacement lives
in the relocation, and mwcceppc emits `0x00000001` there until the link — then check the
`powerpc-eabi-objdump -r` relocations name the right symbols.

`NEW: tool-carve-diff | tooling | tools/carve_diff.sh | it compares the candidate object against build/G2ME01/main.elf, which is the tree's own link rather than retail, so it cannot fail on a correct carve and reports false differences otherwise - it should take the retail bytes from dtk's pre-claim auto_*.s instead, and say so.`
